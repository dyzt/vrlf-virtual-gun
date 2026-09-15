#include "setup/install_mode.h"
#include "setup/lane_pin_rules.h"
#include "setup/setup_lock.h"
#include "setup/strings.h"
#include "tests/test_main.h"

#include <string>
#include <thread>

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

namespace {
const std::wstring PIN0 = L"2&56524c30&0";
setup::LaneKey Key(bool exists, bool installed, const wchar_t* prefix) {
    setup::LaneKey k;
    k.exists = exists;
    k.installed = installed;
    k.prefix = prefix;
    return k;
}
}  // namespace

TEST(pin_decision_covers_every_key_state) {
    using setup::PinAction;
    CHECK(setup::DecidePin(Key(false, false, L""), PIN0, false) == PinAction::CreateFirst);
    CHECK(setup::DecidePin(Key(true, false, L"2&56524c30&0"), PIN0, false) == PinAction::RemoveThenCreate);
    CHECK(setup::DecidePin(Key(true, true, L"2&56524C30&0"), PIN0, true) == PinAction::AlreadyPinned);
    CHECK(setup::DecidePin(Key(true, true, L"2&33377591&0"), PIN0, false) == PinAction::Write);
    CHECK(setup::DecidePin(Key(true, true, L""), PIN0, false) == PinAction::Write);
    CHECK(setup::DecidePin(Key(true, true, L"2&33377591&0"), PIN0, true) == PinAction::Collision);
}

TEST(collision_ignores_the_target_and_the_same_lanes_orphans) {
    const std::wstring target = L"1&39b203ef&4&VRLFGun0";
    std::vector<setup::VhfKey> keys = {
        {L"1&39b203ef&4&VRLFGun0", L"2&33377591&0"},
        {L"1&aaaaaaaa&4&VRLFGun0", L"2&56524c30&0"},  // lane 0's orphan from an earlier install
        {L"1&39b203ef&4&VRLFGun1", L"2&56524c31&0"},
    };
    CHECK(!setup::IsCollision(keys, target, 0, PIN0));
    keys.push_back({L"1&39b203ef&0&{8317515c-289f-59a6-a2d4-0369baa25b50}", L"2&56524C30&0"});
    CHECK(setup::IsCollision(keys, target, 0, PIN0));
    std::vector<setup::VhfKey> other_lane = {{L"1&39b203ef&4&VRLFGun3", L"2&56524c30&0"}};
    CHECK(setup::IsCollision(other_lane, target, 0, PIN0));
}

TEST(installed_lane_key_needs_a_driver_and_no_failed_install_flag) {
    CHECK(setup::IsInstalledLaneKey(true, 0));
    CHECK(!setup::IsInstalledLaneKey(false, 0));
    CHECK(!setup::IsInstalledLaneKey(true, 0x40));
    CHECK(!setup::IsInstalledLaneKey(false, 0x40));
    CHECK(setup::IsInstalledLaneKey(true, 0x20));
}

TEST(pin_exit_code_never_hides_a_failure) {
    setup::PinSummary ok;
    CHECK(setup::PinExitCode(0, ok) == 0);
    CHECK(setup::PinExitCode(3010, ok) == 3010);
    setup::PinSummary no_driver;
    no_driver.driver_unavailable = true;
    CHECK(setup::PinExitCode(3010, no_driver) == 3010);
    CHECK(setup::PinExitCode(0, no_driver) == 1);
    setup::PinSummary one_failed;
    one_failed.failed = 1;
    CHECK(setup::PinExitCode(0, one_failed) == 1);
    CHECK(setup::PinExitCode(3010, one_failed) == 1);
}

TEST(setup_lock_is_busy_while_another_run_holds_it) {
    const std::wstring name = L"Local\\VRLFVirtualGunSetupTest-" + std::to_wstring(GetCurrentProcessId());
    setup::SetupLock mine(name.c_str());
    {
        setup::SetupLock theirs(name.c_str());
        CHECK(theirs.Acquire(0) == setup::LockResult::Held);
        // A mutex is re-entrant on its owning thread, so the second run has to be another thread.
        setup::LockResult seen = setup::LockResult::Held;
        std::thread([&] { seen = mine.Acquire(0); }).join();
        CHECK(seen == setup::LockResult::Busy);
    }
    setup::LockResult after = setup::LockResult::Busy;
    std::thread([&] {
        setup::SetupLock again(name.c_str());
        after = again.Acquire(0);
    }).join();
    CHECK(after == setup::LockResult::Held);
}

TEST(setup_lock_left_by_a_crashed_run_is_taken_over) {
    const std::wstring name = L"Local\\VRLFVirtualGunSetupCrash-" + std::to_wstring(GetCurrentProcessId());
    auto* crashed = new setup::SetupLock(name.c_str());
    std::thread([&] { CHECK(crashed->Acquire(0) == setup::LockResult::Held); }).join();  // exits holding it
    setup::SetupLock next(name.c_str());
    CHECK(next.Acquire(0) == setup::LockResult::Held);
}

int main() { return run_all(); }
