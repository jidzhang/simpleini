# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fork of [simpleini](https://github.com/brofield/simpleini) (`https://github.com/jidzhang/simpleini`) — a cross-platform, header-only C++ INI file parser. This fork maintains **C++03 compatibility** (minimum VS2005) while adding UTF-8 encoding fixes for Win32.

## C++ Standard

- **Production code** (`SimpleIni.h`): **C++03 only**. No `constexpr`, `using` aliases, `auto`, range-for, `std::next`, or other C++11+ features. Must compile with VS2005.
- **Test code** (`tests/`, `test_lightweight/`): C++11/C++17 allowed.

## Architecture

The entire library lives in `SimpleIni.h` (~3700 lines) using a **policy-based template design**:

```
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>
```

- **`SI_CHAR`**: `char` or `wchar_t`
- **`SI_STRLESS`**: Comparison functor — `SI_NoCase` (case-insensitive, default) or `SI_Case` (case-sensitive)
- **`SI_CONVERTER`**: Encoding converter — `SI_ConvertA` (no conversion), `SI_ConvertW` (Win32/Generic/ICU, conditionally compiled)

Convenience typedefs: `CSimpleIniA` (char, case-insensitive), `CSimpleIniCaseA` (char, case-sensitive), `CSimpleIniW` (wchar_t).

Internal storage: `TSection` → `std::map` of section names → `TKeyVal` → `std::multimap` of key entries → values. All string pointers reference into a single `m_pData` buffer.

## Encoding Handling

- On Win32, `SI_NoCase` uses `_mbsicmp` for MBCS mode (system code page dependent)
- In UTF-8 mode (`SetUnicode(true)`), `SI_NoCase` uses byte-by-byte ASCII-only case folding to avoid `_mbsicmp` corrupting multi-byte sequences
- `KeyOrder`/`LoadOrder` functors receive a `const bool*` pointer to `m_bStoreIsUtf8` so `SetUnicode()` after construction takes effect

## Build & Test

### GoogleTest suite (C++17 required)
```bash
cmake -S . -B build -DBUILD_TESTING=ON -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

### Lightweight test suite (C++03, all compilers)
```bash
python run_tests.py
```
Tests VS2005 x86/x64, VS2019 x86/x64, MinGW-4, MinGW-12, and GoogleTest. Each compiler environment is fully isolated via `vcvarsall.bat` capture.

### GCC/Clang test suite (macOS/Linux)
```bash
CXX=g++ bash run_tests_unix.sh
CXX=clang++ bash run_tests_unix.sh
```
Tests the specified compiler in C++03 and C++17 modes. Runs automatically in CI on Ubuntu (GCC + Clang) and macOS (Clang).

### Single compiler quick test
```bash
gcc -x c++ -Wall -O2 -I. test_lightweight/test_runner.cpp -o test.exe -lstdc++ && ./test.exe
```

## Key Files

| File | Role |
|------|------|
| `SimpleIni.h` | Entire library |
| `SI_UTF8` namespace (in `SimpleIni.h`) | Inline locale-independent UTF-8 codec used by `SI_CONVERT_GENERIC`; no extra source files |
| `run_tests.py` | Cross-compiler test runner (Windows) |
| `run_tests_unix.sh` | GCC/Clang test runner (macOS/Linux) |
| `test_lightweight/` | C++03-compatible tests (no dependencies) |
| `tests/` | GoogleTest suite (C++17) |
