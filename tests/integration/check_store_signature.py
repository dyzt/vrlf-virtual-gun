"""Checks that the driver package in the Windows driver store is signed by the certificate
the install recorded, and that the certificate is trusted. Exit 0 = pass.

An update that retires the certificate still signing the store copy leaves a driver that
may not load after the next reboot.
"""
import json
import subprocess
import sys

PS = r"""
$state = Get-ItemProperty 'HKLM:\SOFTWARE\VRLF\VirtualGun'
$inf = $state.DriverInf
$pkg = pnputil /enum-drivers | Out-String
$dirs = Get-ChildItem C:\Windows\System32\DriverStore\FileRepository -Directory -Filter 'vrlfvirtualgun.inf_*'
$files = foreach ($d in $dirs) {
  foreach ($f in 'VRLFVirtualGun.dll', 'vrlfvirtualgun.cat') {
    $s = Get-AuthenticodeSignature (Join-Path $d.FullName $f)
    @{ path = (Join-Path $d.FullName $f); status = [string]$s.Status; signer = [string]$s.SignerCertificate.Thumbprint }
  }
}
$trusted = @(Get-ChildItem Cert:\LocalMachine\Root, Cert:\LocalMachine\TrustedPublisher |
  Where-Object Subject -like '*VRLF*' | ForEach-Object { $_.PSParentPath.Split(':')[-1] + '|' + $_.Thumbprint })
@{ cert = [string]$state.CertThumbprint; inf = [string]$inf; store_dirs = @($dirs).Count;
   files = @($files); trusted = $trusted } | ConvertTo-Json -Depth 4
"""


def main():
    out = subprocess.run(["powershell", "-NoProfile", "-Command", PS], capture_output=True, text=True)
    info = json.loads(out.stdout)
    cert = info["cert"]
    failures = []
    print("recorded cert:", cert, "inf:", info["inf"], "store packages:", info["store_dirs"])
    if info["store_dirs"] != 1:
        failures.append(f"expected one VRLF package in the driver store, found {info['store_dirs']}")
    for f in info["files"]:
        print(f"  {f['path']}: {f['status']} {f['signer']}")
        if f["signer"] != cert:
            failures.append(f"{f['path']} signed by {f['signer']}, recorded cert is {cert}")
        if f["status"] != "Valid":
            failures.append(f"{f['path']} signature status {f['status']}")
    trusted = sorted(info["trusted"])
    print("trusted:", trusted)
    if trusted != sorted([f"LocalMachine\\Root|{cert}", f"LocalMachine\\TrustedPublisher|{cert}"]):
        failures.append("trusted VRLF certificates are not exactly the recorded one in Root and TrustedPublisher")
    for msg in failures:
        print("FAIL:", msg)
    if failures:
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
