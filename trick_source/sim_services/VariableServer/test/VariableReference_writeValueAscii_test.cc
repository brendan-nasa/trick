#include "VariableReference_test.hh"


TEST_F(VariableReference_test, writeValueAscii_int) {
    // ARRANGE
    // Create a variable to make a reference for
    int test_a = 5;
    (void) memmgr->declare_extern_var(&test_a, "int test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "5");
}

TEST_F(VariableReference_test, writeValueAscii_int_arr) {
    // ARRANGE
    // Create a variable to make a reference for
    int test_a[5] = {1, 2, 3, 4, 5};
    (void) memmgr->declare_extern_var(&test_a, "int test_a[5]");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "1,2,3,4,5");
}

TEST_F(VariableReference_test, writeValueAscii_double) {
    // ARRANGE
    // Create a variable to make a reference for
    double test_a = 867.309;
    (void) memmgr->declare_extern_var(&test_a, "double test_a");
    Trick::VariableReference ref_a("test_a");
    std::stringstream ss_a;

    double test_b = std::numeric_limits<double>::max();
    (void) memmgr->declare_extern_var(&test_b, "double test_b");
    Trick::VariableReference ref_b("test_b");
    std::stringstream ss_b;

    double test_c = std::numeric_limits<double>::min();
    (void) memmgr->declare_extern_var(&test_c, "double test_c");
    Trick::VariableReference ref_c("test_c");
    std::stringstream ss_c;

    // ACT
    ref_a.stageValue();
    ref_a.prepareForWrite();
    ref_a.writeValueAscii(ss_a);

    ref_b.stageValue();
    ref_b.prepareForWrite();
    ref_b.writeValueAscii(ss_b);

    ref_c.stageValue();
    ref_c.prepareForWrite();
    ref_c.writeValueAscii(ss_c);

    // ASSERT
    EXPECT_EQ(ss_a.str(), "867.309");
    EXPECT_EQ(ss_b.str(), "1.797693134862316e+308");
    EXPECT_EQ(ss_c.str(), "2.225073858507201e-308");
}

TEST_F(VariableReference_test, writeValueAscii_char) {
    // ARRANGE
    // Create a variable to make a reference for
    char test_a = 'j';
    (void) memmgr->declare_extern_var(&test_a, "char test_a");
    Trick::VariableReference ref_a("test_a");
    std::stringstream ssa;

    char test_b[7] = "jackie";
    (void) memmgr->declare_extern_var(&test_b, "char test_b[7]");
    Trick::VariableReference ref_b("test_b");
    std::stringstream ssb;

    // ACT
    ref_a.stageValue();
    ref_b.stageValue();

    ref_a.prepareForWrite();
    ref_b.prepareForWrite();

    ref_a.writeValueAscii(ssa);
    ref_b.writeValueAscii(ssb);


    // ASSERT
    EXPECT_EQ(ssa.str(), "106");
    EXPECT_EQ(ssb.str(), "jackie");
}


TEST_F(VariableReference_test, writeValueAscii_unsigned_char) {
    // ARRANGE
    // Create a variable to make a reference for
    unsigned char test_a = 'j';
    (void) memmgr->declare_extern_var(&test_a, "unsigned char test_a");
    Trick::VariableReference ref_a("test_a");
    std::stringstream ssa;

    unsigned char test_b[8] = "jackie\n";
    (void) memmgr->declare_extern_var(&test_b, "unsigned char test_b[8]");
    Trick::VariableReference ref_b("test_b");
    std::stringstream ssb;

    // ACT
    ref_a.stageValue();
    ref_b.stageValue();

    ref_a.prepareForWrite();
    ref_b.prepareForWrite();

    ref_a.writeValueAscii(ssa);
    ref_b.writeValueAscii(ssb);

    // ASSERT
    EXPECT_EQ(ssa.str(), "106");
    EXPECT_EQ(ssb.str(), "jackie\\n");
}


TEST_F(VariableReference_test, writeValueAscii_wide_char) {
    // ARRANGE
    // Create a variable to make a reference for
    wchar_t test_a = L'J';
    (void) memmgr->declare_extern_var(&test_a, "wchar_t test_a");
    Trick::VariableReference ref_a("test_a");
    std::stringstream ssa;

    wchar_t test_b[15] = L"jackiebutwider";
    (void) memmgr->declare_extern_var(&test_b, "wchar_t test_b[15]");
    Trick::VariableReference ref_b("test_b");
    std::stringstream ssb;

   // ACT
    ref_a.stageValue();
    ref_b.stageValue();

    ref_a.prepareForWrite();
    ref_b.prepareForWrite();

    ref_a.writeValueAscii(ssa);
    ref_b.writeValueAscii(ssb);

    // ASSERT
    // Original variable server behavior prints individual wchar as its ascii value
    EXPECT_EQ(ssa.str(), "74");
    EXPECT_EQ(ssb.str(), "jackiebutwider");
}

TEST_F(VariableReference_test, DISABLED_writeValueAscii_wide_char_unconstrained) {
    TestObject obj;
    obj.wchar_str = (wchar_t *) malloc (sizeof(wchar_t) * 7);
    for (int i = 0; i < 6; i++) {
        obj.wchar_str[i] = L'j';
    }
    obj.wchar_str[6] = L'\0';
    (void) memmgr->declare_extern_var(&obj, "TestObject obj");
    // (void) memmgr->declare_extern_var(&obj.wchar_str, "wchar_t * obj.wchar_str");

    Trick::VariableReference ref("obj.wchar_str");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "jjjjjj");

}

TEST_F(VariableReference_test, writeValueAscii_std_string) {
    // ARRANGE
    // Create a variable to make a reference for
    std::string test_a = "jackiebutstringy";
    (void) memmgr->declare_extern_var(&test_a, "std::string test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "jackiebutstringy");
}

TEST_F(VariableReference_test, writeValueAscii_escape_characters) {
    // ARRANGE
    // Create a variable to make a reference for
    std::string test_a = "\n\t\b\a\"\f\r\v";
    (void) memmgr->declare_extern_var(&test_a, "std::string test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "\\n\\t\\b\\a\"\\f\\n\\v");
}

TEST_F(VariableReference_test, writeValueAscii_boolean) {
    // ARRANGE
    // Create a variable to make a reference for
    bool test_a = true;
    (void) memmgr->declare_extern_var(&test_a, "bool test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "1");
}

TEST_F(VariableReference_test, writeValueAscii_short) {
    // ARRANGE
    // Create a variable to make a reference for
    short test_a = 255;
    (void) memmgr->declare_extern_var(&test_a, "short test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueAscii(ss);

    // ASSERT
    EXPECT_EQ(ss.str(), "255");
}

// Golden tests for the ASCII wire format. These pin the exact text clients receive today so the
// formatting code can be rewritten without iostreams and still produce byte-identical output.

namespace {
std::string writeAscii(Trick::VariableReference& ref) {
    ref.stageValue();
    ref.prepareForWrite();
    std::stringstream ss;
    EXPECT_EQ(ref.writeValueAscii(ss), 0);
    return ss.str();
}
}

TEST_F(VariableReference_test, writeValueAscii_golden_integer_limits) {
    int i_min = std::numeric_limits<int>::min();
    int i_max = std::numeric_limits<int>::max();
    short s_min = std::numeric_limits<short>::min();
    unsigned short us_max = std::numeric_limits<unsigned short>::max();
    unsigned int ui_max = std::numeric_limits<unsigned int>::max();
    long l_min = std::numeric_limits<long>::min();
    unsigned long ul_max = std::numeric_limits<unsigned long>::max();
    long long ll_min = std::numeric_limits<long long>::min();
    unsigned long long ull_max = std::numeric_limits<unsigned long long>::max();
    (void) memmgr->declare_extern_var(&i_min, "int i_min");
    (void) memmgr->declare_extern_var(&i_max, "int i_max");
    (void) memmgr->declare_extern_var(&s_min, "short s_min");
    (void) memmgr->declare_extern_var(&us_max, "unsigned short us_max");
    (void) memmgr->declare_extern_var(&ui_max, "unsigned int ui_max");
    (void) memmgr->declare_extern_var(&l_min, "long l_min");
    (void) memmgr->declare_extern_var(&ul_max, "unsigned long ul_max");
    (void) memmgr->declare_extern_var(&ll_min, "long long ll_min");
    (void) memmgr->declare_extern_var(&ull_max, "unsigned long long ull_max");

    Trick::VariableReference ref_i_min("i_min");
    Trick::VariableReference ref_i_max("i_max");
    Trick::VariableReference ref_s_min("s_min");
    Trick::VariableReference ref_us_max("us_max");
    Trick::VariableReference ref_ui_max("ui_max");
    Trick::VariableReference ref_l_min("l_min");
    Trick::VariableReference ref_ul_max("ul_max");
    Trick::VariableReference ref_ll_min("ll_min");
    Trick::VariableReference ref_ull_max("ull_max");

    EXPECT_EQ(writeAscii(ref_i_min), "-2147483648");
    EXPECT_EQ(writeAscii(ref_i_max), "2147483647");
    EXPECT_EQ(writeAscii(ref_s_min), "-32768");
    EXPECT_EQ(writeAscii(ref_us_max), "65535");
    EXPECT_EQ(writeAscii(ref_ui_max), "4294967295");
    EXPECT_EQ(writeAscii(ref_l_min), "-9223372036854775808");
    EXPECT_EQ(writeAscii(ref_ul_max), "18446744073709551615");
    EXPECT_EQ(writeAscii(ref_ll_min), "-9223372036854775808");
    EXPECT_EQ(writeAscii(ref_ull_max), "18446744073709551615");
}

TEST_F(VariableReference_test, writeValueAscii_golden_float) {
    float f_pi = 3.14159265f;
    float f_tenth = 0.1f;
    float f_tiny = 1e-10f;
    float f_big = 123456789.0f;
    float f_neg = -2.5f;
    (void) memmgr->declare_extern_var(&f_pi, "float f_pi");
    (void) memmgr->declare_extern_var(&f_tenth, "float f_tenth");
    (void) memmgr->declare_extern_var(&f_tiny, "float f_tiny");
    (void) memmgr->declare_extern_var(&f_big, "float f_big");
    (void) memmgr->declare_extern_var(&f_neg, "float f_neg");

    Trick::VariableReference ref_pi("f_pi");
    Trick::VariableReference ref_tenth("f_tenth");
    Trick::VariableReference ref_tiny("f_tiny");
    Trick::VariableReference ref_big("f_big");
    Trick::VariableReference ref_neg("f_neg");

    // Floats are written with 8 significant digits in %g style.
    EXPECT_EQ(writeAscii(ref_pi), "3.1415927");
    EXPECT_EQ(writeAscii(ref_tenth), "0.1");
    EXPECT_EQ(writeAscii(ref_tiny), "1e-10");
    EXPECT_EQ(writeAscii(ref_big), "1.2345679e+08");
    EXPECT_EQ(writeAscii(ref_neg), "-2.5");
}

TEST_F(VariableReference_test, writeValueAscii_golden_double) {
    double d_tenth = 0.1;
    double d_third = 1.0 / 3.0;
    double d_e16 = 1e16;
    double d_whole = 123456789.0;
    double d_neg_zero = -0.0;
    double d_small = 1.5e-300;
    (void) memmgr->declare_extern_var(&d_tenth, "double d_tenth");
    (void) memmgr->declare_extern_var(&d_third, "double d_third");
    (void) memmgr->declare_extern_var(&d_e16, "double d_e16");
    (void) memmgr->declare_extern_var(&d_whole, "double d_whole");
    (void) memmgr->declare_extern_var(&d_neg_zero, "double d_neg_zero");
    (void) memmgr->declare_extern_var(&d_small, "double d_small");

    Trick::VariableReference ref_tenth("d_tenth");
    Trick::VariableReference ref_third("d_third");
    Trick::VariableReference ref_e16("d_e16");
    Trick::VariableReference ref_whole("d_whole");
    Trick::VariableReference ref_neg_zero("d_neg_zero");
    Trick::VariableReference ref_small("d_small");

    // Doubles are written with 16 significant digits in %g style.
    EXPECT_EQ(writeAscii(ref_tenth), "0.1");
    EXPECT_EQ(writeAscii(ref_third), "0.3333333333333333");
    EXPECT_EQ(writeAscii(ref_e16), "1e+16");
    EXPECT_EQ(writeAscii(ref_whole), "123456789");
    EXPECT_EQ(writeAscii(ref_neg_zero), "-0");
    EXPECT_EQ(writeAscii(ref_small), "1.5e-300");
}

TEST_F(VariableReference_test, writeValueAscii_golden_double_non_finite) {
    double d_inf = std::numeric_limits<double>::infinity();
    double d_neg_inf = -std::numeric_limits<double>::infinity();
    double d_nan = std::numeric_limits<double>::quiet_NaN();
    (void) memmgr->declare_extern_var(&d_inf, "double d_inf");
    (void) memmgr->declare_extern_var(&d_neg_inf, "double d_neg_inf");
    (void) memmgr->declare_extern_var(&d_nan, "double d_nan");

    Trick::VariableReference ref_inf("d_inf");
    Trick::VariableReference ref_neg_inf("d_neg_inf");
    Trick::VariableReference ref_nan("d_nan");

    EXPECT_EQ(writeAscii(ref_inf), "inf");
    EXPECT_EQ(writeAscii(ref_neg_inf), "-inf");
    EXPECT_EQ(writeAscii(ref_nan), "nan");
}

TEST_F(VariableReference_test, writeValueAscii_golden_arrays) {
    double d_arr[3] = {1.5, -2.25, 0.0};
    float f_arr[2] = {0.5f, 1e-10f};
    int i_2d[2][3] = {{1, 2, 3}, {4, 5, 6}};
    (void) memmgr->declare_extern_var(&d_arr, "double d_arr[3]");
    (void) memmgr->declare_extern_var(&f_arr, "float f_arr[2]");
    (void) memmgr->declare_extern_var(&i_2d, "int i_2d[2][3]");

    Trick::VariableReference ref_d("d_arr");
    Trick::VariableReference ref_f("f_arr");
    Trick::VariableReference ref_2d("i_2d");
    Trick::VariableReference ref_row("i_2d[1]");

    EXPECT_EQ(writeAscii(ref_d), "1.5,-2.25,0");
    EXPECT_EQ(writeAscii(ref_f), "0.5,1e-10");
    EXPECT_EQ(writeAscii(ref_2d), "1,2,3,4,5,6");
    EXPECT_EQ(writeAscii(ref_row), "4,5,6");
}

TEST_F(VariableReference_test, writeValueAscii_golden_strings) {
    std::string s_empty = "";
    std::string s_spaces = "a b\tc";
    char c_ctrl[4] = {'a', 0x01, 'b', '\0'};
    std::wstring w_str = L"wide";
    (void) memmgr->declare_extern_var(&s_empty, "std::string s_empty");
    (void) memmgr->declare_extern_var(&s_spaces, "std::string s_spaces");
    (void) memmgr->declare_extern_var(&c_ctrl, "char c_ctrl[4]");
    (void) memmgr->declare_extern_var(&w_str, "std::wstring w_str");

    Trick::VariableReference ref_empty("s_empty");
    Trick::VariableReference ref_spaces("s_spaces");
    Trick::VariableReference ref_ctrl("c_ctrl");
    Trick::VariableReference ref_wide("w_str");

    EXPECT_EQ(writeAscii(ref_empty), "");
    EXPECT_EQ(writeAscii(ref_spaces), "a b\\tc");
    // Other non-printable characters are written as two-digit hex escapes.
    EXPECT_EQ(writeAscii(ref_ctrl), "a\\x01b");
    EXPECT_EQ(writeAscii(ref_wide), "wide");
}

TEST_F(VariableReference_test, writeValueAscii_golden_char_pointer) {
    char storage[] = "pointed";
    char * c_ptr = storage;
    (void) memmgr->declare_extern_var(&c_ptr, "char * c_ptr");
    Trick::VariableReference ref("c_ptr");

    EXPECT_EQ(writeAscii(ref), "pointed");
}

// Known bug: non-ASCII bytes are sign-extended by the "\\x%02x" escape, which writes
// "\\xffffffe9" into a 6-byte buffer. Enable once write_escaped_string is fixed.
TEST_F(VariableReference_test, DISABLED_writeValueAscii_golden_high_bit_char) {
    char c_high[3] = {'a', static_cast<char>(0xe9), '\0'};
    (void) memmgr->declare_extern_var(&c_high, "char c_high[3]");
    Trick::VariableReference ref("c_high");

    EXPECT_EQ(writeAscii(ref), "a\\xe9");
}

TEST_F(VariableReference_test, getSizeAscii_matches_written_length) {
    double d_val = 1.0 / 3.0;
    int i_arr[3] = {10, -20, 30};
    (void) memmgr->declare_extern_var(&d_val, "double d_val");
    (void) memmgr->declare_extern_var(&i_arr, "int i_arr[3]");
    Trick::VariableReference ref_d("d_val");
    Trick::VariableReference ref_arr("i_arr");

    std::string d_text = writeAscii(ref_d);
    std::string arr_text = writeAscii(ref_arr);

    EXPECT_EQ(ref_d.getSizeAscii(), static_cast<int>(d_text.size()));
    EXPECT_EQ(ref_arr.getSizeAscii(), static_cast<int>(arr_text.size()));
}