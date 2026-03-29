@echo off
echo ================================================================
echo   Magic Mouse Utility — Build
echo ================================================================
echo.

:: Resolve dependencies
echo [1/3] Resolving dependencies...
go mod tidy
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: go mod tidy failed.
    exit /b 1
)

:: Vet
echo [2/3] Running go vet...
go vet ./...
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: go vet found issues.
    exit /b 1
)

:: Build Windows executable (no console window)
echo [3/3] Building MagicMouseUtil.exe...
go build -ldflags="-H windowsgui -s -w" -o MagicMouseUtil.exe ./cmd/main.go
if %ERRORLEVEL% == 0 (
    echo.
    echo ================================================================
    echo   Build successful!
    echo   Run: MagicMouseUtil.exe
    echo   Dashboard: http://localhost:7878
    echo   Flags: --version, --no-tray, --port :XXXX, --no-browser
    echo ================================================================
) else (
    echo.
    echo Build FAILED. Check errors above.
    exit /b 1
)
