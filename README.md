# simpleini

![Latest Test Results](https://github.com/brofield/simpleini/actions/workflows/build-and-test.yml/badge.svg)

A cross-platform library that provides a simple API to read and write INI-style configuration files. It supports data files in ASCII, MBCS and Unicode. It is designed explicitly to be portable to any platform and has been tested on Windows, WinCE and Linux. Released as open-source and free using the MIT licence.

**This fork of [brofield/simpleini](https://github.com/brofield/simpleini) keeps two guarantees that matter for production deployment:**

- **Single header file.** Drop `SimpleIni.h` into your project and `#include` it. No CMake, no linker setup, no extra source files, no runtime DLL. The only dependency is the C++ standard library.
- **Visual Studio 2005 compatibility.** The production header is strict C++03 (no `auto`, `constexpr`, `nullptr`, range-for, etc.). This makes it usable on legacy toolchains that are still common in embedded and industrial contexts (VS2005/2008, MinGW), while still compiling cleanly on modern GCC, Clang, and MSVC.

[Full documentation](https://brofield.github.io/simpleini/)

# Feature Summary

- MIT Licence allows free use in all software (including GPL and commercial)
- multi-platform: Windows (from 95 to 11, CE), Linux, MacOS
- loading and saving of INI-style configuration files
- configuration files can have any newline format on all platforms
- liberal acceptance of file format
  - key/values with no section, keys with no value
  - removal of whitespace around sections, keys and values
- support for multi-line values (values with embedded newline characters)
- optional support for multiple keys with the same name
- optional case-insensitive sections and keys (for ASCII characters only)
- saves files with sections and keys in the same order as they were loaded
- preserves comments on the file, section and keys where possible
- supports both char or wchar_t programming interfaces
- supports both MBCS (system locale) and UTF-8 file encodings
- supports ICU as conversion library on all platforms
- system locale does not need to be UTF-8 on Linux/Unix to load UTF-8 file
- support for non-ASCII characters in section, keys, values and comments
- support for non-standard character types or file encodings via user-written converter classes
- support for adding/modifying values programmatically
- should compile with no warnings in most compilers
- **single-header, standard-library-only** deployment: no build system or linker setup needed
- **long-term toolchain support**: the public header targets strict C++03 and is verified against VS2005 (x86/x64), VS2019, MinGW-4, MinGW-12, GCC and Clang

# Documentation

Full documentation of the interface is available in doxygen format. See [latest documentation here](https://brofield.github.io/simpleini/).

# Installation

SimpleIni is a header-only library. No building is required to use it in your project.

Simply include `SimpleIni.h` in your source files:

```c++
#include "SimpleIni.h"
```

That's it! The library is ready to use.

# Character Encoding

SimpleIni stores all data internally as `char` bytes. Two storage encodings are supported, selected at runtime via `SetUnicode()`:

| Mode | API call | Bytes on disk | Behaviour |
|---|---|---|---|
| **UTF-8** (recommended) | `ini.SetUnicode(true)` | UTF-8 | Locale-independent, fully portable, supports the entire Unicode range (CJK, emoji, combining marks, etc.) |
| **MBCS** (legacy)      | *(default, no call needed)* | Current system code page | Works on Windows when the file's encoding matches the OS locale (e.g. GBK on zh-CN, Shift-JIS on ja-JP); **not portable across locales or platforms** |

A leading UTF-8 BOM in the input is auto-detected: it is consumed and forces UTF-8 mode on.

## Recommended usage for non-ASCII content (CJK, etc.)

```c++
CSimpleIniA ini;
ini.SetUnicode(true);          // store as UTF-8
ini.LoadFile("config.ini");   // file must be saved as UTF-8
```

This combination is locale-independent and behaves identically on Windows, Linux, and macOS. Section names, keys, and values may contain any Unicode text (Chinese, Japanese, Korean, emoji, rare CJK extensions, ...). Round-trip is byte-exact via the built-in strict UTF-8 codec.

## When MBCS mode is acceptable

- Native Win32 applications where the configuration file is produced and consumed on a single machine whose system locale matches the file's encoding (e.g. a GBK file on a Simplified Chinese Windows).
- Backward compatibility with legacy `.ini` files.

Avoid MBCS mode on Linux/macOS (it relies on `setlocale`) and never mix encodings across machines.

## Strict UTF-8 validation

In UTF-8 mode, malformed byte sequences (overlong encodings, lone continuation bytes, UTF-16 surrogates encoded as UTF-8, etc.) cause `LoadData` / `LoadFile` to return `SI_FAIL` instead of silently substituting `U+FFFD`. This surfaces data problems early. If you need lenient handling of legacy byte streams, save the file as proper UTF-8 first.

## wchar_t interface (`CSimpleIniW`)

`CSimpleIniW` exposes the same data through `wchar_t`. Conversion between on-disk `char` and in-memory `wchar_t` is performed by a converter selected at compile time:

- `SI_CONVERT_WIN32` (Windows default) — uses Win32 `MultiByteToWideChar`, locale-independent, Windows-only.
- `SI_CONVERT_GENERIC` — built-in locale-independent UTF-8 codec in `SimpleIni.h`; portable across all platforms; no extra source files to link.
- `SI_CONVERT_ICU` — uses IBM ICU; requires ICU headers and `icuuc`.
- `SI_NO_CONVERSION` (Linux/macOS default) — disables the wide interface entirely.

For portable `CSimpleIniW` usage, pair it with `SetUnicode(true)` and `SI_CONVERT_GENERIC`.

# Build and Test

While the library itself doesn't require building, you can build and run the test suite using CMake.

```bash
# Configure the project
cmake -S . -B build

# Build the tests (optional)
cmake --build build

# To build without tests
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build

# Run all tests
cd build
ctest --verbose
```

## CMake Integration

To use SimpleIni in your CMake project:

```cmake
# Add SimpleIni as a subdirectory
add_subdirectory(simpleini)

# Link against your target
target_link_libraries(your_target PRIVATE SimpleIni::SimpleIni)
```

Or install it system-wide:

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

Then in your CMake project:

```cmake
find_package(SimpleIni REQUIRED)
target_link_libraries(your_target PRIVATE SimpleIni::SimpleIni)
```

SI_CONVERT_GENERIC uses a built-in, locale-independent UTF-8 converter that is
implemented inline in `SimpleIni.h`. No additional source files need to be
compiled or linked.

# Examples

These snippets are included with the distribution in the automatic tests as ts-snippets.cpp.

### SIMPLE USAGE

```c++
	// simple demonstration

	CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile("example.ini");
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_OK);

	const char* pv;
	pv = ini.GetValue("section", "key", "default");
	ASSERT_STREQ(pv, "value");

	ini.SetValue("section", "key", "newvalue");

	pv = ini.GetValue("section", "key", "default");
	ASSERT_STREQ(pv, "newvalue");
```

### LOADING DATA

```c++
	// load from a data file
	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("example.ini");
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_OK);

	// load from a string
	const std::string example = "[section]\nkey = value\n";
	CSimpleIniA ini;
	SI_Error rc = ini.LoadData(example);
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_OK);
```

### GETTING SECTIONS AND KEYS

```c++
	// get all sections
	CSimpleIniA::TNamesDepend sections;
	ini.GetAllSections(sections);

	// get all keys in a section
	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("section1", keys);
```

### GETTING VALUES

```c++
	// get the value of a key that doesn't exist
	const char* pv;
	pv = ini.GetValue("section1", "key99");
	ASSERT_EQ(pv, nullptr);

	// get the value of a key that does exist
	pv = ini.GetValue("section1", "key1");
	ASSERT_STREQ(pv, "value1");

	// get the value of a key which may have multiple
	// values. If hasMultiple is true, then there are
	// multiple values and just one value has been returned
	bool hasMulti;
	pv = ini.GetValue("section1", "key1", nullptr, &hasMulti);
	ASSERT_STREQ(pv, "value1");
	ASSERT_EQ(hasMulti, false);

	pv = ini.GetValue("section1", "key2", nullptr, &hasMulti);
	ASSERT_STREQ(pv, "value2.1");
	ASSERT_EQ(hasMulti, true);

	// get all values of a key with multiple values
	CSimpleIniA::TNamesDepend values;
	ini.GetAllValues("section1", "key2", values);

	// sort the values into a known order, in this case we want
	// the original load order
	values.sort(CSimpleIniA::Entry::LoadOrder());

	// output all of the items
	CSimpleIniA::TNamesDepend::const_iterator it;
	for (it = values.begin(); it != values.end(); ++it) {
		printf("value = '%s'\n", it->pItem);
	}
```

### MODIFYING DATA

```c++
	// add a new section
	rc = ini.SetValue("section1", nullptr, nullptr);
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_INSERTED);

	// not an error to add one that already exists
	rc = ini.SetValue("section1", nullptr, nullptr);
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_UPDATED);

	// get the value of a key that doesn't exist
	const char* pv;
	pv = ini.GetValue("section2", "key1", "default-value");
	ASSERT_STREQ(pv, "default-value");

	// adding a key (the section will be added if needed)
	rc = ini.SetValue("section2", "key1", "value1");
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_INSERTED);

	// ensure it is set to expected value
	pv = ini.GetValue("section2", "key1", nullptr);
	ASSERT_STREQ(pv, "value1");

	// change the value of a key
	rc = ini.SetValue("section2", "key1", "value2");
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_UPDATED);

	// ensure it is set to expected value
	pv = ini.GetValue("section2", "key1", nullptr);
	ASSERT_STREQ(pv, "value2");
```

### DELETING DATA

```c++
	// deleting a key from a section. Optionally the entire
	// section may be deleted if it is now empty.
	bool done, deleteSectionIfEmpty = true;
	done = ini.Delete("section1", "key1", deleteSectionIfEmpty);
	ASSERT_EQ(done, true);
	done = ini.Delete("section1", "key1");
	ASSERT_EQ(done, false);

	// deleting an entire section and all keys in it
	done = ini.Delete("section2", nullptr);
	ASSERT_EQ(done, true);
	done = ini.Delete("section2", nullptr);
	ASSERT_EQ(done, false);
```

### SAVING DATA

```c++
	// save the data to a string
	std::string data;
	rc = ini.Save(data);
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_OK);

	// save the data back to the file
	rc = ini.SaveFile("example2.ini");
	if (rc < 0) { /* handle error */ };
	ASSERT_EQ(rc, SI_OK);
```
