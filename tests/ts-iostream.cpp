// Stream loading tests (SI_SUPPORT_IOSTREAMS). The LoadData(std::istream&)
// overload rejects streams with embedded NUL bytes (SI_FAIL) and streams
// larger than SI_MAX_FILE_SIZE (SI_FILE) — synced from upstream d749efc.
#define SI_SUPPORT_IOSTREAMS
#include "../SimpleIni.h"
#include "gtest/gtest.h"

#include <sstream>
#include <string>

TEST(UpstreamSyncStreams, ReadsTextStream) {
	std::istringstream ss("[s]\nk = v\n");
	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData(ss));
	EXPECT_STREQ("v", ini.GetValue("s", "k"));
}

TEST(UpstreamSyncStreams, RejectsEmbeddedNul) {
	// previously the data was silently truncated at the NUL byte and the
	// partial content parsed as if the file ended there
	const char data[] = "a = b\n\0c = d\n";
	const std::string strData(data, sizeof(data) - 1);
	std::istringstream ss(strData);

	CSimpleIniA ini;
	EXPECT_EQ(SI_FAIL, ini.LoadData(ss));
	EXPECT_TRUE(ini.IsEmpty());
}

TEST(UpstreamSyncStreams, ReadsLargeStreamInChunks) {
	// larger than the internal 4096-byte read buffer
	std::string strData = "[s]\nk = ";
	strData.append(10000, 'x');
	strData.append("\n");
	std::istringstream ss(strData);

	CSimpleIniA ini;
	ASSERT_EQ(SI_OK, ini.LoadData(ss));
	const char* value = ini.GetValue("s", "k");
	ASSERT_NE(static_cast<const char*>(NULL), value);
	EXPECT_EQ(10000u, strlen(value));
}
