@echo off
rem Build PriceAlert.exe. Compiler lookup order:
rem   1) g++ already in PATH
rem   2) portable w64devkit in tools\ (or ..\tools\)
rem   3) download w64devkit into tools\ and unpack it (one time, ~65 MB)
cd /d "%~dp0"

set "W64_VERSION=2.10.1"
set "W64_URL=https://github.com/skeeto/w64devkit/releases/download/v%W64_VERSION%/w64devkit-x64-%W64_VERSION%.7z.exe"

where g++ >nul 2>nul
if not errorlevel 1 goto build

for %%D in ("%~dp0tools" "%~dp0..\tools") do (
    if exist "%%~D\w64devkit\bin\g++.exe" (
        set "TOOLS=%%~D"
        goto add_to_path
    )
)

set "TOOLS=%~dp0tools"
set "SFX=%TOOLS%\download\w64devkit.7z.exe"
if exist "%~dp0..\tools\download\w64devkit.7z.exe" set "SFX=%~dp0..\tools\download\w64devkit.7z.exe"
if not exist "%SFX%" (
    echo Downloading the portable compiler w64devkit %W64_VERSION% ^(one time, ~65 MB^)...
    if not exist "%TOOLS%\download" mkdir "%TOOLS%\download"
    curl -L --fail -o "%SFX%" "%W64_URL%"
    if errorlevel 1 (
        echo [!] Download failed. Get it manually: %W64_URL%
        echo     and save it as "%SFX%"
        exit /b 1
    )
)
echo Unpacking w64devkit (one time only)...
start "" /wait "%SFX%" -y -o"%TOOLS%"
if not exist "%TOOLS%\w64devkit\bin\g++.exe" (
    echo [!] Unpacking failed. Run "%SFX%" manually and extract it into "%TOOLS%".
    exit /b 1
)

:add_to_path
set "PATH=%TOOLS%\w64devkit\bin;%PATH%"

:build
echo Building PriceAlert.exe (about 30 seconds)...
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0A00 -I. ^
    web/server.cpp mock/MockAuthService.cpp mock/MockPortfolioService.cpp ^
    -o PriceAlert.exe -static -lws2_32 -lshell32
if errorlevel 1 (
    echo [!] Build failed.
    exit /b 1
)
echo Done: PriceAlert.exe
exit /b 0
