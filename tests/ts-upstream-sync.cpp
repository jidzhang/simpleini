// Regression tests for the fixes synced from upstream brofield/simpleini
// (v4.26 d749efc "Bug fixes after AI code review", v4.27 3a5e854 SetQuotes
// wrap + cee498b code review), adapted to this fork's tree.
//
// The LoadData rollback tests need to make an allocation fail mid-parse.
// That is done with a glibc malloc wrapper enabled by linking the test
// binary with -Wl,--wrap=malloc on Linux; on other platforms those tests
// are skipped.
//
// The oversize tests below rely on the allocation limit being reached; the
// 1 MiB limit (instead of the default 1 GiB) is set for the whole test
// binary via SI_MAX_FILE_SIZE in tests/CMakeLists.txt. It must be uniform
// across all translation units, hence not defined in this file.
#include "../SimpleIni.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <string>

#if defined(__linux__) && defined(__GLIBC__)
#include <atomic>
#include <malloc.h>
#endif

// ---------------------------------------------------------------------------
// glibc malloc wrapper: fails allocations while armed, for the LoadData
// rollback regression tests. The nothrow operator new overrides must live
// at global scope so they replace the global ones.
// ---------------------------------------------------------------------------
#if defined(__linux__) && defined(__GLIBC__)
static std::atomic<bool> g_failSmallAllocs(false);
static std::atomic<bool> g_failAfterBudget(false);
static std::atomic<int> g_allocBudget(0);

extern "C" void* __real_malloc(size_t size);
extern "C" void* __wrap_malloc(size_t size) {
	if (g_failAfterBudget.load()) {
		if (g_allocBudget.fetch_sub(1) <= 0) {
			return NULL;
		}
	}
	if (g_failSmallAllocs.load() && size < 256) {
		return NULL;
	}
	return __real_malloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
	return __wrap_malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
	return __wrap_malloc(size);
}

static size_t CurrentHeapBytes() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	const size_t bytes = static_cast<size_t>(mallinfo().uordblks);
#pragma GCC diagnostic pop
	return bytes;
}
#endif // __linux__ && __GLIBC__

namespace upstreamsync {

static void StripCR(std::string& s) {
	s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
}

// ---------------------------------------------------------------------------
// SetDoubleValue full-precision formatting (upstream "%.17g" fix)
// ---------------------------------------------------------------------------

TEST(UpstreamSyncDouble, SetDoubleValueRoundTripsFullPrecision) {
	CSimpleIniA ini;
	ini.SetDoubleValue("s", "k", 3.141592653589793);
	EXPECT_DOUBLE_EQ(3.141592653589793, ini.GetDoubleValue("s", "k", 0.0));

	// "%f" formatting wrote "0.000000" and lost the value entirely
	ini.SetDoubleValue("s", "small", 1e-300);
	EXPECT_DOUBLE_EQ(1e-300, ini.GetDoubleValue("s", "small", 0.0));

	ini.SetDoubleValue("s", "big", 1e300);
	EXPECT_DOUBLE_EQ(1e300, ini.GetDoubleValue("s", "big", 0.0));
}

TEST(UpstreamSyncDouble, SetDoubleValueSavesShortestForm) {
	CSimpleIniA ini;
	ini.SetDoubleValue("s", "k", 0.5);
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);
	EXPECT_NE(std::string::npos, text.find("k = 0.5\n"));
	EXPECT_EQ(std::string::npos, text.find("0.500000"));
}

// ---------------------------------------------------------------------------
// SetQuotes: a value that is itself quoted must be wrapped on save
// (upstream 3a5e854), otherwise the loader strips its quotes on reload
// ---------------------------------------------------------------------------

TEST(UpstreamSyncQuotes, FullyQuotedValueSurvivesRoundTrip) {
	CSimpleIniA ini;
	ini.SetQuotes();
	ini.SetValue("s", "k", "\"abc\"");
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);
	// the saved form must wrap the quoted value a second time
	EXPECT_NE(std::string::npos, text.find("k = \"\"abc\"\"\n"));

	CSimpleIniA ini2;
	ini2.SetQuotes();
	ASSERT_EQ(SI_OK, ini2.LoadData(text));
	EXPECT_STREQ("\"abc\"", ini2.GetValue("s", "k"));
}

TEST(UpstreamSyncQuotes, PartiallyQuotedValuesAreNotRewrapped) {
	const char* values[] = { "\"abc", "abc\"", "a\"bc" };
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		CSimpleIniA ini;
		ini.SetQuotes();
		ini.SetValue("s", "k", values[i]);
		std::string text;
		ASSERT_EQ(SI_OK, ini.Save(text)) << values[i];
		StripCR(text);
		EXPECT_EQ(std::string::npos, text.find("\"\"")) << values[i];

		CSimpleIniA ini2;
		ini2.SetQuotes();
		ASSERT_EQ(SI_OK, ini2.LoadData(text)) << values[i];
		EXPECT_STREQ(values[i], ini2.GetValue("s", "k")) << values[i];
	}
}

TEST(UpstreamSyncQuotes, QuotedValueWithSpacesSurvivesRoundTrip) {
	CSimpleIniA ini;
	ini.SetQuotes();
	ini.SetValue("s", "k", "\"a b\"");
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);

	CSimpleIniA ini2;
	ini2.SetQuotes();
	ASSERT_EQ(SI_OK, ini2.LoadData(text));
	EXPECT_STREQ("\"a b\"", ini2.GetValue("s", "k"));
}

// ---------------------------------------------------------------------------
// Multi-line end tag must not collide with a line inside the value
// (upstream cee498b MultiLineTextHasTag + suffixed tags)
// ---------------------------------------------------------------------------

TEST(UpstreamSyncMultiline, ValueContainingEndTagSurvivesRoundTrip) {
	CSimpleIniA ini(false, false, true);
	ini.SetValue("s", "k", "line1\nEND_OF_TEXT\nline3");
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);
	// the plain tag appears inside the value, so a suffixed tag is picked
	EXPECT_NE(std::string::npos, text.find("<<<END_OF_TEXT_1\n"));

	CSimpleIniA ini2(false, false, true);
	ASSERT_EQ(SI_OK, ini2.LoadData(text));
	EXPECT_STREQ("line1\nEND_OF_TEXT\nline3", ini2.GetValue("s", "k"));
}

TEST(UpstreamSyncMultiline, SuffixedTagSkipsCollision) {
	// the value contains lines matching both the plain tag and the first
	// suffix, so the writer must settle on END_OF_TEXT_2
	CSimpleIniA ini(false, false, true);
	ini.SetValue("s", "k", "END_OF_TEXT\nEND_OF_TEXT_1\nplain");
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);
	EXPECT_NE(std::string::npos, text.find("<<<END_OF_TEXT_2\n"));

	CSimpleIniA ini2(false, false, true);
	ASSERT_EQ(SI_OK, ini2.LoadData(text));
	EXPECT_STREQ("END_OF_TEXT\nEND_OF_TEXT_1\nplain", ini2.GetValue("s", "k"));
}

TEST(UpstreamSyncMultiline, PlainTagUsedWhenNoCollision) {
	CSimpleIniA ini(false, false, true);
	ini.SetValue("s", "k", "line1\nline2");
	std::string text;
	ASSERT_EQ(SI_OK, ini.Save(text));
	StripCR(text);
	EXPECT_NE(std::string::npos, text.find("<<<END_OF_TEXT\n"));

	CSimpleIniA ini2(false, false, true);
	ASSERT_EQ(SI_OK, ini2.LoadData(text));
	EXPECT_STREQ("line1\nline2", ini2.GetValue("s", "k"));
}

// ---------------------------------------------------------------------------
// SetValue rejects a NULL section (upstream cee498b)
// ---------------------------------------------------------------------------

TEST(UpstreamSyncNullArgs, SetValueWithNullSectionFails) {
	CSimpleIniA ini;
	EXPECT_EQ(SI_FAIL, ini.SetValue(NULL, "k", "v"));
	EXPECT_FALSE(ini.KeyExists("", "k"));
	EXPECT_TRUE(ini.IsEmpty());
}

// ---------------------------------------------------------------------------
// SetLongValue/SetDoubleValue/SetBoolValue must fail when the storage
// conversion fails instead of inserting uninitialized buffer contents
// (upstream d749efc)
// ---------------------------------------------------------------------------

struct FailConvertFromStore {
	bool m_bStoreIsUtf8;

	FailConvertFromStore(bool a_bStoreIsUtf8 = false)
		: m_bStoreIsUtf8(a_bStoreIsUtf8) { }

	size_t SizeFromStore(const char*, size_t a_uInputDataLen) {
		return a_uInputDataLen;
	}

	bool ConvertFromStore(const char*, size_t, char*, size_t) {
		return false;
	}

	size_t SizeToStore(const char* a_pInputData) {
		return strlen(a_pInputData) + 1;
	}

	bool ConvertToStore(const char* a_pInputData, char* a_pOutputData,
		size_t a_uOutputDataSize) {
		const size_t uLen = strlen(a_pInputData) + 1;
		if (uLen > a_uOutputDataSize) {
			return false;
		}
		memcpy(a_pOutputData, a_pInputData, uLen);
		return true;
	}
};

typedef CSimpleIniTempl<char, SI_GenericNoCase<char>, FailConvertFromStore>
	FailingSetIni;

TEST(UpstreamSyncSetters, SettersFailWhenStorageConversionFails) {
	FailingSetIni ini;
	EXPECT_EQ(SI_FAIL, ini.SetLongValue("s", "k", 42));
	EXPECT_FALSE(ini.KeyExists("s", "k"));

	EXPECT_EQ(SI_FAIL, ini.SetDoubleValue("s", "k", 1.5));
	EXPECT_FALSE(ini.KeyExists("s", "k"));

	EXPECT_EQ(SI_FAIL, ini.SetBoolValue("s", "k", true));
	EXPECT_FALSE(ini.KeyExists("s", "k"));

	EXPECT_TRUE(ini.IsEmpty());
}

// ---------------------------------------------------------------------------
// Save must not corrupt a multiline value when output conversion fails
// (upstream d749efc: OutputMultiLineText copies lines before converting)
// ---------------------------------------------------------------------------

struct FailOnFourthConvertToStore {
	bool m_bStoreIsUtf8;
	int m_calls;

	FailOnFourthConvertToStore(bool a_bStoreIsUtf8 = false)
		: m_bStoreIsUtf8(a_bStoreIsUtf8), m_calls(0) { }
	FailOnFourthConvertToStore(const FailOnFourthConvertToStore& rhs)
		: m_bStoreIsUtf8(rhs.m_bStoreIsUtf8), m_calls(rhs.m_calls) { }
	FailOnFourthConvertToStore& operator=(const FailOnFourthConvertToStore& rhs) {
		m_bStoreIsUtf8 = rhs.m_bStoreIsUtf8;
		m_calls = rhs.m_calls;
		return *this;
	}

	size_t SizeFromStore(const char*, size_t a_uInputDataLen) {
		return a_uInputDataLen;
	}

	bool ConvertFromStore(const char* a_pInputData, size_t a_uInputDataLen,
		char* a_pOutputData, size_t a_uOutputDataSize) {
		if (a_uInputDataLen > a_uOutputDataSize) {
			return false;
		}
		memcpy(a_pOutputData, a_pInputData, a_uInputDataLen);
		return true;
	}

	size_t SizeToStore(const char* a_pInputData) {
		return strlen(a_pInputData) + 1;
	}

	bool ConvertToStore(const char* a_pInputData, char* a_pOutputData,
		size_t a_uOutputDataSize) {
		// calls: 1 section, 2 key, 3 value, 4.. lines of the value
		if (++m_calls >= 4) {
			return false;
		}
		const size_t uLen = strlen(a_pInputData) + 1;
		if (uLen > a_uOutputDataSize) {
			return false;
		}
		memcpy(a_pOutputData, a_pInputData, uLen);
		return true;
	}
};

typedef CSimpleIniTempl<char, SI_GenericNoCase<char>, FailOnFourthConvertToStore>
	FailingSaveIni;

TEST(UpstreamSyncSave, MultilineValueIntactWhenSaveFails) {
	FailingSaveIni ini(false, false, true);
	ini.SetValue("section", "key", "line1\nline2\nline3");

	std::string output;
	ASSERT_EQ(SI_FAIL, ini.Save(output));

	// the in-memory value must not be mutated by the failed save
	EXPECT_STREQ("line1\nline2\nline3", ini.GetValue("section", "key"));
}

// ---------------------------------------------------------------------------
// CopyString enforces SI_MAX_FILE_SIZE and AddEntry rolls back cleanly
// (upstream d749efc/cee498b). SI_MAX_FILE_SIZE is 1 MiB in the test binary,
// so these allocate ~1 MiB, not the default 1 GiB.
// ---------------------------------------------------------------------------

TEST(UpstreamSyncOversize, SetValueRejectsOversizedValue) {
	std::string huge;
	ASSERT_NO_THROW(huge.assign(SI_MAX_FILE_SIZE, 'x'));

	CSimpleIniA ini;
	EXPECT_EQ(SI_NOMEM, ini.SetValue("s", "k", huge.c_str()));
	EXPECT_FALSE(ini.KeyExists("s", "k"));
	// AddEntry must not leave the newly created section behind either
	EXPECT_FALSE(ini.SectionExists("s"));
	EXPECT_TRUE(ini.IsEmpty());
}

TEST(UpstreamSyncOversize, SetValueRejectsOversizedValueInExistingData) {
	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData("[existing]\nkey = value\n"));

	std::string huge;
	ASSERT_NO_THROW(huge.assign(SI_MAX_FILE_SIZE, 'x'));
	EXPECT_EQ(SI_NOMEM, ini.SetValue("newsection", "key", huge.c_str()));

	EXPECT_FALSE(ini.SectionExists("newsection"));
	EXPECT_TRUE(ini.SectionExists("existing"));
	EXPECT_STREQ("value", ini.GetValue("existing", "key"));
}

// ---------------------------------------------------------------------------
// A failed incremental LoadData must roll back everything it added
// (upstream d749efc UndoIncrementalLoadData). Requires allocation-failure
// injection, which is only available with the glibc malloc wrapper.
// ---------------------------------------------------------------------------

#if defined(__linux__) && defined(__GLIBC__)

TEST(UpstreamSyncRollback, DoesNotPartiallyMergeOnAddEntryFailure) {
	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData("[existing]\nkey = value\n"));
	ASSERT_TRUE(ini.SectionExists("existing"));

	const std::string second = "[b1]\nk = v\n[b2]\nk = v\n";

	// let a few allocations succeed so that [b1] is fully added, then fail
	g_failAfterBudget = true;
	g_allocBudget = 5;
	const SI_Error rc = ini.LoadData(second);
	g_failAfterBudget = false;
	g_allocBudget = 0;

	ASSERT_EQ(SI_NOMEM, rc);
	EXPECT_TRUE(ini.SectionExists("existing"));
	EXPECT_FALSE(ini.SectionExists("b1"));
	EXPECT_FALSE(ini.SectionExists("b2"));
	EXPECT_STREQ("value", ini.GetValue("existing", "key"));

	CSimpleIniA::TNamesDepend sections;
	ini.GetAllSections(sections);
	ASSERT_EQ(1u, sections.size());
	ASSERT_STREQ("existing", sections.front().pItem);
}

TEST(UpstreamSyncRollback, DoesNotLeakParseBufferOnAddEntryFailure) {
	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData("[existing]\nkey = value\n"));

	std::string second = "[b1]\nk = v\n";
	for (int i = 0; i < 200; ++i) {
		second += "[s" + std::to_string(i) + "]\nk = v\n";
	}

	const size_t heapBefore = CurrentHeapBytes();
	g_failSmallAllocs = true;
	const SI_Error rc = ini.LoadData(second);
	g_failSmallAllocs = false;

	ASSERT_EQ(SI_NOMEM, rc);
	const size_t heapAfter = CurrentHeapBytes();
	EXPECT_LE(heapAfter, heapBefore + 4096);
}

// A failed incremental LoadData must not touch data that already existed.
// The object below holds SetValue data while m_pData is still NULL, i.e. the
// !bCopyStrings branch: an early bug wiped everything with m_data.clear().
// The first line of the loaded data also creates the empty-named section,
// which must be tracked and rolled back like any other section. Sweeping the
// allocation budget makes the load fail at every possible point, including
// after the empty-named section and [k2] were already inserted.
TEST(UpstreamSyncRollback, PreservesPreExistingDataAndRollsBackEmptySection) {
	const std::string second = "early = 1\n[k2]\nx = y\n";
	int nFailed = 0;

	for (int budget = 0; budget < 64; ++budget) {
		CSimpleIniA ini;
		ASSERT_EQ(SI_OK, ini.SetValue("keep", "k", "v"));

		g_failAfterBudget = true;
		g_allocBudget = budget;
		const SI_Error rc = ini.LoadData(second);
		g_failAfterBudget = false;
		g_allocBudget = 0;

		if (rc >= 0) {
			continue;
		}
		++nFailed;

		EXPECT_EQ(SI_NOMEM, rc);
		EXPECT_TRUE(ini.SectionExists("keep"));
		EXPECT_STREQ("v", ini.GetValue("keep", "k"));
		EXPECT_FALSE(ini.SectionExists(""));
		EXPECT_FALSE(ini.SectionExists("k2"));
	}
	ASSERT_GT(nFailed, 0) << "no allocation budget triggered a failure";
}

TEST(UpstreamSyncRollback, FileCommentFailureLeavesStateClean) {
	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData("[existing]\nkey = value\n"));

	g_failSmallAllocs = true;
	const SI_Error rc = ini.LoadData("; file comment\n\n[k]\nv = 1\n");
	g_failSmallAllocs = false;

	ASSERT_EQ(SI_NOMEM, rc);
	EXPECT_TRUE(ini.SectionExists("existing"));
	EXPECT_STREQ("value", ini.GetValue("existing", "key"));
	// saving must not read the freed parse buffer through a stale comment
	std::string text;
	EXPECT_EQ(SI_OK, ini.Save(text));
	EXPECT_TRUE(text.find("key = value") != std::string::npos);
}

#else // !(__linux__ && __GLIBC__)

TEST(UpstreamSyncRollback, DISABLED_NotBuiltWithoutGlibcMallocWrapper) {
	// rollback tests need -Wl,--wrap=malloc (Linux + glibc); they run in CI
}

#endif // __linux__ && __GLIBC__

// ---------------------------------------------------------------------------
// LoadFile(FILE*) 64-bit size probing and full-read verification
// (upstream d749efc GetFileSize)
// ---------------------------------------------------------------------------

TEST(UpstreamSyncLoadFile, LoadsFromFILEPointer) {
	FILE* fp = fopen("tests.ini", "rb");
	ASSERT_NE(static_cast<FILE*>(NULL), fp);

	CSimpleIniA ini;
	EXPECT_EQ(SI_OK, ini.LoadFile(fp));
	fclose(fp);
	EXPECT_STREQ("value1", ini.GetValue("section1", "key1"));
}

TEST(UpstreamSyncLoadFile, EmptyFileReturnsOk) {
	FILE* fp = tmpfile();
	ASSERT_NE(static_cast<FILE*>(NULL), fp);

	CSimpleIniA ini;
	EXPECT_EQ(SI_OK, ini.LoadFile(fp));
	fclose(fp);
	EXPECT_TRUE(ini.IsEmpty());
}

} // namespace upstreamsync
