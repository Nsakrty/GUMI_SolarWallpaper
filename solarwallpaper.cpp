#include <windows.h>
#include <shellapi.h>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <ctime>

using namespace std;

//
// ================= 常量 =================
//

const double PI = 3.14159265358979323846;

// Explorer restart message
UINT WM_TASKBARCREATED;

double LAT = 0;
double LON = 0;

wstring WALLPAPERS[5];

#define WM_TRAYICON (WM_USER + 1)

#define ID_TRAY_EXIT   1001
#define ID_TRAY_RELOAD 1002

NOTIFYICONDATA nid;

bool running = true;


//
// ================= 路径 =================
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
// ================= config =================
//

double readIniDouble(const wchar_t* key)
{
    wchar_t buffer[64];

    GetPrivateProfileStringW(
        L"location",
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
    LAT = readIniDouble(L"lat");
    LON = readIniDouble(L"lon");

    if (LAT == 0 && LON == 0)
        return false;

    return true;
}


//
// ================= 数学 =================
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
// ================= 日期 =================
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
// ================= 太阳高度角 =================
//

double getSolarAltitude()
{
    time_t now = time(NULL);

    tm local;
    localtime_s(&local, &now);

    int month = local.tm_mon;
    int day   = local.tm_mday;

    int hour = local.tm_hour;
    int min  = local.tm_min;
    int sec  = local.tm_sec;

    int N = getDayOfYear(&local);

    double timeHour =
        hour +
        min / 60.0 +
        sec / 3600.0;

    double longitudeCorrection =
        (LON - 120.0) * 4.0;

    double offset =
        getSolarOffsetMinutes(month, day);

    double solarTime =
        timeHour
        + longitudeCorrection / 60.0
        - offset / 60.0;

    double hourAngle =
        15.0 * (solarTime - 12.0);

    double decl =
        23.45 *
        sin(deg2rad(
            360.0 * (284 + N) / 365.0
        ));

    double latRad  = deg2rad(LAT);
    double declRad = deg2rad(decl);
    double hourRad = deg2rad(hourAngle);

    double altitude =
        asin(
            sin(latRad)*sin(declRad)
            +
            cos(latRad)*cos(declRad)*cos(hourRad)
        );

    return rad2deg(altitude);
}


//
// ================= 壁纸 =================
//

wstring getCurrentWallpaper()
{
    wchar_t buffer[MAX_PATH];

    SystemParametersInfoW(
        SPI_GETDESKWALLPAPER,
        MAX_PATH,
        buffer,
        0
    );

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
// ================= 区域 =================
//

int getZone(double angle)
{
    if (angle < -12) return 1;
    if (angle < 10)  return 4;
    if (angle < 40)  return 2;
    return 3;
}


//
// ================= 托盘 =================
//

void addTrayIcon()
{
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void updateTray(double angle, const wstring& wallpaper)
{
    wchar_t text[128];

    swprintf_s(
        text,
        L"Solar Altitude: %.2f° | %s",
        angle,
        getFilename(wallpaper).c_str()
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

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    // Explorer restarted
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

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPWSTR,
    int
)
{
    HANDLE mutex =
        CreateMutexW(NULL, TRUE, L"SolarWallpaperMutex");

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


    // Register Explorer restart message
    WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");


    WNDCLASS wc = {};

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"SolarWallpaperClass";

    RegisterClass(&wc);


    HWND hwnd =
        CreateWindowEx(
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

    WALLPAPERS[1] = baseDir + L"\\wallpaper\\1.jpg";
    WALLPAPERS[2] = baseDir + L"\\wallpaper\\2.jpg";
    WALLPAPERS[3] = baseDir + L"\\wallpaper\\3.jpg";
    WALLPAPERS[4] = baseDir + L"\\wallpaper\\4.jpg";


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

            updateTray(angle, target);

            this_thread::sleep_for(
                chrono::seconds(60)
            );
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
