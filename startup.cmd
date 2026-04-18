@echo off
title SolarWallpaper Setup
setlocal EnableDelayedExpansion

:: Enable ANSI escape sequences
for /f %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"

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

:: Reset choice variable
set "choice=-1"

:: Startup status
reg query "%REGKEY%" /v "%APPNAME%" >nul 2>&1
if !errorlevel!==0 (
    set "STATUS=Enabled"
    echo Startup status : !ESC![32mEnabled!ESC![0m
) else (
    set "STATUS=Disabled"
    echo Startup status : !ESC![31mDisabled!ESC![0m
)

echo.

:: Process status
tasklist /FI "IMAGENAME eq %EXE%" | find /I "%EXE%" >nul
if !errorlevel!==0 (
    echo Process status : !ESC![32mRunning!ESC![0m
) else (
    echo Process status : !ESC![31mNot running!ESC![0m
)

echo.

:: Config status
if exist "%CONFIG%" (
    echo Current location:
    for /f "tokens=1,2 delims==" %%a in ('findstr "lat lon" "%CONFIG%"') do (
        echo %%a = %%b
    )
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
echo 6. Restart
echo 7. Recode Program
echo 8. Exit
echo --------------------------------
echo.

set /p choice=Select option:

if "%choice%"=="1" goto setloc
if "%choice%"=="2" goto enable
if "%choice%"=="3" goto disable
if "%choice%"=="4" goto startnow
if "%choice%"=="5" goto stopnow
if "%choice%"=="6" goto restart
if "%choice%"=="7" goto recode
if "%choice%"=="8" exit

goto menu


:setloc
cls
echo ================================
echo          Set Location
echo ================================
echo.
echo 1. Manual input
echo 2. Auto detect (IP)
echo 3. GPS detect
echo 4. Back
echo.

set /p locchoice=Select option:

if "%locchoice%"=="1" goto manual
if "%locchoice%"=="2" goto autoip
if "%locchoice%"=="3" goto gpsdetect
if "%locchoice%"=="4" goto menu

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


:gpsdetect
cls
echo Detecting location via GPS...
echo.

:: 尝试获取GPS位置，最多尝试3次
set "attempt=0"
set "valid=false"

:gps_retry
set /a "attempt+=1"

if !attempt! gtr 3 goto gps_fail

echo Attempt !attempt! of 3...

:: 使用PowerShell调用Windows GPS API
PowerShell -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'SilentlyContinue'; try { Add-Type -AssemblyName System.Device; $locator = New-Object System.Device.Location.GeoCoordinateWatcher; $locator.Start(); $timeout = 10; $startTime = Get-Date; while ($locator.Status -ne 'Ready' -and ((Get-Date) - $startTime).TotalSeconds -lt $timeout) { Start-Sleep -Milliseconds 500 }; if ($locator.Status -eq 'Ready') { $position = $locator.Position; if ($position.Location.IsUnknown -eq $false) { $lat = $position.Location.Latitude; $lon = $position.Location.Longitude; Write-Output \"$lat,$lon\" } else { Write-Output \"0,0\" } } else { Write-Output \"0,0\" }; $locator.Stop() } catch { Write-Output \"0,0\" }" > temp_gps.txt

:: 读取GPS结果
if not exist temp_gps.txt (
    echo Failed to get GPS data.
    goto gps_retry
)

set /p gps_result=<temp_gps.txt
del temp_gps.txt

:: 解析结果
for /f "tokens=1,2 delims=," %%A in ("!gps_result!") do (
    set lat=%%A
    set lon=%%B
)

:: 检查是否为有效值
if "!lat!"=="0" if "!lon!"=="0" goto gps_retry
if "!lat!"=="NaN" if "!lon!"=="NaN" goto gps_retry

:: 检查是否为数字
set "valid=true"
for /f "delims=.0123456789" %%i in ("!lat!") do set "valid=false"
for /f "delims=.0123456789" %%i in ("!lon!") do set "valid=false"

if "!valid!"=="false" goto gps_retry

:: 显示获取到的位置
echo GPS location detected:
echo Latitude  : !lat!
echo Longitude : !lon!
echo.
pause
goto saveloc

:gps_fail
cls
echo Failed to get valid GPS location after 3 attempts.
echo config.ini will not be updated.
echo.
pause
goto setloc

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
cls
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
cls
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
    pushd "%~dp0"
    start "" "%EXE%"
    popd
    if !errorlevel!==0 (
        echo SolarWallpaper started successfully.
    ) else (
        echo Failed to start SolarWallpaper.
    )
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


:restart
cls
echo Restarting SolarWallpaper...
echo.

:: Stop first
taskkill /IM "%EXE%" /F >nul 2>&1

:: Wait a moment
ping 127.0.0.1 -n 2 >nul

:: Start now
pushd "%~dp0"
start "" "%EXE%"
popd

if !errorlevel!==0 (
    echo SolarWallpaper restarted successfully.
) else (
    echo Failed to restart SolarWallpaper.
)

pause
goto menu

:recode
cls
echo Recoding SolarWallpaper...
echo.

if exist "coding.cmd" (
    call "coding.cmd"
) else (
    echo ERROR: coding.cmd not found.
    pause
    goto menu
)

pause
goto menu
