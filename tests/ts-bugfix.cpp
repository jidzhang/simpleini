#include "../SimpleIni.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

TEST(TestBugFix, TestEmptySection) {
	CSimpleIniA ini;
	ini.SetValue("foo", "skey", "sval");
	ini.SetValue("", "rkey", "rval");
	ini.SetValue("bar", "skey", "sval");

	std::string output;
	ini.Save(output);

	std::string expected =
		"rkey = rval\n"
		"\n"
		"\n"
		"[foo]\n"
		"skey = sval\n"
		"\n"
		"\n"
		"[bar]\n"
		"skey = sval\n";

	output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
	ASSERT_STREQ(expected.c_str(), output.c_str());
}

TEST(TestBugFix, TestMultiLineIgnoreTrailSpace0) {
	std::string input =
		"; multiline values\n"
		"key = <<<EOS\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"EOS\n"
		"\n"
		"[section]\n";

	bool multiline = true;
	CSimpleIniA ini(true, false, multiline);

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	std::string output;
	ini.Save(output);

	std::string expected =
		"; multiline values\n"
		"\n"
		"\n"
		"key = <<<END_OF_TEXT\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"END_OF_TEXT\n"
		"\n"
		"\n"
		"[section]\n";

	output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
	ASSERT_STREQ(expected.c_str(), output.c_str());
}

TEST(TestBugFix, TestMultiLineIgnoreTrailSpace1) {
	std::string input =
		"; multiline values\n"
		"key = <<<EOS\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"EOS \n"
		"\n"
		"[section]\n";

	bool multiline = true;
	CSimpleIniA ini(true, false, multiline);

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	std::string output;
	ini.Save(output);

	std::string expected =
		"; multiline values\n"
		"\n"
		"\n"
		"key = <<<END_OF_TEXT\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"END_OF_TEXT\n"
		"\n"
		"\n"
		"[section]\n";

	output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
	ASSERT_STREQ(expected.c_str(), output.c_str());
}

TEST(TestBugFix, TestMultiLineIgnoreTrailSpace2) {
	std::string input =
		"; multiline values\n"
		"key = <<<EOS\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"EOS  \n"
		"\n"
		"[section]\n";

	bool multiline = true;
	CSimpleIniA ini(true, false, multiline);

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	std::string output;
	ini.Save(output);

	std::string expected =
		"; multiline values\n"
		"\n"
		"\n"
		"key = <<<END_OF_TEXT\n"
		"This is a\n"
		"multiline value\n"
		"and it ends.\n"
		"END_OF_TEXT\n"
		"\n"
		"\n"
		"[section]\n";

	output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
	ASSERT_STREQ(expected.c_str(), output.c_str());
}

// ============================================================
// Regression tests for bugs found during code review
// ============================================================

// Bug 1: SetAllowKeyOnly(true) mode - a key-only entry following a normal
// key=value entry used to inherit the previous entry's value as a stale
// pointer (FindEntry never reset a_pVal in the "no value" branch).
TEST(TestBugFix, TestKeyOnlyDoesNotInheritPreviousValue) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetAllowKeyOnly(true);

	std::string input =
		"[section]\n"
		"key1 = value1\n"
		"key2\n"          // key-only; must NOT pick up "value1"
		"key3 = value3\n"
		"key4\n";         // key-only; must NOT pick up "value3"

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	// key1/key3 must keep their real values
	ASSERT_STREQ(ini.GetValue("section", "key1"), "value1");
	ASSERT_STREQ(ini.GetValue("section", "key3"), "value3");

	// key2/key4 must be present but empty (NULL becomes m_cEmptyString)
	const char* v2 = ini.GetValue("section", "key2");
	const char* v4 = ini.GetValue("section", "key4");
	ASSERT_NE(v2, nullptr);
	ASSERT_NE(v4, nullptr);
	ASSERT_STREQ(v2, "");
	ASSERT_STREQ(v4, "");
}

// Bug 1 variant: key-only as the very first entry after a section header.
// Before the fix, pVal was uninitialized (NULL) and AddEntry would map it
// to m_cEmptyString by accident - which happened to look correct. Lock the
// behaviour anyway so it cannot regress.
TEST(TestBugFix, TestKeyOnlyAsFirstEntryInSection) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetAllowKeyOnly(true);

	std::string input =
		"[section]\n"
		"keyonly\n";

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	const char* v = ini.GetValue("section", "keyonly");
	ASSERT_NE(v, nullptr);
	ASSERT_STREQ(v, "");
}

// Bug 1 variant: key-only after a multi-line value. Before the fix, the
// key-only entry would inherit the entire multi-line content.
TEST(TestBugFix, TestKeyOnlyAfterMultilineValue) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiLine(true);
	ini.SetAllowKeyOnly(true);

	std::string input =
		"[section]\n"
		"k1 = <<<END\n"
		"line1\n"
		"line2\n"
		"END\n"
		"keyonly\n";

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	ASSERT_STREQ(ini.GetValue("section", "k1"), "line1\nline2");
	const char* v = ini.GetValue("section", "keyonly");
	ASSERT_NE(v, nullptr);
	ASSERT_STREQ(v, "");
}

// Bug 1 variant: key-only after a comment. The comment is loaded via
// LoadMultiLineText into a_pComment, but a_pVal is still untouched.
TEST(TestBugFix, TestKeyOnlyAfterComment) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetAllowKeyOnly(true);

	std::string input =
		"[section]\n"
		"k1 = value1\n"
		"; this is a comment\n"
		"; continued comment\n"
		"keyonly\n";

	SI_Error rc = ini.LoadData(input);
	ASSERT_EQ(rc, SI_OK);

	ASSERT_STREQ(ini.GetValue("section", "k1"), "value1");
	const char* v = ini.GetValue("section", "keyonly");
	ASSERT_NE(v, nullptr);
	ASSERT_STREQ(v, "");
}

// Bug 6: Reset() used to leave m_nOrder at its previous value, so sections
// added after Reset got inflated order numbers (and could in principle
// overflow int after many cycles). Verify the order counter is reset.
TEST(TestBugFix, TestResetResetsOrderCounter) {
	CSimpleIniA ini;
	ini.SetUnicode();

	// Bump the order counter with several SetValues
	for (int i = 0; i < 5; ++i) {
		ini.SetValue("section", std::to_string(i).c_str(), "v");
	}
	ini.Reset();

	// After Reset+reload, the section load order must start fresh from small
	// values, not continue from the pre-reset counter.
	ASSERT_EQ(ini.LoadData("[a]\nx=1\n[b]\ny=2\n[c]\nz=3\n"), SI_OK);

	CSimpleIniA::TNamesDepend secs;
	ini.GetAllSections(secs);
	secs.sort(CSimpleIniA::Entry::LoadOrder());

	ASSERT_EQ(secs.size(), 3u);
	// First section's order must be small (<= 2), proving the counter reset.
	ASSERT_LE(secs.front().nOrder, 2);

	// Verify ordering is preserved.
	std::vector<std::string> names;
	for (const auto& s : secs) names.push_back(s.pItem);
	ASSERT_EQ(names, (std::vector<std::string>{"a", "b", "c"}));
}

// Bug 2: Save() with SetMultiLine(false) on a value containing an embedded
// newline used to silently write the value across multiple lines, corrupting
// the file on reload. It must now return SI_FAIL so the caller can react.
TEST(TestBugFix, TestSaveFailsOnNewlineValueWhenMultilineDisabled) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiLine(false);
	ini.SetValue("s", "k", "line1\nline2\nline3");

	std::string out;
	SI_Error rc = ini.Save(out);
	ASSERT_EQ(rc, SI_FAIL);
}

// Bug 2 (no regression): the same value with SetMultiLine(true) must save OK
// using the <<<END_OF_TEXT heredoc form.
TEST(TestBugFix, TestSaveOkOnNewlineValueWhenMultilineEnabled) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiLine(true);
	ini.SetValue("s", "k", "line1\nline2\nline3");

	std::string out;
	SI_Error rc = ini.Save(out);
	ASSERT_EQ(rc, SI_OK);
	ASSERT_NE(out.find("<<<END_OF_TEXT"), std::string::npos);
	ASSERT_NE(out.find("END_OF_TEXT"), std::string::npos);
}

// Bug 2 (no regression): values without newlines must still save fine when
// SetMultiLine(false), including values that merely have trailing whitespace
// (which is intentionally NOT covered by the new failure path).
TEST(TestBugFix, TestSaveOkOnPlainAndTrailingWsValuesWhenMultilineDisabled) {
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiLine(false);
		ini.SetValue("s", "k", "plain value");
		std::string out;
		ASSERT_EQ(ini.Save(out), SI_OK);
	}
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiLine(false);
		ini.SetValue("s", "k", "trailing space ");
		std::string out;
		ASSERT_EQ(ini.Save(out), SI_OK);
	}
}
