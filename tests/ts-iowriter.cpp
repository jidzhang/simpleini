#include "../SimpleIni.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <string>

// ============================================================
// Regression tests for Bug 4: OutputWriter error propagation.
//
// FileWriter used to ignore the return value of fputs, so SaveFile would
// return SI_OK even when writes failed (disk full, broken pipe, closed
// FILE*, ...). These tests exercise the failure path through both the
// OutputWriter interface and the SaveFile entry point.
// ============================================================

// A custom OutputWriter that always fails. Verifies that Save surfaces the
// failure via Fail() and returns SI_FAIL.
class FailingWriter : public CSimpleIniA::OutputWriter {
public:
	FailingWriter() { }
	void Write(const char * /*a_pBuf*/) {
		this->m_bFail = true;
	}
};

TEST(TestIoWriter, TestSaveReturnsFailWhenOutputWriterFails) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ASSERT_EQ(ini.SetValue("section", "key", "value"), SI_INSERTED);

	FailingWriter w;
	SI_Error rc = ini.Save(w, false);
	EXPECT_EQ(rc, SI_FAIL);
	EXPECT_TRUE(w.Fail());
}

// A custom OutputWriter that fails on the Nth write. Verifies the flag stays
// latched once set and Save still reports failure.
class FailAfterNWriter : public CSimpleIniA::OutputWriter {
public:
	explicit FailAfterNWriter(size_t n) : m_count(0), m_n(n) { }
	void Write(const char * /*a_pBuf*/) {
		if (m_count >= m_n) {
			this->m_bFail = true;
		}
		++m_count;
	}
	size_t m_count;
	size_t m_n;
};

TEST(TestIoWriter, TestSaveReturnsFailOnMidStreamError) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetValue("section", "key1", "value1");
	ini.SetValue("section", "key2", "value2");

	FailAfterNWriter w(2);
	SI_Error rc = ini.Save(w, false);
	EXPECT_EQ(rc, SI_FAIL);
	EXPECT_TRUE(w.Fail());
	EXPECT_GT(w.m_count, 2u);
}

// A custom OutputWriter that always succeeds. Sanity check that the new flag
// machinery does not produce false positives on the happy path.
class CountingWriter : public CSimpleIniA::OutputWriter {
public:
	CountingWriter() : m_writes(0) { }
	void Write(const char * /*a_pBuf*/) { ++m_writes; }
	size_t m_writes;
};

TEST(TestIoWriter, TestSaveSucceedsWhenOutputWriterIsHealthy) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetValue("section", "key", "value");

	CountingWriter w;
	SI_Error rc = ini.Save(w, false);
	EXPECT_EQ(rc, SI_OK);
	EXPECT_FALSE(w.Fail());
	EXPECT_GT(w.m_writes, 0u);
}

// FileWriter path: writing to a closed FILE* must surface as SI_FAIL rather
// than silently reporting success.
#if !defined(_WIN32) || !defined(_MSC_VER)
// On POSIX we can fclose() the stream and then attempt to write to it; the
// underlying fputs will fail and the new logic latches the error.
TEST(TestIoWriter, TestSaveFileToClosedFileFails) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetValue("section", "key", "value");

	FILE * fp = tmpfile();
	ASSERT_NE(fp, (FILE *)nullptr);
	fclose(fp);  // hand over an already-closed FILE*

	SI_Error rc = ini.SaveFile(fp, false);
	EXPECT_EQ(rc, SI_FAIL);
}
#endif
