#include "setup/strings.h"
#include "tests/test_main.h"

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
