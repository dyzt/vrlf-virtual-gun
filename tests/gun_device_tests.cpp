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

int main() { return run_all(); }
