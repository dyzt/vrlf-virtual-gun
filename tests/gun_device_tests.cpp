#include "shared/gun_device.h"
#include "tests/test_main.h"

using namespace vgun;

TEST(descriptor_is_the_52_byte_absolute_mouse) {
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR_BYTES, 52);
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR[0], 0x05);   // Usage Page (Generic Desktop)
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR[3], 0x02);   // Usage (Mouse)
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR[15], 0x05);  // Usage Maximum: button 5
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR[42], 0xFF);  // Logical Maximum low byte
    CHECK_EQ(VGUN_REPORT_DESCRIPTOR[43], 0x7F);  // Logical Maximum high byte: 32767
}

TEST(identity_is_the_project_pid_codes_pid) {
    CHECK_EQ(VGUN_VID, 0x1209);
    CHECK_EQ(VGUN_PID, 0x5647);
}

TEST(report_is_five_packed_bytes) {
    CHECK_EQ(sizeof(GunReport), VGUN_REPORT_BYTES);
    const GunReport r{0x05, 0x1234, 0x7FFF};
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&r);
    CHECK_EQ(bytes[0], 0x05);
    CHECK_EQ(bytes[1], 0x34);
    CHECK_EQ(bytes[2], 0x12);
    CHECK_EQ(bytes[3], 0xFF);
    CHECK_EQ(bytes[4], 0x7F);
}

TEST(centre_is_the_middle_of_the_range_with_no_buttons) {
    CHECK_EQ(GUN_CENTRE.buttons, 0);
    CHECK_EQ(GUN_CENTRE.x, 16384);
    CHECK_EQ(GUN_CENTRE.y, 16384);
}

TEST(make_report_clamps_coordinates_and_masks_buttons) {
    const GunReport r = make_report(40000, -5, 0xFF);
    CHECK_EQ(r.x, 32767);
    CHECK_EQ(r.y, 0);
    CHECK_EQ(r.buttons, 0x1F);
}

TEST(make_report_passes_in_range_values_through) {
    const GunReport r = make_report(100, 200, 3);
    CHECK_EQ(r.x, 100);
    CHECK_EQ(r.y, 200);
    CHECK_EQ(r.buttons, 3);
}

TEST(pinned_prefix_is_fixed_per_lane) {
    CHECK(vgun::pinned_prefix(0) == L"2&56524c30&0");
    CHECK(vgun::pinned_prefix(7) == L"2&56524c37&0");
}

TEST(pinned_ids_and_path_match_what_windows_reports) {
    CHECK(vgun::pinned_hid_instance_id(0) == L"HID\\HID_DEVICE_SYSTEM_VHF\\2&56524c30&0&0000");
    CHECK(vgun::pinned_device_path(3) ==
          L"\\\\?\\HID#HID_DEVICE_SYSTEM_VHF#2&56524c33&0&0000#{378de44c-56ef-11d1-bc8c-00a0c91405dd}");
    CHECK(vgun::same_id(L"HID\\HID_DEVICE_SYSTEM_VHF\\2&56524C30&0&0000", vgun::pinned_hid_instance_id(0)));
    CHECK(!vgun::same_id(L"HID\\HID_DEVICE_SYSTEM_VHF\\2&33377591&0&0000", vgun::pinned_hid_instance_id(0)));
}

TEST(lane_key_names_follow_the_root_prefix) {
    CHECK(vgun::lane_instance_name(2) == L"VRLFGun2");
    CHECK(vgun::lane_key_name(L"1&39b203ef&4", 1) == L"1&39b203ef&4&VRLFGun1");
    CHECK(vgun::is_lane_key_of(L"1&39b203ef&4&VRLFGun1", 1));
    CHECK(vgun::is_lane_key_of(L"VHF\\HID_DEVICE_SYSTEM_VHF\\1&39B203EF&4&VRLFGUN1", 1));
    CHECK(!vgun::is_lane_key_of(L"1&39b203ef&4&VRLFGun1", 0));
    CHECK(!vgun::is_lane_key_of(L"1&39b203ef&4&VRLFGun11", 1));
    CHECK(!vgun::is_lane_key_of(L"1&39b203ef&0&{8317515c-289f-59a6-a2d4-0369baa25b50}", 0));
    CHECK(!vgun::is_lane_key_of(L"Gun1", 1));
}

int main() { return run_all(); }
