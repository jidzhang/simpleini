# simpleini

![Latest Test Results](https://github.com/jidzhang/simpleini/actions/workflows/build-and-test.yml/badge.svg)

A cross-platform library that provides a simple API to read and write INI-style configuration files.
It supports data files in ASCII, MBCS and Unicode, and is designed to be portable to any platform.
Released as open-source and free using the MIT licence.

> **This is a fork of [brofield/simpleini](https://github.com/brofield/simpleini).**
> The public header is kept **strict C++03** so it compiles unchanged on **Visual Studio 2005**
> and other legacy toolchains, while still building cleanly on modern GCC, Clang, and MSVC.
> The library is **single-header** — drop `SimpleIni.h` into your project and `#include` it.
> No CMake, no linker setup, no extra source files, no runtime DLL. The only dependency is the
> C++ standard library.

[Full API documentation](https://brofield.github.io/simpleini/) (doxygen) ·
[Original upstream](https://github.com/brofield/simpleini)

---

## Quick start

```c++
#include "SimpleIni.h"

CSimpleIniA ini;
ini.SetUnicode();                       // save as UTF-8 (recommended)

SI_Error rc = ini.LoadFile("config.ini");
if (rc < 0) { /* handle error */ }

const char* host = ini.GetValue("server", "host", "localhost");
ini.SetValue("server", "host", "0.0.0.0");

ini.SaveFile("config.ini");
```

That is the entire integration story: one header, the C++ standard library, done.

---

## Features

- **MIT licence** — free for all software, including GPL and commercial.
- **Multi-platform** — Windows (95 through 11, including WinCE), Linux, macOS, Unix.
- **Single-header** — no build step required to use the library; `#include "SimpleIni.h"` is enough.
- **Long-term toolchain support** — public header targets strict C++03 and is verified against
  VS2005 (x86/x64), VS2019, MinGW-4, MinGW-12, GCC, and Clang.
- **INI parsing** — any newline format on any platform; tolerant of whitespace around sections,
  keys, and values; supports keys with no section and keys with no value.
- **Comments preserved** at file, section, and key level where possible.
- **Order preserving** — sections and keys are saved in the order they were loaded/added.
- **Multi-line values** (heredoc `<<<END_OF_TEXT`), **multi-key** (duplicate key names), and
  **quoted values** are opt-in.
- **Case-insensitive** section and key matching (ASCII only) — default for `CSimpleIniA`.
- **Type-safe getters/setters** for string, `long`, `double`, and `bool`.
- **UTF-8 and MBCS** file encodings; full Unicode support (CJK, emoji, combining marks, …)
  via the built-in strict UTF-8 codec.
- **`char` and `wchar_t`** programming interfaces (`CSimpleIniA` / `CSimpleIniW`).
- **Pluggable converters** for non-standard character types or encodings.
- **File-size guard** — files above 1 GiB (`SI_MAX_FILE_SIZE`) are rejected to prevent DoS.
- **Compiles with no warnings** on most compilers at the strictest warning level.
- **Not thread-safe** — callers must provide their own locking.

---

## Character encoding

SimpleIni stores everything internally as a `char` byte stream. The on-disk encoding is chosen
at runtime via `SetUnicode()`:

| Mode | API call | Bytes on disk | Portability |
|---|---|---|---|
| **UTF-8** (recommended) | `ini.SetUnicode(true)` | UTF-8 | Locale-independent; identical behaviour on Windows, Linux, and macOS. |
| **MBCS** (legacy) | *(default)* | System code page | Only meaningful on Windows when the file encoding matches the OS locale (GBK on zh-CN, Shift-JIS on ja-JP, etc.). **Not portable across locales or platforms.** |

A leading UTF-8 BOM in the input is detected automatically: it is consumed and forces UTF-8 mode on.

### Recommended usage for non-ASCII content (CJK, emoji, …)

```c++
CSimpleIniA ini;
ini.SetUnicode(true);                   // store as UTF-8
ini.LoadFile("config.ini");             // file must be saved as UTF-8
```

Section names, keys, and values may contain any Unicode text — Chinese, Japanese, Korean,
emoji, combining marks, rare CJK extensions — and round-trip byte-exactly through the built-in
strict UTF-8 codec. The system locale is irrelevant in this mode.

### Strict UTF-8 validation

In UTF-8 mode, malformed byte sequences (overlong encodings, lone continuation bytes, UTF-16
surrogates encoded as UTF-8, impossible bytes, …) cause `LoadData` / `LoadFile` to return
`SI_FAIL` instead of silently substituting `U+FFFD`. This surfaces data problems early. If you
need to ingest legacy byte streams, save them as proper UTF-8 first.

### When MBCS mode is acceptable

- Native Win32 applications where the configuration file is produced and consumed on a single
  machine whose system locale matches the file encoding.
- Backward compatibility with legacy `.ini` files.

Avoid MBCS on Linux/macOS (it relies on `setlocale`) and never mix encodings across machines.

### `wchar_t` interface (`CSimpleIniW`) converter backends

`CSimpleIniW` exposes the same data through `wchar_t`. The compile-time-selected converter
translates between on-disk `char` and in-memory `wchar_t`:

| Macro | Backend | Notes |
|---|---|---|
| `SI_CONVERT_WIN32` | Win32 `MultiByteToWideChar` | Windows default; locale-independent; Windows-only. |
| `SI_CONVERT_GENERIC` | Built-in inline UTF-8 codec | Portable; header-only; no extra source files. **Recommended for cross-platform `CSimpleIniW`.** |
| `SI_CONVERT_ICU` | IBM ICU | Requires ICU headers and `icuuc`. |
| `SI_NO_CONVERSION` | *(none)* | Linux/macOS default; disables the wide interface entirely. |

For portable `CSimpleIniW`, pair `#define SI_CONVERT_GENERIC` (before `#include`) with
`SetUnicode(true)`. No additional files need to be compiled or linked.

---

## API at a glance

The library exposes one class template, `CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>`,
plus convenience typedefs:

| Typedef | Char type | Default comparison |
|---|---|---|
| `CSimpleIniA` | `char` | case-insensitive (ASCII) |
| `CSimpleIniCaseA` | `char` | case-sensitive |
| `CSimpleIniW` | `wchar_t` | case-insensitive |

### Configuration (call before `LoadFile` / `SetValue`)

| Method | Default | Effect |
|---|---|---|
| `SetUnicode(bool = true)` | `false` | Store/load as UTF-8 instead of MBCS. |
| `SetMultiKey(bool = true)` | `false` | Allow duplicate key names in the same section. |
| `SetMultiLine(bool = true)` | `false` | Allow values spanning multiple lines (heredoc). |
| `SetQuotes(bool = true)` | `false` | Parse values wrapped in quotes; preserve surrounding whitespace. |
| `SetSpaces(bool = true)` | `true` | Emit `key = value` instead of `key=value`. |
| `SetAllowKeyOnly(bool = true)` | `false` | Allow lines of the form `key` with no `= value`. |

### I/O

| Method | Description |
|---|---|
| `LoadFile(filename)` / `LoadFile(FILE*)` | Load from disk (char or wide path). |
| `LoadData(const char*, size_t)` / `LoadData(std::string)` | Load from memory. |
| `SaveFile(filename, addSignature)` / `SaveFile(FILE*, …)` | Save to disk. |
| `Save(std::string&)` / `Save(std::ostream&)` | Save to memory/stream. |
| `Reset()` | Discard all data and reset order counters. |

### Access

| Method | Description |
|---|---|
| `GetValue(section, key, default, &hasMulti)` | String value (returns `default` if missing). |
| `GetLongValue` / `GetDoubleValue` / `GetBoolValue` | Typed getters with default fallback. |
| `SetValue(section, key, value, …)` | Insert or update; returns `SI_INSERTED` or `SI_UPDATED`. |
| `SetLongValue` / `SetDoubleValue` / `SetBoolValue` | Typed setters. |
| `GetAllSections` / `GetAllKeys` / `GetAllValues` | Enumerate into `TNamesDepend`. |
| `GetSection(name)` / `GetSectionSize(name)` | Direct section access. |
| `Delete(section, key, deleteSectionIfEmpty)` / `DeleteValue` | Mutations. |

Return codes: `SI_OK (0)`, `SI_UPDATED (1)`, `SI_INSERTED (2)` on success; any value `< 0`
indicates an error (`SI_FAIL`, `SI_NOMEM`, `SI_FILE`). Test with `if (rc < 0)`.

---

## Integration

### Option A: drop the header in (simplest)

Copy `SimpleIni.h` next to your sources and `#include "SimpleIni.h"`. Nothing else to do.

### Option B: CMake subdirectory

```cmake
add_subdirectory(simpleini)
target_link_libraries(your_target PRIVATE SimpleIni::SimpleIni)
```

### Option C: install + `find_package`

```bash
cmake -S . -B build && cmake --build build && sudo cmake --install build
```

```cmake
find_package(SimpleIni REQUIRED)
target_link_libraries(your_target PRIVATE SimpleIni::SimpleIni)
```

---

## Examples

### Load and modify

```c++
CSimpleIniA ini;
ini.SetUnicode();

if (ini.LoadFile("example.ini") < 0) { /* error */ }

ini.SetValue("server", "host", "0.0.0.0");
const char* host = ini.GetValue("server", "host", "localhost");

ini.SaveFile("example2.ini");
```

### Load from a string

```c++
const std::string data = "[section]\nkey = value\n";

CSimpleIniA ini;
ini.SetUnicode();
if (ini.LoadData(data) < 0) { /* error */ }
```

### Enumerate sections and keys

```c++
CSimpleIniA::TNamesDepend sections;
ini.GetAllSections(sections);

CSimpleIniA::TNamesDepend keys;
ini.GetAllKeys("section1", keys);
```

### Multi-value keys

```c++
bool hasMulti = false;
const char* v = ini.GetValue("section1", "key1", nullptr, &hasMulti);

CSimpleIniA::TNamesDepend values;
ini.GetAllValues("section1", "key1", values);
values.sort(CSimpleIniA::Entry::LoadOrder());   // original load order

for (CSimpleIniA::TNamesDepend::const_iterator it = values.begin();
     it != values.end(); ++it) {
    printf("value = '%s'\n", it->pItem);
}
```

### Insert, update, delete

```c++
// Adding a key creates the section if needed.
// Return value distinguishes insert vs update.
SI_Error rc = ini.SetValue("section2", "key1", "value1");
// rc == SI_INSERTED

rc = ini.SetValue("section2", "key1", "value2");
// rc == SI_UPDATED

// Delete a single key, optionally removing the section if it becomes empty.
ini.Delete("section1", "key1", /*deleteSectionIfEmpty=*/true);

// Delete an entire section.
ini.Delete("section2", nullptr);
```

---

## Build and test (for maintainers)

The library itself needs no build step. The test suites use CMake + GoogleTest:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

To build without tests:

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build
```

### Two-tier test suites

| Suite | Language | Purpose |
|---|---|---|
| `tests/` (GoogleTest, C++17) | C++17 | Full behavioural coverage, including the upstream differential UTF-8 test against `c32rtomb` / `mbrtoc32` / `iconv` over all assigned Unicode scalars. |
| `test_lightweight/` (C++03) | C++03 | Runs on VS2005/2008/2019, MinGW-4/12, GCC, Clang. Guarantees the public header stays usable on the oldest supported toolchains. |

Run the lightweight suite directly:

```bash
CXX=g++     bash run_tests_unix.sh    # or CXX=clang++
python run_tests.py                   # Windows: VS + MinGW matrix
```

---

## Licence

MIT. See [LICENCE.txt](LICENCE.txt) for the full text. Copyright (c) 2006-2024, Brodie Thiesfield.
