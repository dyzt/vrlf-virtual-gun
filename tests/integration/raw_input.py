"""Raw Input mouse watcher: device list plus live WM_INPUT events."""
import ctypes
import ctypes.wintypes as W
import time

_u = ctypes.WinDLL("user32", use_last_error=True)
LRESULT = ctypes.c_ssize_t
WNDPROC = ctypes.WINFUNCTYPE(LRESULT, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
_u.DefWindowProcW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
_u.DefWindowProcW.restype = LRESULT
_u.CreateWindowExW.restype = W.HWND
VHF_MARK = "HID_DEVICE_SYSTEM_VHF"
# Windows reports a 0..32767 absolute device as 0..65535.
RAW_PER_DEVICE_UNIT = 65535 / 32767

# usFlags / usButtonFlags bits
MOUSE_MOVE_ABSOLUTE = 0x0001
LEFT_DOWN, LEFT_UP, RIGHT_DOWN = 0x0001, 0x0002, 0x0004


class RAWINPUTDEVICE(ctypes.Structure):
    _fields_ = [("usUsagePage", W.USHORT), ("usUsage", W.USHORT), ("dwFlags", W.DWORD), ("hwndTarget", W.HWND)]


class RAWINPUTDEVICELIST(ctypes.Structure):
    _fields_ = [("hDevice", W.HANDLE), ("dwType", W.DWORD)]


class RAWINPUTHEADER(ctypes.Structure):
    _fields_ = [("dwType", W.DWORD), ("dwSize", W.DWORD), ("hDevice", W.HANDLE), ("wParam", W.WPARAM)]


class RAWMOUSE(ctypes.Structure):
    _fields_ = [("usFlags", W.USHORT), ("_pad", W.USHORT), ("usButtonFlags", W.USHORT), ("usButtonData", W.USHORT),
                ("ulRawButtons", W.ULONG), ("lLastX", W.LONG), ("lLastY", W.LONG), ("ulExtraInformation", W.ULONG)]


class RAWINPUT(ctypes.Structure):
    _fields_ = [("header", RAWINPUTHEADER), ("mouse", RAWMOUSE)]


class WNDCLASSW(ctypes.Structure):
    _fields_ = [("style", W.UINT), ("lpfnWndProc", WNDPROC), ("cbClsExtra", ctypes.c_int), ("cbWndExtra", ctypes.c_int),
                ("hInstance", W.HINSTANCE), ("hIcon", W.HICON), ("hCursor", W.HANDLE), ("hbrBackground", W.HBRUSH),
                ("lpszMenuName", W.LPCWSTR), ("lpszClassName", W.LPCWSTR)]


def device_name(handle):
    size = W.UINT(0)
    _u.GetRawInputDeviceInfoW(handle, 0x20000007, None, ctypes.byref(size))
    buf = ctypes.create_unicode_buffer(size.value + 1)
    _u.GetRawInputDeviceInfoW(handle, 0x20000007, buf, ctypes.byref(size))
    return buf.value


def vhf_mice():
    """Names of every VHF-backed Raw Input mouse present now."""
    count = W.UINT(0)
    _u.GetRawInputDeviceList(None, ctypes.byref(count), ctypes.sizeof(RAWINPUTDEVICELIST))
    arr = (RAWINPUTDEVICELIST * count.value)()
    _u.GetRawInputDeviceList(arr, ctypes.byref(count), ctypes.sizeof(RAWINPUTDEVICELIST))
    return [device_name(d.hDevice) for d in arr if d.dwType == 0 and VHF_MARK in device_name(d.hDevice)]


class Watcher:
    """Collects (handle, name, usFlags, usButtonFlags, x, y) for every mouse event."""

    def __init__(self):
        self.events = []
        self._proc = WNDPROC(self._wndproc)
        wc = WNDCLASSW()
        wc.lpfnWndProc = self._proc
        wc.lpszClassName = "vgun_raw_watch"
        _u.RegisterClassW(ctypes.byref(wc))
        self.hwnd = _u.CreateWindowExW(0, "vgun_raw_watch", "vgun", 0, 0, 0, 0, 0, W.HWND(-3), None, None, None)
        rid = RAWINPUTDEVICE(1, 2, 0x100, self.hwnd)  # RIDEV_INPUTSINK
        if not _u.RegisterRawInputDevices(ctypes.byref(rid), 1, ctypes.sizeof(rid)):
            raise OSError(ctypes.get_last_error(), "RegisterRawInputDevices")

    def _wndproc(self, hwnd, msg, wparam, lparam):
        if msg == 0x00FF:
            size = W.UINT(0)
            _u.GetRawInputData(W.HANDLE(lparam), 0x10000003, None, ctypes.byref(size), ctypes.sizeof(RAWINPUTHEADER))
            buf = ctypes.create_string_buffer(max(size.value, ctypes.sizeof(RAWINPUT)))
            _u.GetRawInputData(W.HANDLE(lparam), 0x10000003, buf, ctypes.byref(size), ctypes.sizeof(RAWINPUTHEADER))
            ri = RAWINPUT.from_buffer_copy(buf.raw[: ctypes.sizeof(RAWINPUT)])
            h = ri.header.hDevice or 0
            name = device_name(ri.header.hDevice) if h else ""
            self.events.append((h, name, ri.mouse.usFlags, ri.mouse.usButtonFlags, ri.mouse.lLastX, ri.mouse.lLastY))
        return _u.DefWindowProcW(hwnd, msg, wparam, lparam)

    def ours(self):
        return [e for e in self.events if VHF_MARK in e[1]]

    def pump(self, seconds):
        msg = W.MSG()
        end = time.time() + seconds
        while time.time() < end:
            while _u.PeekMessageW(ctypes.byref(msg), None, 0, 0, 1):
                _u.TranslateMessage(ctypes.byref(msg))
                _u.DispatchMessageW(ctypes.byref(msg))
            time.sleep(0.005)
