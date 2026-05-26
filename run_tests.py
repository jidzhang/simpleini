import subprocess
import os
import sys
import time
import shutil

REPO_DIR = os.path.dirname(os.path.abspath(__file__))

def run_cmd(cmd, cwd=None, env=None):
    """Run a command and return (returncode, stdout, stderr)."""
    result = subprocess.run(
        cmd, cwd=cwd, env=env,
        capture_output=True, text=True,
        encoding='gbk', errors='replace'
    )
    return result.returncode, result.stdout, result.stderr

def find_in_path(exe_name):
    """Find an executable in PATH."""
    for path_dir in os.environ.get('PATH', '').split(';'):
        path_dir = path_dir.strip()
        if not path_dir:
            continue
        candidate = os.path.join(path_dir, exe_name)
        if os.path.exists(candidate):
            return candidate
    return None

def get_vs_env(vcvars_path, arch):
    """Run vcvarsall.bat and capture the resulting environment.

    Starts from a minimal system environment so the result is always
    clean, regardless of the parent shell's state.
    """
    tmp_bat = os.path.join(REPO_DIR, '_vs_env_tmp.bat')
    sysroot = os.environ.get('SYSTEMROOT', r'C:\Windows')
    clean_env = {
        'SYSTEMROOT': sysroot,
        'SYSTEMDRIVE': os.environ.get('SYSTEMDRIVE', 'C:'),
        'COMSPEC': os.path.join(sysroot, 'system32', 'cmd.exe'),
        'TEMP': os.environ.get('TEMP', ''),
        'TMP': os.environ.get('TMP', ''),
        'PATH': ';'.join([
            sysroot + r'\system32',
            sysroot + r'\system32\Wbem',
        ]),
    }
    # Preserve VS install hints that vcvarsall.bat scripts rely on
    for key in os.environ:
        if key.startswith('VS') and key.endswith('COMNTOOLS'):
            clean_env[key] = os.environ[key]
    with open(tmp_bat, 'w') as f:
        f.write('@echo off\n')
        f.write('call "' + vcvars_path + '" ' + arch + ' >nul 2>&1\n')
        f.write('set\n')
    result = subprocess.run(
        ['cmd', '/C', tmp_bat],
        capture_output=True, text=True, encoding='gbk', errors='replace',
        env=clean_env
    )
    if os.path.exists(tmp_bat):
        os.remove(tmp_bat)
    if result.returncode != 0:
        return None
    env = {}
    for line in result.stdout.splitlines():
        if '=' in line:
            key, _, value = line.partition('=')
            env[key] = value
    return env

def _vs_paths_from_comntools(comntools_var):
    """Derive VS install paths from VSxxCOMNTOOLS environment variable.

    Returns dict with keys: vcvars, vs_base, ide_dir, or empty dict if not set.
    """
    comntools = os.environ.get(comntools_var)
    if not comntools:
        return {}
    comntools = comntools.rstrip('\\')
    vs_base = os.path.normpath(os.path.join(comntools, '..', '..'))
    ide_dir = os.path.normpath(os.path.join(comntools, '..', 'IDE'))
    vcvars = os.path.join(vs_base, 'VC', 'vcvarsall.bat')
    return {'vcvars': vcvars, 'vs_base': vs_base, 'ide_dir': ide_dir}

def _vs_paths_from_vswhere():
    """Find VS2017+ install path via vswhere.exe.

    Returns dict with key: vcvars, or empty dict if not found.
    """
    vswhere = os.path.join(
        os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
        'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if not os.path.exists(vswhere):
        return {}
    rc, out, _ = run_cmd(
        [vswhere, '-latest', '-property', 'installationPath', '-version', '[16,18)'])
    if rc != 0 or not out.strip():
        return {}
    install_path = out.strip().splitlines()[0]
    vcvars = os.path.join(install_path, 'VC', 'Auxiliary', 'Build', 'vcvarsall.bat')
    return {'vcvars': vcvars}

def rmtree_retry(path, retries=3, delay=1):
    """Remove a directory tree with retries for locked files on Windows."""
    for i in range(retries):
        try:
            shutil.rmtree(path)
            return True
        except Exception:
            if i < retries - 1:
                time.sleep(delay)
    try:
        alt = path + '_old_' + str(int(time.time()))
        os.rename(path, alt)
        return True
    except Exception:
        return False

def _find_cl_in_env(env):
    """Find cl.exe in the PATH of a captured environment."""
    for path_dir in env.get('PATH', '').split(';'):
        path_dir = path_dir.strip()
        if not path_dir:
            continue
        candidate = os.path.join(path_dir, 'cl.exe')
        if os.path.exists(candidate):
            return candidate
    return None

# ---------------------------------------------------------------
# MSVC test (shared by VS2005/VS2019, x86/x64)
# ---------------------------------------------------------------

def _test_msvc(label, vcvars_path, arch, extra_flags, fallback_cl,
               ide_dir=None, out_suffix=None):
    """Compile and run lightweight tests with an MSVC compiler."""
    print("=" * 60)
    print(f"[{label}]")
    print("=" * 60)

    if not os.path.exists(vcvars_path):
        print(f"SKIP: {label} vcvarsall.bat not found")
        return False, "not found"

    env = get_vs_env(vcvars_path, arch)
    if not env:
        print(f"FAIL: Could not setup {label} environment")
        return False, "env setup failed"

    cl = _find_cl_in_env(env)
    if not cl:
        if fallback_cl and os.path.exists(fallback_cl):
            cl = fallback_cl
        else:
            print(f"FAIL: cl.exe not found for {label}")
            return False, "cl not found"

    # Some MSVC versions need debugger DLLs from Common7\IDE
    if ide_dir and os.path.isdir(ide_dir):
        env['PATH'] = ide_dir + ';' + env.get('PATH', '')

    test_dir = os.path.join(REPO_DIR, 'test_lightweight')
    exe_name = f'test_{out_suffix or label.lower().replace(" ", "_")}.exe'
    out_exe = os.path.join(test_dir, exe_name)

    if os.path.exists(out_exe):
        os.remove(out_exe)

    convertutf = os.path.join(REPO_DIR, 'ConvertUTF.c')
    runner = os.path.join(test_dir, 'test_runner.cpp')
    cmd = [cl, '/nologo', '/W3', '/EHsc', '/O2'] + extra_flags + \
          ['/I' + REPO_DIR, runner, convertutf, '/Fe' + out_exe]
    rc, out, err = run_cmd(cmd, env=env)
    if rc != 0:
        print(f"FAIL: Compile error (code {rc})")
        print(out)
        print(err)
        return False, "compile failed"

    rc, out, err = run_cmd([out_exe])
    if rc != 0:
        print(f"FAIL: Runtime error (code {rc})")
        print(out)
        print(err)
        return False, "runtime failed"

    print("PASS")
    print(out)
    return True, "ok"

def test_vs2005_x86():
    vs = _vs_paths_from_comntools('VS80COMNTOOLS')
    if not vs:
        print("=" * 60)
        print("[VS2005 x86]")
        print("=" * 60)
        print("SKIP: VS80COMNTOOLS not set")
        return False, "not found"
    return _test_msvc(
        "VS2005 x86",
        vcvars_path=vs['vcvars'],
        arch='x86',
        extra_flags=[],
        fallback_cl=os.path.join(vs['vs_base'], 'VC', 'bin', 'cl.exe'),
        ide_dir=vs['ide_dir'],
        out_suffix='vs2005_x86',
    )

def test_vs2005_x64():
    vs = _vs_paths_from_comntools('VS80COMNTOOLS')
    if not vs:
        print("=" * 60)
        print("[VS2005 x64]")
        print("=" * 60)
        print("SKIP: VS80COMNTOOLS not set")
        return False, "not found"
    return _test_msvc(
        "VS2005 x64",
        vcvars_path=vs['vcvars'],
        arch='amd64',
        extra_flags=[],
        fallback_cl=os.path.join(vs['vs_base'], 'VC', 'bin', 'amd64', 'cl.exe'),
        ide_dir=vs['ide_dir'],
        out_suffix='vs2005_x64',
    )

def test_vs2019_x86():
    vs = _vs_paths_from_vswhere()
    if not vs:
        print("=" * 60)
        print("[VS2019 x86]")
        print("=" * 60)
        print("SKIP: vswhere.exe not found or no VS2019+ installation")
        return False, "not found"
    return _test_msvc(
        "VS2019 x86",
        vcvars_path=vs['vcvars'],
        arch='x86',
        extra_flags=['/std:c++17'],
        fallback_cl=None,
        out_suffix='vs2019_x86',
    )

def test_vs2019_x64():
    vs = _vs_paths_from_vswhere()
    if not vs:
        print("=" * 60)
        print("[VS2019 x64]")
        print("=" * 60)
        print("SKIP: vswhere.exe not found or no VS2019+ installation")
        return False, "not found"
    return _test_msvc(
        "VS2019 x64",
        vcvars_path=vs['vcvars'],
        arch='x64',
        extra_flags=['/std:c++17'],
        fallback_cl=None,
        out_suffix='vs2019_x64',
    )

# ---------------------------------------------------------------
# MinGW tests
# ---------------------------------------------------------------

def _test_mingw(label, gcc_path, out_suffix, gcc_from_path=False):
    """Compile and run lightweight tests with a MinGW GCC."""
    print("=" * 60)
    print(f"[{label}]")
    print("=" * 60)

    if gcc_from_path:
        gcc = find_in_path('gcc.exe')
        if not gcc:
            print("SKIP: gcc.exe not found in PATH")
            return False, "not found"
    elif gcc_path:
        gcc = gcc_path
        if not os.path.exists(gcc):
            print(f"SKIP: {label} not found at {gcc_path}")
            return False, "not found"
    else:
        print(f"SKIP: {label} path not configured")
        return False, "not found"

    print(f"Using: {gcc}")

    test_dir = os.path.join(REPO_DIR, 'test_lightweight')
    out_exe = os.path.join(test_dir, f'test_{out_suffix}.exe')

    if os.path.exists(out_exe):
        os.remove(out_exe)

    convertutf = os.path.join(REPO_DIR, 'ConvertUTF.c')
    cmd = [gcc, '-x', 'c++', '-Wall', '-O2', '-I' + REPO_DIR,
           os.path.join(test_dir, 'test_runner.cpp'), convertutf,
           '-o', out_exe, '-lstdc++']
    rc, out, err = run_cmd(cmd)
    if rc != 0:
        print(f"FAIL: Compile error (code {rc})")
        print(out)
        print(err)
        return False, "compile failed"

    # Ensure the compiler's bin dir is in PATH for runtime DLLs
    run_env = None
    if not gcc_from_path:
        run_env = dict(os.environ)
        mingw_bin = os.path.dirname(gcc)
        run_env['PATH'] = mingw_bin + ';' + run_env.get('PATH', '')

    rc, out, err = run_cmd([out_exe], env=run_env)
    if rc != 0:
        print(f"FAIL: Runtime error (code {rc})")
        print(out)
        print(err)
        return False, "runtime failed"

    print("PASS")
    print(out)
    return True, "ok"

def test_mingw4():
    mingw4_home = os.environ.get('MINGW4_HOME')
    gcc = os.path.join(mingw4_home, 'bin', 'gcc.exe') if mingw4_home else None
    return _test_mingw(
        "MinGW-4 GCC",
        gcc_path=gcc,
        out_suffix='mingw4',
    )

def test_mingw12():
    return _test_mingw(
        "MinGW-12 GCC",
        gcc_path=None,
        out_suffix='mingw12',
        gcc_from_path=True,
    )

# ---------------------------------------------------------------
# GoogleTest (CMake-based, requires C++17)
# ---------------------------------------------------------------

def test_googletest_mingw12():
    """Run GoogleTest suite with MinGW-12."""
    print("=" * 60)
    print("[GoogleTest with MinGW-12]")
    print("=" * 60)

    build_dir = os.path.join(REPO_DIR, 'build_test')
    if os.path.exists(build_dir):
        if not rmtree_retry(build_dir):
            build_dir = build_dir + '_new'
    os.makedirs(build_dir, exist_ok=True)

    rc, out, err = run_cmd(
        ['cmake', '-S', '..', '-B', '.', '-DBUILD_TESTING=ON',
         '-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++',
         '-G', 'MinGW Makefiles'],
        cwd=build_dir
    )
    if rc != 0:
        print(f"FAIL: CMake configure error (code {rc})")
        print(out)
        print(err)
        return False, "cmake failed"

    rc, out, err = run_cmd(['cmake', '--build', '.'], cwd=build_dir)
    if rc != 0:
        print(f"FAIL: Build error (code {rc})")
        print(out)
        print(err)
        return False, "build failed"

    rc, out, err = run_cmd(['ctest', '--output-on-failure'], cwd=build_dir)
    if rc != 0:
        print(f"FAIL: Tests failed (code {rc})")
        print(out)
        print(err)
        return False, "tests failed"

    print("PASS")
    print(out)
    return True, "ok"

# ---------------------------------------------------------------
# Main
# ---------------------------------------------------------------

def main():
    results = []

    print("\nSimpleIni Cross-Compiler Test Suite")
    print("=" * 60)
    print()

    # Lightweight tests
    print("--- Lightweight Tests ---\n")
    results.append(("VS2005 x86",  test_vs2005_x86()))
    results.append(("VS2005 x64",  test_vs2005_x64()))
    results.append(("VS2019 x86",  test_vs2019_x86()))
    results.append(("VS2019 x64",  test_vs2019_x64()))
    results.append(("MinGW-4 GCC", test_mingw4()))
    results.append(("MinGW-12 GCC", test_mingw12()))

    # GoogleTest (C++17 required)
    print("\n--- GoogleTest ---\n")
    results.append(("GoogleTest", test_googletest_mingw12()))

    # Summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)

    passed = 0
    failed = 0
    skipped = 0

    for name, (ok, msg) in results:
        if ok:
            status = "PASS"
            passed += 1
        elif msg == "not found":
            status = "SKIP"
            skipped += 1
        else:
            status = f"FAIL ({msg})"
            failed += 1
        print(f"  {name:20s} {status}")

    print()
    print(f"Total: {passed} passed, {failed} failed, {skipped} skipped")

    if failed > 0:
        print("\nSOME TESTS FAILED")
        return 1
    else:
        print("\nALL TESTS PASSED")
        return 0

if __name__ == '__main__':
    sys.exit(main())
