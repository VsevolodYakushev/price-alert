@echo off
rem Price Alert: build if needed, start the server, open the browser.
cd /d "%~dp0"

if not exist PriceAlert.exe (
    call build.bat
    if errorlevel 1 goto failed
)

echo.
echo Starting Price Alert... the browser will open http://localhost:8080
echo Close this window to stop the server.
echo.
PriceAlert.exe
if errorlevel 1 goto failed
exit /b 0

:failed
echo.
echo [!] Something went wrong. Copy the text above and send it.
pause
exit /b 1
