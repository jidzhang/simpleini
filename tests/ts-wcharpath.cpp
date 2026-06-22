#include "../SimpleIni.h"

// This entire file is Windows-only. It validates end-to-end behavior that the
// lightweight/ASCII-path tests cannot cover: reading and writing INI files
// whose *path* contains non-ASCII (Chinese) characters, using the wchar_t
// overloads of LoadFile/SaveFile. On Windows these resolve to _wfopen/_wremove
// and bypass the MBCS code-page pitfalls that char* paths fall into.
//
// The source file itself is saved as UTF-8-with-BOM so MSVC interprets the
// wide string literals (L"测试") as UTF-16 regardless of the system locale or
// the /utf-8 switch.
#if defined(_WIN32)

#include "gtest/gtest.h"

#include <cstdio>
#include <direct.h>     // _wmkdir, _wrmdir
#include <sys/stat.h>

namespace {

// Wide-string literals for the test fixtures. All Chinese.
const wchar_t kSection[] = L"设置";
const wchar_t kKey[]     = L"键";
const wchar_t kValue[]   = L"值";

// Temp file name with Chinese characters, written into the test's CWD.
const wchar_t kTempFile[] = L"测试配置_临时.ini";

// Read the first n bytes of a file in binary mode. Returns the count actually
// read; 0 on open failure.
size_t ReadFileHead(const wchar_t* path, unsigned char* buf, size_t n) {
    FILE* fp = NULL;
#if __STDC_WANT_SECURE_LIB__
    _wfopen_s(&fp, path, L"rb");
#else
    fp = _wfopen(path, L"rb");
#endif
    if (!fp) return 0;
    size_t got = fread(buf, 1, n, fp);
    fclose(fp);
    return got;
}

} // namespace

class TestWCharPath : public ::testing::Test {
protected:
    void TearDown() override {
        // Best-effort cleanup so a failed run does not leave litter behind.
        _wremove(kTempFile);
    }
};

// Core scenario: write Chinese content to a Chinese-named file with BOM,
// then reload via a fresh instance and verify the values survive intact.
TEST_F(TestWCharPath, TestChineseFilenameRoundTrip) {
    {
        CSimpleIniW ini;
        ini.SetUnicode(true);
        SI_Error rc = ini.SetValue(kSection, kKey, kValue);
        ASSERT_EQ(rc, SI_INSERTED);

        rc = ini.SaveFile(kTempFile, /*a_bAddSignature=*/true);
        ASSERT_EQ(rc, SI_OK);
    }

    {
        CSimpleIniW ini;
        ini.SetUnicode(true);
        SI_Error rc = ini.LoadFile(kTempFile);
        ASSERT_EQ(rc, SI_OK);

        const wchar_t* result = ini.GetValue(kSection, kKey);
        ASSERT_STREQ(result, kValue);
    }
}

// Verify that SaveFile(..., true) actually emits a UTF-8 BOM on disk when the
// path itself is non-ASCII. Guards against regressions in the signature branch.
TEST_F(TestWCharPath, TestPreservesBOM) {
    CSimpleIniW ini;
    ini.SetUnicode(true);
    ASSERT_EQ(ini.SetValue(kSection, kKey, kValue), SI_INSERTED);
    ASSERT_EQ(ini.SaveFile(kTempFile, /*a_bAddSignature=*/true), SI_OK);

    unsigned char head[3] = {0, 0, 0};
    size_t got = ReadFileHead(kTempFile, head, sizeof(head));
    ASSERT_EQ(got, (size_t)3);
    EXPECT_EQ(head[0], 0xEF);
    EXPECT_EQ(head[1], 0xBB);
    EXPECT_EQ(head[2], 0xBF);
}

// Verify a Chinese *directory* segment in the path works too, not just the
// filename. Skips gracefully if the directory cannot be created.
TEST_F(TestWCharPath, TestChinesePathChineseContent) {
    const wchar_t kDir[]  = L"测试目录";
    const wchar_t kPath[] = L"测试目录/配置.ini";

    if (_wmkdir(kDir) != 0) {
        // May already exist or be unwritable; probe existence instead.
        struct _stat st;
        if (_wstat(kDir, &st) != 0) {
            GTEST_SKIP() << "Cannot create Chinese-named directory; skipping.";
        }
    }

    {
        CSimpleIniW ini;
        ini.SetUnicode(true);
        ASSERT_EQ(ini.SetValue(kSection, kKey, kValue), SI_INSERTED);
        ASSERT_EQ(ini.SaveFile(kPath, /*a_bAddSignature=*/true), SI_OK);
    }

    {
        CSimpleIniW ini;
        ini.SetUnicode(true);
        ASSERT_EQ(ini.LoadFile(kPath), SI_OK);
        ASSERT_STREQ(ini.GetValue(kSection, kKey), kValue);
    }

    _wremove(kPath);
    _wrmdir(kDir);
}

#endif // _WIN32
