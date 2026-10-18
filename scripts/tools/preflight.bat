@echo off
setlocal

echo ===================================================
echo [PREFLIGHT] 1/3 Compiling custom firmware...
echo ===================================================
python -c "import sys; sys.path.insert(0, r'firmware'); import build; sys.exit(0 if build.build() else 1)"
if errorlevel 1 (
    echo [FAIL] Firmware compilation failed!
    exit /b 1
)

echo.
echo ===================================================
echo [PREFLIGHT] 2/3 Verifying segments and headers...
echo ===================================================
python scripts/tools/verify_audit.py
if errorlevel 1 (
    echo [FAIL] Segment verification failed!
    exit /b 1
)

echo.
echo ===================================================
echo [PREFLIGHT] 3/3 Checking git index for build leaks...
echo ===================================================
for /f %%i in ('git ls-files firmware/build 2^>nul ^| find /c /v ""') do set LEAK_COUNT=%%i
if not "%LEAK_COUNT%"=="0" (
    echo [FAIL] Build artifacts detected in git index! Count=%LEAK_COUNT%
    exit /b 1
)

echo [OK] All preflight checks passed successfully!
exit /b 0
