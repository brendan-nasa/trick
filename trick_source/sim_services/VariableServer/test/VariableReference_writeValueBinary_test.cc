#include "VariableReference_test.hh"


TEST_F(VariableReference_test, writeValueBinary_int) {
    // ARRANGE
    // Create a variable to make a reference for
    int test_a = 4095;
    (void) memmgr->declare_extern_var(&test_a, "int test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss);

    
    unsigned char expected_bytes[4] = {0xFF, 0x0F, 0x00, 0x00};
    char * actual_bytes = (char *) malloc (sizeof(int));

    ss.read(actual_bytes, 4);

    // ASSERT
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}

TEST_F(VariableReference_test, writeValueBinary_float) {
    // ARRANGE
    // Create a variable to make a reference for
    float test_a = 40.95;
    (void) memmgr->declare_extern_var(&test_a, "float test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss);
    
    unsigned char expected_bytes[4] = {0xcd, 0xcc, 0x23, 0x42};

    char * actual_bytes = (char *) malloc (sizeof(float));

    ss.read(actual_bytes, 4);

    // ASSERT
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}

TEST_F(VariableReference_test, writeValueBinary_int_arr) {
    // ARRANGE
    // Create a variable to make a reference for
    int test_a[3] = {1, 2, 3};
    (void) memmgr->declare_extern_var(&test_a, "int test_a[3]");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss);

    // ASSERT
    char * actual_bytes = (char *) malloc (sizeof(int )* 3);
    ss.read(actual_bytes, sizeof(int) * 3);
    unsigned char expected_bytes[12] = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00};


    // ASSERT
    for (int i = 0; i < 12; i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}


TEST_F(VariableReference_test, writeValueBinary_double_arr) {
    // ARRANGE
    // Create a variable to make a reference for
    double test_a[3] = {1.0, 10.0, 100.0};
    (void) memmgr->declare_extern_var(&test_a, "double test_a[3]");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss);

    // ASSERT
    char * actual_bytes = (char *) malloc (sizeof(double )* 3);
    ss.read(actual_bytes, sizeof(double) * 3);
    unsigned char expected_bytes[24] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x40};

    // ASSERT
    for (int i = 0; i < 12; i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}


TEST_F(VariableReference_test, writeValueBinary_string) {
    // ARRANGE
    // Create a variable to make a reference for
    std::string test_a = "abcdef";
    (void) memmgr->declare_extern_var(&test_a, "std::string test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss);

    // ASSERT
    char * actual_bytes = (char *) malloc (test_a.length());
    ss.read(actual_bytes, test_a.length());
    unsigned char expected_bytes[6] = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66};

    // ASSERT
    for (unsigned int i = 0; i < test_a.length(); i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}

TEST_F(VariableReference_test, writeNameBinary) {
    // ARRANGE
    // Create a variable to make a reference for
    std::string test_a = "abcdef";
    (void) memmgr->declare_extern_var(&test_a, "std::string test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeNameBinary(ss);

    // ASSERT
    char * actual_bytes = (char *) malloc (ref.getName().size());
    ss.read(actual_bytes, 6);
    unsigned char expected_bytes[6] = {0x74, 0x65, 0x73, 0x74, 0x5f, 0x61};

    // ASSERT
    for (int i = 0; i < ref.getName().size(); i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}

TEST_F(VariableReference_test, writeNameLengthBinary) {
    // ARRANGE
    // Create a variable to make a reference for
    std::string test_a = "abcdef";
    (void) memmgr->declare_extern_var(&test_a, "std::string test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeNameLengthBinary(ss);

    // ASSERT
    char * actual_bytes = (char *) malloc (sizeof(int));
    ss.read(actual_bytes, sizeof(int));
    unsigned char expected_bytes[sizeof(int)] = {0x06, 0x00, 0x00, 0x00};

    // ASSERT
    for (int i = 0; i < sizeof(int); i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}


TEST_F(VariableReference_test, writeValueBinaryByteswap_int) {
    // ARRANGE
    // Create a variable to make a reference for
    int test_a = 4095;
    (void) memmgr->declare_extern_var(&test_a, "int test_a");
    Trick::VariableReference ref("test_a");
    std::stringstream ss;

    // ACT
    ref.stageValue();
    ref.prepareForWrite();
    ref.writeValueBinary(ss, true);

    
    unsigned char expected_bytes[4] = {0x00, 0x00, 0x0F, 0xFF};
    char * actual_bytes = (char *) malloc (sizeof(int));

    ss.read(actual_bytes, 4);

    // ASSERT
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(static_cast<unsigned char>(actual_bytes[i]), expected_bytes[i]);
    }
}

// Golden tests for the binary wire format. These compare the complete output, including its
// length, so the writers can be rewritten without iostreams and stay byte-identical.
// Like the tests above, the expected values assume a little-endian host.

namespace {
std::string bytesOf(std::initializer_list<unsigned char> bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::string writeBinary(Trick::VariableReference& ref, bool byteswap = false) {
    ref.stageValue();
    ref.prepareForWrite();
    std::stringstream ss;
    ref.writeValueBinary(ss, byteswap);
    return ss.str();
}
}

TEST_F(VariableReference_test, writeValueBinary_golden_header_fields) {
    int test_int = 7;
    (void) memmgr->declare_extern_var(&test_int, "int test_int");
    Trick::VariableReference ref("test_int");
    ref.stageValue();
    ref.prepareForWrite();

    std::stringstream type_ss, size_ss, len_ss, name_ss;
    ref.writeTypeBinary(type_ss);
    ref.writeSizeBinary(size_ss);
    ref.writeNameLengthBinary(len_ss);
    ref.writeNameBinary(name_ss);

    EXPECT_EQ(type_ss.str(), bytesOf({TRICK_INTEGER, 0x00, 0x00, 0x00}));
    EXPECT_EQ(size_ss.str(), bytesOf({0x04, 0x00, 0x00, 0x00}));
    EXPECT_EQ(len_ss.str(), bytesOf({0x08, 0x00, 0x00, 0x00}));
    EXPECT_EQ(name_ss.str(), "test_int");
}

TEST_F(VariableReference_test, writeValueBinary_golden_header_fields_byteswap) {
    int test_int = 7;
    (void) memmgr->declare_extern_var(&test_int, "int test_int");
    Trick::VariableReference ref("test_int");
    ref.stageValue();
    ref.prepareForWrite();

    std::stringstream type_ss, size_ss, len_ss, name_ss;
    ref.writeTypeBinary(type_ss, true);
    ref.writeSizeBinary(size_ss, true);
    ref.writeNameLengthBinary(len_ss, true);
    ref.writeNameBinary(name_ss, true);

    EXPECT_EQ(type_ss.str(), bytesOf({0x00, 0x00, 0x00, TRICK_INTEGER}));
    EXPECT_EQ(size_ss.str(), bytesOf({0x00, 0x00, 0x00, 0x04}));
    EXPECT_EQ(len_ss.str(), bytesOf({0x00, 0x00, 0x00, 0x08}));
    // Names are never byteswapped.
    EXPECT_EQ(name_ss.str(), "test_int");
}

TEST_F(VariableReference_test, writeValueBinary_golden_string_includes_terminator) {
    std::string test_str = "abc";
    char test_chars[6] = "abc";
    (void) memmgr->declare_extern_var(&test_str, "std::string test_str");
    (void) memmgr->declare_extern_var(&test_chars, "char test_chars[6]");
    Trick::VariableReference ref_str("test_str");
    Trick::VariableReference ref_chars("test_chars");

    // std::string sends its characters plus the terminating NUL; char arrays send the whole array.
    EXPECT_EQ(writeBinary(ref_str), bytesOf({'a', 'b', 'c', 0x00}));
    EXPECT_EQ(ref_str.getSizeBinary(), 4);
    EXPECT_EQ(writeBinary(ref_chars), bytesOf({'a', 'b', 'c', 0x00, 0x00, 0x00}));
    EXPECT_EQ(ref_chars.getSizeBinary(), 6);
}

TEST_F(VariableReference_test, writeValueBinary_golden_byteswap_sizes) {
    short test_short = 0x0102;
    long long test_ll = 0x0102030405060708LL;
    double test_double = 1.0;
    double test_darr[2] = {1.0, -2.0};
    char test_chars[4] = "abc";
    (void) memmgr->declare_extern_var(&test_short, "short test_short");
    (void) memmgr->declare_extern_var(&test_ll, "long long test_ll");
    (void) memmgr->declare_extern_var(&test_double, "double test_double");
    (void) memmgr->declare_extern_var(&test_darr, "double test_darr[2]");
    (void) memmgr->declare_extern_var(&test_chars, "char test_chars[4]");
    Trick::VariableReference ref_short("test_short");
    Trick::VariableReference ref_ll("test_ll");
    Trick::VariableReference ref_double("test_double");
    Trick::VariableReference ref_darr("test_darr");
    Trick::VariableReference ref_chars("test_chars");

    EXPECT_EQ(writeBinary(ref_short, true), bytesOf({0x01, 0x02}));
    EXPECT_EQ(writeBinary(ref_ll, true), bytesOf({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}));
    EXPECT_EQ(writeBinary(ref_double, true), bytesOf({0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    EXPECT_EQ(writeBinary(ref_darr, true), bytesOf({0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                    0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    // Single-byte elements are copied unchanged.
    EXPECT_EQ(writeBinary(ref_chars, true), bytesOf({'a', 'b', 'c', 0x00}));
}

// Known bug: byteswap_var() switches on the attribute size, which for std::string is
// sizeof(std::string), so no case matches and the zeroed buffer is sent instead of the text.
// Enable once byteswapped strings are fixed.
TEST_F(VariableReference_test, DISABLED_writeValueBinary_golden_byteswap_string) {
    std::string test_str = "abc";
    (void) memmgr->declare_extern_var(&test_str, "std::string test_str");
    Trick::VariableReference ref("test_str");

    EXPECT_EQ(writeBinary(ref, true), bytesOf({'a', 'b', 'c', 0x00}));
}

TEST_F(VariableReference_test, writeValueBinary_golden_bad_ref) {
    Trick::VariableReference ref("no_such_var");

    std::stringstream type_ss;
    std::string value = writeBinary(ref);
    ref.writeTypeBinary(type_ss);

    // Unresolvable names are sent as TRICK_NUMBER_OF_TYPES with a zero-filled int value.
    EXPECT_EQ(type_ss.str(), bytesOf({TRICK_NUMBER_OF_TYPES, 0x00, 0x00, 0x00}));
    EXPECT_EQ(value, bytesOf({0x00, 0x00, 0x00, 0x00}));
    EXPECT_EQ(ref.getSizeBinary(), 4);
}