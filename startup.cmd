@echo off
title SolarWallpaper Setup
setlocal EnableDelayedExpansion

set "APPNAME=SolarWallpaper"
set "REGKEY=HKCU\Software\Microsoft\Windows\CurrentVersion\Run"
set "CONFIG=config.ini"
set "EXE=solarwallpaper.exe"

:menu
cls
echo ================================
echo        SolarWallpaper Setup
echo ================================
echo.

:: Startup status
reg query "%REGKEY%" /v "%APPNAME%" >nul 2>&1
if !errorlevel!==0 (
    set "STATUS=Enabled"
) else (
    set "STATUS=Disabled"
)

echo Startup status : !STATUS!
echo.

:: Process status
tasklist /FI "IMAGENAME eq %EXE%" | find /I "%EXE%" >nul
if !errorlevel!==0 (
    echo Process status : Running
) else (
    echo Process status : Not running
)

echo.

:: Config status
if exist "%CONFIG%" (
    echo Current location:
    type "%CONFIG%"
) else (
    echo Location not configured
)

echo.
echo --------------------------------
echo 1. Set location
echo 2. Enable startup
echo 3. Disable startup
echo 4. Start now
echo 5. Stop now
echo 6. Exit
echo --------------------------------
echo.

set /p choice=Select option:

if "%choice%"=="1" goto setloc
if "%choice%"=="2" goto enable
if "%choice%"=="3" goto disable
if "%choice%"=="4" goto startnow
if "%choice%"=="5" goto stopnow
if "%choice%"=="6" exit

goto menu


:setloc
cls
echo ================================
echo          Set Location
echo ================================
echo.
echo 1. Manual input
echo 2. Auto detect (IP)
echo 3. Back
echo.

set /p locchoice=Select option:

if "%locchoice%"=="1" goto manual
if "%locchoice%"=="2" goto autoip
if "%locchoice%"=="3" goto menu

goto setloc


:manual
cls
echo Manual location input
echo.

set "lat="
set "lon="

set /p lat=Latitude  :
set /p lon=Longitude :

if "!lat!"=="" (
    echo Latitude cannot be empty
    pause
    goto setloc
)

if "!lon!"=="" (
    echo Longitude cannot be empty
    pause
    goto setloc
)

goto saveloc


:autoip
cls
echo Detecting location via IP...
echo.

where curl >nul 2>nul
if errorlevel 1 (
    echo ERROR: curl not found.
    echo Requires Windows 10 or newer.
    pause
    goto setloc
)

curl -s http://ip-api.com/csv/?fields=lat,lon > temp_loc.txt

if not exist temp_loc.txt (
    echo Detection failed.
    pause
    goto setloc
)

set /p line=<temp_loc.txt
del temp_loc.txt

for /f "tokens=1,2 delims=," %%A in ("!line!") do (
    set lat=%%A
    set lon=%%B
)

if "!lat!"=="" (
    echo Failed to detect location.
    pause
    goto setloc
)

echo Detected:
echo Latitude  : !lat!
echo Longitude : !lon!
echo.

pause

goto saveloc


:saveloc
(
echo [location]
echo lat=!lat!
echo lon=!lon!
) > "%CONFIG%"

echo.
echo Location saved successfully.
echo Latitude  : !lat!
echo Longitude : !lon!
echo.

pause
goto menu


:enable
reg add "%REGKEY%" ^
/v "%APPNAME%" ^
/t REG_SZ ^
/d "%cd%\%EXE%" ^
/f >nul

echo.
echo Startup enabled successfully.
pause
goto menu


:disable
reg delete "%REGKEY%" ^
/v "%APPNAME%" ^
/f >nul 2>&1

echo.
echo Startup disabled successfully.
pause
goto menu


:startnow
cls
tasklist /FI "IMAGENAME eq %EXE%" | find /I "%EXE%" >nul

if !errorlevel!==0 (
    echo SolarWallpaper is already running.
) else (
    start "" "%cd%\%EXE%"
    echo SolarWallpaper started successfully.
)

pause
goto menu


:stopnow
cls
taskkill /IM "%EXE%" /F >nul 2>&1

if !errorlevel!==0 (
    echo SolarWallpaper stopped successfully.
) else (
    echo SolarWallpaper is not running.
)

pause
goto menu
