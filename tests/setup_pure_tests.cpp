#include "setup/install_mode.h"
#include "setup/strings.h"
#include "tests/test_main.h"

using setup::ChooseInstallMode;
using setup::InstallMode;
using setup::InstallState;

namespace {

InstallState CompleteState() {
    InstallState s;
    s.version = L"1.0.1";
    s.install_dir = L"C:\\Program Files\\VRLF Virtual Lightgun";
    s.cert_thumbprint = L"7536756946A0373603E0E8FDEAC2C79791CD74DA";
    s.driver_inf = L"oem88.inf";
    return s;
}

}  // namespace

TEST(no_state_is_a_fresh_install) {
    CHECK(ChooseInstallMode(std::nullopt, 0, 0) == InstallMode::Fresh);
    CHECK(ChooseInstallMode(std::nullopt, 1, 1) == InstallMode::Fresh);
}

TEST(complete_install_with_one_live_device_updates_in_place) {
    CHECK(ChooseInstallMode(CompleteState(), 1, 1) == InstallMode::Update);
}

TEST(incomplete_install_is_reinstalled) {
    for (int field = 0; field < 3; ++field) {
        InstallState s = CompleteState();
        if (field == 0) s.version.clear();
        if (field == 1) s.cert_thumbprint.clear();
        if (field == 2) s.driver_inf.clear();
        CHECK(ChooseInstallMode(s, 1, 1) == InstallMode::Reinstall);
    }
}

TEST(interrupted_update_is_reinstalled) {
    for (int field = 0; field < 4; ++field) {
        InstallState s = CompleteState();
        if (field == 0) s.pending_cert_thumbprint = L"AA";
        if (field == 1) s.pending_driver_inf = L"oem90.inf";
        if (field == 2) s.previous_cert_thumbprint = L"BB";
        if (field == 3) s.previous_driver_inf = L"oem87.inf";
        CHECK(ChooseInstallMode(s, 1, 1) == InstallMode::Reinstall);
    }
}

TEST(missing_extra_or_absent_device_is_reinstalled) {
    CHECK(ChooseInstallMode(CompleteState(), 0, 0) == InstallMode::Reinstall);
    CHECK(ChooseInstallMode(CompleteState(), 1, 0) == InstallMode::Reinstall);
    CHECK(ChooseInstallMode(CompleteState(), 2, 1) == InstallMode::Reinstall);
    CHECK(ChooseInstallMode(CompleteState(), 2, 2) == InstallMode::Reinstall);
}

TEST(hex_upper_formats_bytes) {
    const unsigned char bytes[] = {0x00, 0x0A, 0xFF, 0x3C};
    CHECK(setup::HexUpper(bytes, 4) == L"000AFF3C");
}

TEST(multi_sz_has_double_terminator) {
    const std::wstring s = setup::MultiSz({L"Root\\A", L"B"});
    CHECK_EQ(s.size(), 10);
    CHECK(s[6] == L'\0');
    CHECK(s[8] == L'\0');
    CHECK(s[9] == L'\0');
    CHECK(s.substr(0, 6) == L"Root\\A");
}

int main() { return run_all(); }
