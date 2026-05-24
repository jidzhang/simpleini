#include "test_framework.h"
#include "../SimpleIni.h"
#include <string.h>

// ============================================================
// Test: Basic load and save
// ============================================================
void test_basic_load_save() {
    const char* data =
        "[section1]\n"
        "key1 = value1\n"
        "\n"
        "[section2]\n"
        "key2 = value2\n";

    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error rc = ini.LoadData(data);
    TEST_ASSERT_EQ(rc, SI_OK);

    const char* pv = ini.GetValue("section1", "key1", NULL);
    TEST_ASSERT_STR_EQ(pv, "value1");

    pv = ini.GetValue("section2", "key2", NULL);
    TEST_ASSERT_STR_EQ(pv, "value2");

    std::string output;
    rc = ini.Save(output);
    TEST_ASSERT_EQ(rc, SI_OK);
    TEST_ASSERT(output.find("[section1]") != std::string::npos);
}

// ============================================================
// Test: GetValue with default fallback
// ============================================================
void test_get_value_default() {
    const char* data = "[s]\nkey = val\n";

    CSimpleIniA ini;
    SI_Error rc = ini.LoadData(data);
    TEST_ASSERT_EQ(rc, SI_OK);

    const char* pv = ini.GetValue("s", "key", "default");
    TEST_ASSERT_STR_EQ(pv, "val");

    pv = ini.GetValue("s", "nonexist", "default");
    TEST_ASSERT_STR_EQ(pv, "default");

    pv = ini.GetValue("nosuch", "key", "default");
    TEST_ASSERT_STR_EQ(pv, "default");

    pv = ini.GetValue("s", "nonexist");
    TEST_ASSERT_NULL(pv);
}

// ============================================================
// Test: SetValue (insert and update)
// ============================================================
void test_set_value() {
    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.SetValue("sec", "key", "first");
    TEST_ASSERT_EQ(rc, SI_INSERTED);

    const char* pv = ini.GetValue("sec", "key", NULL);
    TEST_ASSERT_STR_EQ(pv, "first");

    rc = ini.SetValue("sec", "key", "second");
    TEST_ASSERT_EQ(rc, SI_UPDATED);

    pv = ini.GetValue("sec", "key", NULL);
    TEST_ASSERT_STR_EQ(pv, "second");

    rc = ini.SetValue("sec", "key2", "val2");
    TEST_ASSERT_EQ(rc, SI_INSERTED);

    pv = ini.GetValue("sec", "key2", NULL);
    TEST_ASSERT_STR_EQ(pv, "val2");
}

// ============================================================
// Test: Delete key and section
// ============================================================
void test_delete() {
    const char* data =
        "[s1]\n"
        "k1 = v1\n"
        "k2 = v2\n"
        "\n"
        "[s2]\n"
        "k3 = v3\n";

    CSimpleIniA ini;
    ini.LoadData(data);

    bool done = ini.Delete("s1", "k1", true);
    TEST_ASSERT_EQ(done, true);
    TEST_ASSERT_NULL(ini.GetValue("s1", "k1", NULL));

    done = ini.Delete("s1", "k1");
    TEST_ASSERT_EQ(done, false);

    done = ini.Delete("s2", NULL);
    TEST_ASSERT_EQ(done, true);
    TEST_ASSERT_NULL(ini.GetValue("s2", "k3", NULL));

    done = ini.Delete("s2", NULL);
    TEST_ASSERT_EQ(done, false);
}

// ============================================================
// Test: Multi-key support
// ============================================================
void test_multikey() {
    const char* data =
        "[s]\n"
        "key = value1\n"
        "key = value2\n"
        "key = value3\n";

    CSimpleIniA ini;
    ini.SetMultiKey(true);
    SI_Error rc = ini.LoadData(data);
    TEST_ASSERT_EQ(rc, SI_OK);

    bool hasMulti = false;
    const char* pv = ini.GetValue("s", "key", NULL, &hasMulti);
    TEST_ASSERT_NOT_NULL(pv);
    TEST_ASSERT_EQ(hasMulti, true);

    CSimpleIniA::TNamesDepend values;
    ini.GetAllValues("s", "key", values);
    TEST_ASSERT_EQ(values.size(), 3);
}

// ============================================================
// Test: Multi-line values
// ============================================================
void test_multiline() {
    const char* data =
        "[s]\n"
        "key = \"line1\n"
        "line2\n"
        "line3\"\n";

    CSimpleIniA ini;
    ini.SetMultiLine(true);
    SI_Error rc = ini.LoadData(data);
    TEST_ASSERT_EQ(rc, SI_OK);

    const char* pv = ini.GetValue("s", "key", NULL);
    TEST_ASSERT_NOT_NULL(pv);
    TEST_ASSERT(strlen(pv) > 0);
}

// ============================================================
// Test: UTF-8 content
// ============================================================
void test_utf8() {
    const char* data =
        "[\xE4\xB8\xAD\xE6\x96\x87]\n"
        "\xE9\x94\xAE = \xE5\x80\xBC\n";

    CSimpleIniA ini;
    ini.SetUnicode(true);
    SI_Error rc = ini.LoadData(data);
    TEST_ASSERT_EQ(rc, SI_OK);

    const char* pv = ini.GetValue("\xE4\xB8\xAD\xE6\x96\x87", "\xE9\x94\xAE", NULL);
    TEST_ASSERT_NOT_NULL(pv);
    TEST_ASSERT_STR_EQ(pv, "\xE5\x80\xBC");
}

// ============================================================
// Test: Case sensitivity
// ============================================================
void test_case_sensitive() {
    const char* data = "[Section]\nKey = Value\n";

    // CSimpleIniA is case-INsensitive by default (uses SI_NoCase)
    {
        CSimpleIniA ini;
        ini.LoadData(data);

        TEST_ASSERT_NOT_NULL(ini.GetValue("Section", "Key", NULL));
        TEST_ASSERT_NOT_NULL(ini.GetValue("section", "key", NULL));
        TEST_ASSERT_NOT_NULL(ini.GetValue("SECTION", "KEY", NULL));
    }

    // CSimpleIniCaseA is case-sensitive (uses SI_Case)
    {
        CSimpleIniCaseA ini;
        ini.LoadData(data);

        TEST_ASSERT_NOT_NULL(ini.GetValue("Section", "Key", NULL));
        TEST_ASSERT_NULL(ini.GetValue("section", "Key", NULL));
        TEST_ASSERT_NULL(ini.GetValue("Section", "key", NULL));
    }
}

// ============================================================
// Test: Keys without section
// ============================================================
void test_no_section() {
    const char* data = "key = value\n";

    CSimpleIniA ini;
    ini.LoadData(data);

    const char* pv = ini.GetValue("", "key", NULL);
    TEST_ASSERT_NOT_NULL(pv);
    TEST_ASSERT_STR_EQ(pv, "value");
}

// ============================================================
// Test: Empty values
// ============================================================
void test_empty_value() {
    const char* data = "[s]\nkey = \n";

    CSimpleIniA ini;
    ini.LoadData(data);

    const char* pv = ini.GetValue("s", "key", NULL);
    TEST_ASSERT_NOT_NULL(pv);
    TEST_ASSERT_STR_EQ(pv, "");
}

// ============================================================
// Test: Numeric and boolean conversions
// ============================================================
void test_numeric() {
    const char* data =
        "[s]\n"
        "int = 42\n"
        "double = 3.14\n"
        "bool = true\n";

    CSimpleIniA ini;
    ini.LoadData(data);

    long n = ini.GetLongValue("s", "int", 0);
    TEST_ASSERT_EQ(n, 42);

    double d = ini.GetDoubleValue("s", "double", 0.0);
    TEST_ASSERT(d > 3.13 && d < 3.15);

    bool b = ini.GetBoolValue("s", "bool", false);
    TEST_ASSERT_EQ(b, true);

    ini.SetLongValue("s", "newint", 100);
    n = ini.GetLongValue("s", "newint", 0);
    TEST_ASSERT_EQ(n, 100);
}

// ============================================================
// Main
// ============================================================
int main() {
    printf("SimpleIni Lightweight Test Suite\n");
    printf("================================\n\n");

    RUN_TEST(basic_load_save);
    RUN_TEST(get_value_default);
    RUN_TEST(set_value);
    RUN_TEST(delete);
    RUN_TEST(multikey);
    RUN_TEST(multiline);
    RUN_TEST(utf8);
    RUN_TEST(case_sensitive);
    RUN_TEST(no_section);
    RUN_TEST(empty_value);
    RUN_TEST(numeric);

    test_summary();

    return g_failures > 0 ? 1 : 0;
}
