#include <cmath>
#include <ctime>
#include <string>
#include <thread>
#include <chrono>

#include <windows.h>
#include <shellapi.h>

using namespace std;

//
// ================= 常量定义 =================
//

const double PI = 3.14159265358979323846;

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT   1001
#define ID_TRAY_RELOAD 1002

//
// ================= 全局变量 =================
//

UINT WM_TASKBARCREATED;
NOTIFYICONDATA nid;

double LAT = 0;
double LON = 0;
double THRESHOLD_NIGHT = -12;
double THRESHOLD_SUNRISE = 10;
double THRESHOLD_MORNING = 35;
double THRESHOLD_DAY = 50;
wstring WALLPAPERS[6];
bool running = true;

//
// ================= 路径工具 =================
//

wstring getExeDir()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return full.substr(0, pos);
}

wstring getConfigPath()
{
    return getExeDir() + L"\\config.ini";
}

//
// ================= 配置文件 =================
//

double readIniDouble(const wchar_t* section, const wchar_t* key)
{
    wchar_t buffer[64];
    GetPrivateProfileStringW(
        section,
        key,
        L"0",
        buffer,
        64,
        getConfigPath().c_str()
    );
    return _wtof(buffer);
}

bool loadConfig()
{
    LAT = readIniDouble(L"location", L"lat");
    LON = readIniDouble(L"location", L"lon");
    
    // Read thresholds from config
    THRESHOLD_NIGHT = readIniDouble(L"thresholds", L"night");
    THRESHOLD_SUNRISE = readIniDouble(L"thresholds", L"sunrise");
    THRESHOLD_MORNING = readIniDouble(L"thresholds", L"morning");
    THRESHOLD_DAY = readIniDouble(L"thresholds", L"day");
    
    // Use default values if config is missing
    if (THRESHOLD_NIGHT == 0) THRESHOLD_NIGHT = -12;
    if (THRESHOLD_SUNRISE == 0) THRESHOLD_SUNRISE = 10;
    if (THRESHOLD_MORNING == 0) THRESHOLD_MORNING = 35;
    if (THRESHOLD_DAY == 0) THRESHOLD_DAY = 50;
    
    return !(LAT == 0 && LON == 0);
}

//
// ================= 数学工具 =================
//

double deg2rad(double d)
{
    return d * PI / 180.0;
}

double rad2deg(double r)
{
    return r * 180.0 / PI;
}

//
// ================= 日期时间 =================
//

int solarOffsetTable[12][3] =
{
    {-5,-9,-12},
    {-14,-13,-11},
    {-11,-8,-5},
    {-2,0,2},
    {3,3,2},
    {1,-1,-3},
    {-5,-6,-6},
    {-5,-3,-1},
    {2,5,9},
    {11,14,15},
    {16,15,12},
    {9,4,0}
};

int getXunIndex(int day)
{
    if (day <= 10) return 0;
    if (day <= 20) return 1;
    return 2;
}

double getSolarOffsetMinutes(int month, int day)
{
    return solarOffsetTable[month][getXunIndex(day)];
}

int getDayOfYear(tm* t)
{
    tm start = *t;
    start.tm_mon  = 0;
    start.tm_mday = 1;
    time_t t1 = mktime(&start);
    time_t t2 = mktime(t);
    return (t2 - t1) / 86400 + 1;
}

//
// ================= 太阳高度角计算 =================
//

double getSolarAltitude(time_t* timePtr = NULL)
{
    time_t now = timePtr ? *timePtr : time(NULL);

    tm local;
    localtime_s(&local, &now);

    int month = local.tm_mon;
    int day   = local.tm_mday;

    int hour = local.tm_hour;
    int min  = local.tm_min;
    int sec  = local.tm_sec;

    int N = getDayOfYear(&local);

    double timeHour = hour + min / 60.0 + sec / 3600.0;
    double longitudeCorrection = (LON - 120.0) * 4.0;
    double offset = getSolarOffsetMinutes(month, day);
    double solarTime = timeHour + longitudeCorrection / 60.0 - offset / 60.0;
    double hourAngle = 15.0 * (solarTime - 12.0);

    double decl = 23.45 * sin(deg2rad(360.0 * (284 + N) / 365.0));

    double latRad  = deg2rad(LAT);
    double declRad = deg2rad(decl);
    double hourRad = deg2rad(hourAngle);

    double altitude = asin(
        sin(latRad) * sin(declRad) +
        cos(latRad) * cos(declRad) * cos(hourRad)
    );

    return rad2deg(altitude);
}

//
// ================= 壁纸管理 =================
//

wstring getCurrentWallpaper()
{
    wchar_t buffer[MAX_PATH];
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buffer, 0);
    return buffer;
}

void setWallpaper(const wchar_t* path)
{
    SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        (PVOID)path,
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
    );
}

wstring getFilename(const wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    return path.substr(pos + 1);
}

//
// ================= 区域判断 =================
//

int getZone(double angle)
{
    if (angle < THRESHOLD_NIGHT) return 5;
    if (angle < THRESHOLD_SUNRISE)  return 1;
    if (angle < THRESHOLD_MORNING)  return 2;
    if (angle < THRESHOLD_DAY)  return 3;
    return 4;
}

//
// ================= 下次切换时间计算 =================
//

wstring calculateNextSwitchTime()
{
    time_t now = time(NULL);
    tm local;
    localtime_s(&local, &now);
    
    double currentAngle = getSolarAltitude();
    int currentZone = getZone(currentAngle);
    
    // 确定下一个区域边界的高度角阈值
    double nextThreshold = 0;
    bool isIncreasing = false;
    
    // 判断太阳高度角是在上升还是下降
    tm testTime = local;
    testTime.tm_min += 10;
    time_t testNow = mktime(&testTime);
    double testAngle = getSolarAltitude(&testNow);
    isIncreasing = (testAngle > currentAngle);
    
    // 根据当前区域和变化趋势确定下一个阈值
    if (isIncreasing)
    {
        switch (currentZone)
        {
        case 1: nextThreshold = THRESHOLD_SUNRISE; break;
        case 2: nextThreshold = THRESHOLD_MORNING; break;
        case 3: nextThreshold = THRESHOLD_DAY; break;
        case 4: return L"No switch today";
        case 5: nextThreshold = THRESHOLD_NIGHT; break;
        }
    }
    else
    {
        switch (currentZone)
        {
        case 1: nextThreshold = THRESHOLD_NIGHT; break;
        case 2: nextThreshold = THRESHOLD_SUNRISE; break;
        case 3: nextThreshold = THRESHOLD_MORNING; break;
        case 4: nextThreshold = THRESHOLD_DAY; break;
        case 5: return L"No switch today";
        }
    }
    
    // 模拟时间流逝，寻找边界时间
    tm searchTime = local;
    time_t searchNow = now;
    
    // 最大搜索范围：24小时
    for (int i = 0; i < 288; i++) // 288个10分钟间隔 = 24小时
    {
        searchTime.tm_min += 10;
        searchNow = mktime(&searchTime);
        localtime_s(&searchTime, &searchNow);
        
        double searchAngle = getSolarAltitude(&searchNow);
        
        if ((isIncreasing && searchAngle >= nextThreshold) || 
            (!isIncreasing && searchAngle <= nextThreshold))
        {
            // 找到大致范围后，缩小到分钟级精度
            tm preciseTime = searchTime;
            preciseTime.tm_min -= 10;
            time_t preciseNow = mktime(&preciseTime);
            
            for (int j = 0; j < 10; j++)
            {
                preciseTime.tm_min += 1;
                preciseNow = mktime(&preciseTime);
                localtime_s(&preciseTime, &preciseNow);
                
                double preciseAngle = getSolarAltitude(&preciseNow);
                
                if ((isIncreasing && preciseAngle >= nextThreshold) || 
                    (!isIncreasing && preciseAngle <= nextThreshold))
                {
                    wchar_t timeStr[32];
                    swprintf_s(
                        timeStr, 
                        L"%02d:%02d", 
                        preciseTime.tm_hour, 
                        preciseTime.tm_min
                    );
                    return timeStr;
                }
            }
        }
    }
    
    return L"No switch today";
}

//
// ================= 系统托盘 =================
//

void addTrayIcon()
{
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void updateTray(double angle, const wstring& wallpaper, const wstring& nextSwitchTime)
{
    wchar_t text[128];
    swprintf_s(
        text,
        L"Solar Altitude: %.2f°\nWallpaper: %s\nNext Switch: %s",
        angle,
        getFilename(wallpaper).c_str(),
        nextSwitchTime.c_str()
    );
    wcscpy_s(nid.szTip, text);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void ShowTrayMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, ID_TRAY_RELOAD, L"Reload config");
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT,   L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(
        menu,
        TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x,
        pt.y,
        0,
        hwnd,
        NULL
    );
    DestroyMenu(menu);
}

//
// ================= 窗口过程 =================
//

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TASKBARCREATED)
    {
        addTrayIcon();
        return 0;
    }

    switch (msg)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu(hwnd);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT)
        {
            running = false;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
        }
        if (LOWORD(wParam) == ID_TRAY_RELOAD)
        {
            loadConfig();
        }
        break;

    case WM_DESTROY:
        running = false;
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//
// ================= 主程序 =================
//

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"SolarWallpaperMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    if (!loadConfig())
    {
        MessageBoxW(
            NULL,
            L"config.ini missing or invalid.\nRun startup.cmd first.",
            L"SolarWallpaper",
            MB_ICONERROR
        );
        return 0;
    }

    WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"SolarWallpaperClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"SolarWallpaper",
        0,
        0,0,0,0,
        NULL,NULL,hInstance,NULL
    );

    nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcscpy_s(nid.szTip, L"SolarWallpaper");
    addTrayIcon();

    wstring baseDir = getExeDir();
    WALLPAPERS[5] = baseDir + L"\\wallpaper\\night.JPG";
    WALLPAPERS[1] = baseDir + L"\\wallpaper\\sunrise_sunset.JPG";
    WALLPAPERS[2] = baseDir + L"\\wallpaper\\morning.JPG";
    WALLPAPERS[3] = baseDir + L"\\wallpaper\\day.JPG";
    WALLPAPERS[4] = baseDir + L"\\wallpaper\\noon.JPG";

    thread worker([&]()
    {
        int lastZone = -1;
        while (running)
        {
            double angle = getSolarAltitude();
            int zone = getZone(angle);
            wstring target = WALLPAPERS[zone];

            if (zone != lastZone)
            {
                if (getCurrentWallpaper() != target)
                    setWallpaper(target.c_str());
                lastZone = zone;
            }

            wstring nextSwitchTime = calculateNextSwitchTime();
            updateTray(angle, target, nextSwitchTime);
            this_thread::sleep_for(chrono::seconds(60));
        }
    });

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    worker.join();
    ReleaseMutex(mutex);
    return 0;
}
