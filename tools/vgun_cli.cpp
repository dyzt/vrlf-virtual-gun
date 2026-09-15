// Stdin-driven virtual gun driver for the integration tests. Same library and
// same calls VRLF makes, without VRLF.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "WinUHid.h"
#include "shared/gun_device.h"
#include "shared/lane_devnode.h"

namespace {

PWINUHID_DEVICE g_devices[VGUN_MAX_LANES] = {};

DWORD last_error_or(DWORD fallback) {
    const DWORD e = GetLastError();
    return e != 0 ? e : fallback;
}

DWORD create(unsigned lane) {
    if (lane >= VGUN_MAX_LANES) return ERROR_INVALID_PARAMETER;
    if (g_devices[lane] != nullptr) return ERROR_ALREADY_EXISTS;
    wchar_t instance[16];
    swprintf_s(instance, VGUN_INSTANCE_ID_FORMAT, lane);
    WINUHID_DEVICE_CONFIG config = {};
    config.SupportedEvents = WINUHID_EVENT_NONE;
    config.VendorID = VGUN_VID;
    config.ProductID = VGUN_PID;
    config.VersionNumber = 1;
    config.ReportDescriptorLength = VGUN_REPORT_DESCRIPTOR_BYTES;
    config.ReportDescriptor = VGUN_REPORT_DESCRIPTOR;
    config.InstanceID = instance;
    PWINUHID_DEVICE device = WinUHidCreateDevice(&config);
    if (device == nullptr) return last_error_or(ERROR_GEN_FAILURE);
    if (!WinUHidStartDevice(device, nullptr, nullptr)) {
        const DWORD err = last_error_or(ERROR_GEN_FAILURE);
        WinUHidDestroyDevice(device);
        return err;
    }
    g_devices[lane] = device;
    return 0;
}

DWORD submit(unsigned lane, int x, int y, int buttons) {
    if (lane >= VGUN_MAX_LANES || g_devices[lane] == nullptr) return ERROR_INVALID_PARAMETER;
    vgun::GunReport report = vgun::make_report(x, y, buttons);
    if (!WinUHidSubmitInputReport(g_devices[lane], &report, sizeof(report))) {
        return last_error_or(ERROR_GEN_FAILURE);
    }
    return 0;
}

void destroy(unsigned lane) {
    if (lane >= VGUN_MAX_LANES || g_devices[lane] == nullptr) return;
    WinUHidStopDevice(g_devices[lane]);
    WinUHidDestroyDevice(g_devices[lane]);
    g_devices[lane] = nullptr;
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    char line[128];
    while (std::fgets(line, sizeof(line), stdin) != nullptr) {
        unsigned lane = 0;
        int x = 0, y = 0, buttons = 0;
        if (std::strncmp(line, "version", 7) == 0) {
            std::printf("version %lu\n", WinUHidGetDriverInterfaceVersion());
        } else if (std::sscanf(line, "create %u", &lane) == 1) {
            const DWORD err = create(lane);
            if (err == 0) std::printf("ok %u\n", lane);
            else std::printf("err %u %lu\n", lane, err);
        } else if (std::sscanf(line, "state %u %d %d %d", &lane, &x, &y, &buttons) == 4) {
            const DWORD err = submit(lane, x, y, buttons);
            if (err == 0) std::printf("sent %u\n", lane);
            else std::printf("err %u %lu\n", lane, err);
        } else if (std::sscanf(line, "hid %u", &lane) == 1) {
            const std::wstring id = lane < VGUN_MAX_LANES ? vgun::lane_hid_instance_id(lane) : std::wstring();
            std::printf("hid %u %ls\n", lane, id.empty() ? L"-" : id.c_str());
        } else if (std::sscanf(line, "destroy %u", &lane) == 1) {
            destroy(lane);
            std::printf("gone %u\n", lane);
        } else if (std::strncmp(line, "quit", 4) == 0) {
            for (unsigned i = 0; i < VGUN_MAX_LANES; ++i) destroy(i);
            std::printf("bye\n");
            return 0;
        } else {
            std::printf("err 0 %d\n", ERROR_INVALID_PARAMETER);
        }
    }
    return 0;
}
