#include <cmath>
#include <ctime>
#include <string>
#include <thread>
#include <chrono>

#include <windows.h>
#include <shellapi.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

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
int weathercode = 0;
int lastWeathercode = 0;
bool weatherUpdated = false;

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
// ================= 日志功能 =================
//

void writeLog(const string& message)
{
    time_t now = time(NULL);
    tm local;
    localtime_s(&local, &now);
    
    char timeStr[32];
    strftime(timeStr, 32, "%Y-%m-%d %H:%M:%S", &local);
    
    FILE* logFile = fopen("weather_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "[%s] %s\n", timeStr, message.c_str());
        fclose(logFile);
    }
}

//
// ================= 网络请求 =================
//

string httpGet(const string& url)
{
    writeLog("Requesting: " + url);
    
    HINTERNET hInternet = InternetOpenA("SolarWallpaper", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        writeLog("InternetOpen failed");
        return "";
    }
    
    // 添加INTERNET_FLAG_SECURE标志以支持HTTPS
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (!hConnect) {
        writeLog("InternetOpenUrl failed");
        InternetCloseHandle(hInternet);
        return "";
    }
    
    char buffer[4096];
    DWORD bytesRead;
    string response;
    
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }
    
    writeLog("Response: " + response);
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    return response;
}

//
// ================= JSON解析 =================
//

int parseWeatherCode(const string& json)
{
    // 查找第二个current_weather（跳过current_weather_units）
    size_t firstPos = json.find("current_weather");
    if (firstPos == string::npos) {
        writeLog("current_weather not found");
        return 0;
    }
    
    size_t secondPos = json.find("current_weather", firstPos + 1);
    if (secondPos == string::npos) {
        writeLog("second current_weather not found");
        return 0;
    }
    
    // 找到current_weather对象的开始
    size_t objStart = json.find('{', secondPos);
    if (objStart == string::npos) {
        writeLog("current_weather object not found");
        return 0;
    }
    
    // 在current_weather对象内查找weathercode
    size_t weathercodePos = json.find("weathercode", objStart);
    if (weathercodePos == string::npos) {
        writeLog("weathercode not found in current_weather");
        return 0;
    }
    
    // 跳过weathercode: 
    size_t colonPos = json.find(':', weathercodePos);
    if (colonPos == string::npos) {
        writeLog("colon not found");
        return 0;
    }
    
    colonPos++;
    // 跳过空格
    while (colonPos < json.size() && (json[colonPos] == ' ' || json[colonPos] == '\t' || json[colonPos] == '\n' || json[colonPos] == '\r')) {
        colonPos++;
    }
    
    // 读取数字
    string codeStr;
    while (colonPos < json.size() && isdigit(json[colonPos])) {
        codeStr += json[colonPos];
        colonPos++;
    }
    
    if (codeStr.empty()) {
        writeLog("empty weathercode");
        return 0;
    }
    
    int code = stoi(codeStr);
    writeLog("Parsed weathercode: " + to_string(code));
    return code;
}

//
// ================= 天气API调用 =================
//

string getWeatherDescription(int code)
{
    switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly Clear";
    case 2: return "Partly Cloudy";
    case 3: return "Cloudy";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: case 56: case 57: case 61: case 63: case 65: case 66: case 67: return "Rain";
    case 71: case 73: case 75: case 77: return "Snow";
    case 80: case 81: case 82: case 85: case 86: case 95: case 96: case 99: return "Thunderstorm";
    default: return "";
    }
}

void updateWeather()
{
    if (LAT == 0 || LON == 0) {
        writeLog("Invalid location: lat=" + to_string(LAT) + ", lon=" + to_string(LON));
        return;
    }
    
    char url[256];
    sprintf_s(url, "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f&current_weather=true", LAT, LON);
    
    string response = httpGet(url);
    if (!response.empty()) {
        int newCode = parseWeatherCode(response);
        writeLog("Parsed weathercode: " + to_string(newCode));
        // 允许weathercode为0（晴天）
        if (newCode != 0 || (newCode == 0 && weathercode != 0)) {
            writeLog("Updating weathercode from " + to_string(weathercode) + " to " + to_string(newCode));
            lastWeathercode = weathercode;
            weathercode = newCode;
            weatherUpdated = true;
        } else {
            writeLog("Weathercode unchanged: " + to_string(newCode));
        }
    } else {
        writeLog("Empty response");
    }
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

bool isCloudyWeather(int code)
{
    return code == 2 || code == 3;
}

wstring getWeatherWallpaperPath(int zone, int weatherCode)
{
    wstring baseDir = getExeDir();
    wstring defaultPath = WALLPAPERS[zone];

    if (!isCloudyWeather(weatherCode)) {
        return defaultPath;
    }

    wstring weatherSuffix = L"_cloudy";
    size_t dotPos = defaultPath.rfind(L'.');
    if (dotPos == wstring::npos) {
        return defaultPath;
    }

    wstring weatherPath = defaultPath.substr(0, dotPos) + weatherSuffix + defaultPath.substr(dotPos);

    FILE* f = _wfopen(weatherPath.c_str(), L"rb");
    if (f) {
        fclose(f);
        return weatherPath;
    }

    return defaultPath;
}

//
// ================= 下次切换时间计算 =================
//

wstring calculateNextSwitchTime(time_t* testTimePtr = NULL, bool testMode = false)
{
    time_t now = testTimePtr ? *testTimePtr : time(NULL);
    tm local;
    localtime_s(&local, &now);
    
    double currentAngle = getSolarAltitude(&now);
    int currentZone = getZone(currentAngle);
    
    // 确定下一个阈值
    double nextThreshold = 0;
    bool isIncreasing = false;
    
    // 判断太阳高度角是在上升还是下降
    tm testTime = local;
    testTime.tm_min += 10;
    time_t testNowTemp = mktime(&testTime);
    double testAngle = getSolarAltitude(&testNowTemp);
    isIncreasing = (testAngle > currentAngle);
    
    // 根据当前区域确定下一个阈值
    switch (currentZone)
    {
    case 1:
        if (isIncreasing) nextThreshold = THRESHOLD_SUNRISE;
        else nextThreshold = THRESHOLD_NIGHT;
        break;
    case 2:
        if (isIncreasing) nextThreshold = THRESHOLD_MORNING;
        else nextThreshold = THRESHOLD_SUNRISE;
        break;
    case 3:
        if (isIncreasing) nextThreshold = THRESHOLD_DAY;
        else nextThreshold = THRESHOLD_MORNING;
        break;
    case 4:
        nextThreshold = THRESHOLD_DAY;
        break;
    case 5:
        // 晚上：太阳高度角 <= THRESHOLD_NIGHT，下一次切换是达到THRESHOLD_NIGHT
        nextThreshold = THRESHOLD_NIGHT;
        break;
    }
    
    // 以10分钟为步长遍历，寻找分界线
    tm searchTime = local;
    time_t searchNow = now;
    
    // 对于zone 5，如果当前太阳高度角 <= THRESHOLD_NIGHT，需要找到太阳从最低点上升到THRESHOLD_NIGHT的时间
    bool zone5SpecialCase = (currentZone == 5 && currentAngle <= THRESHOLD_NIGHT);
    
    // 对于zone 4，如果太阳高度角在上升，需要找到太阳从最高点下降到THRESHOLD_DAY的时间
    bool zone4SpecialCase = (currentZone == 4 && isIncreasing);
    
    for (int i = 0; i < 288; i++) // 48小时
    {
        searchTime.tm_min += 10;
        searchNow = mktime(&searchTime);
        localtime_s(&searchTime, &searchNow);
        
        double searchAngle = getSolarAltitude(&searchNow);
        
        // 检查是否达到阈值
        bool reachedThreshold = false;
        if (zone5SpecialCase)
        {
            // Zone 5 特殊情况：需要找到太阳从最低点上升到THRESHOLD_NIGHT的时间
            if (searchAngle >= THRESHOLD_NIGHT)
            {
                reachedThreshold = true;
            }
        }
        else if (zone4SpecialCase)
        {
            // Zone 4 特殊情况：需要找到太阳从最高点下降到THRESHOLD_DAY的时间
            // 先检查太阳高度角是否开始下降
            if (searchAngle < currentAngle)
            {
                // 太阳高度角开始下降，现在检查是否达到THRESHOLD_DAY
                if (searchAngle <= THRESHOLD_DAY)
                {
                    reachedThreshold = true;
                }
            }
        }
        else
        {
            // 正常情况
            if ((isIncreasing && searchAngle >= nextThreshold) || 
                (!isIncreasing && searchAngle <= nextThreshold))
            {
                reachedThreshold = true;
            }
        }
        
        if (reachedThreshold)
        {
            // 找到分界线，切换到1分钟精度进行详细搜索
            tm preciseTime = searchTime;
            preciseTime.tm_min -= 10;
            time_t preciseNow = mktime(&preciseTime);
            
            for (int j = 0; j < 10; j++)
            {
                preciseTime.tm_min += 1;
                preciseNow = mktime(&preciseTime);
                localtime_s(&preciseTime, &preciseNow);
                
                double preciseAngle = getSolarAltitude(&preciseNow);
                
                bool preciseReachedThreshold = false;
                if (zone5SpecialCase)
                {
                    // Zone 5 特殊情况
                    if (preciseAngle >= THRESHOLD_NIGHT)
                    {
                        preciseReachedThreshold = true;
                    }
                }
                else if (zone4SpecialCase)
                {
                    // Zone 4 特殊情况：需要找到太阳从最高点下降到THRESHOLD_DAY的时间
                    // 先检查太阳高度角是否开始下降
                    if (preciseAngle < currentAngle)
                    {
                        // 太阳高度角开始下降，现在检查是否达到THRESHOLD_DAY
                        if (preciseAngle <= THRESHOLD_DAY)
                        {
                            preciseReachedThreshold = true;
                        }
                    }
                }
                else
                {
                    // 正常情况
                    if ((isIncreasing && preciseAngle >= nextThreshold) || 
                        (!isIncreasing && preciseAngle <= nextThreshold))
                    {
                        preciseReachedThreshold = true;
                    }
                }
                
                if (preciseReachedThreshold)
                {
                    // 找到精确时间
                    wchar_t timeStr[32];
                    swprintf_s(timeStr, L"%02d:%02d", preciseTime.tm_hour, preciseTime.tm_min);
                    
                    // 测试模式：显示详细信息
                    if (testMode)
                    {
                        wchar_t testMsg[512];
                        swprintf_s(testMsg, 512,
                            L"Test Mode Result:\n\n"
                            L"Input Time: %02d:%02d\n"
                            L"Current Angle: %.2f°, Zone: %d\n"
                            L"Next Threshold: %.2f°\n"
                            L"Next Switch: %02d:%02d\n"
                            L"Switch Angle: %.2f°\n"
                            L"Search Steps: %d (10min) + %d (1min)",
                            local.tm_hour, local.tm_min,
                            currentAngle, currentZone,
                            nextThreshold,
                            preciseTime.tm_hour, preciseTime.tm_min,
                            preciseAngle,
                            i + 1, j + 1
                        );
                        MessageBoxW(NULL, testMsg, L"Test Mode", MB_OK);
                    }
                    
                    return timeStr;
                }
            }
        }
    }
    
    if (testMode)
    {
        wchar_t testMsg[256];
        swprintf_s(testMsg, 256,
            L"Test Mode Result:\n\n"
            L"Input Time: %02d:%02d\n"
            L"Current Angle: %.2f°, Zone: %d\n"
            L"Next Threshold: %.2f°\n"
            L"Result: No switch today",
            local.tm_hour, local.tm_min,
            currentAngle, currentZone,
            nextThreshold
        );
        MessageBoxW(NULL, testMsg, L"Test Mode", MB_OK);
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

wstring stringToWstring(const string& str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* buffer = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, len);
    wstring wstr(buffer);
    delete[] buffer;
    return wstr;
}

void updateTray(double angle, const wstring& wallpaper, const wstring& nextSwitchTime)
{
    wchar_t text[128];
    string weatherDesc = getWeatherDescription(weathercode);
    
    if (!weatherDesc.empty()) {
        wstring wWeatherDesc = stringToWstring(weatherDesc);
        if (weatherUpdated) {
            swprintf_s(
                text,
                L"Solar Altitude: %.2f°\nWallpaper: %s\nNext Switch: %s\nWeather:%s(%d)",
                angle,
                getFilename(wallpaper).c_str(),
                nextSwitchTime.c_str(),
                wWeatherDesc.c_str(),
                weathercode
            );
        } else {
            swprintf_s(
                text,
                L"Solar Altitude: %.2f°\nWallpaper: %s\nNext Switch: %s\nWeather:%s(%d)(last)",
                angle,
                getFilename(wallpaper).c_str(),
                nextSwitchTime.c_str(),
                wWeatherDesc.c_str(),
                weathercode
            );
        }
    } else {
        swprintf_s(
            text,
            L"Solar Altitude: %.2f°\nWallpaper: %s\nNext Switch: %s",
            angle,
            getFilename(wallpaper).c_str(),
            nextSwitchTime.c_str()
        );
    }
    
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
        int currentMinute = -1;
        
        // 程序启动时立即调用一次天气API
        weatherUpdated = false;
        updateWeather();
        
        while (running)
        {
            time_t now = time(NULL);
            tm local;
            localtime_s(&local, &now);
            
            // 每10分钟更新一次天气
            if (local.tm_min % 10 == 0 && local.tm_min != currentMinute)
            {
                weatherUpdated = false;
                updateWeather();
                currentMinute = local.tm_min;
            }
            
            double angle = getSolarAltitude();
            int zone = getZone(angle);
            wstring target = getWeatherWallpaperPath(zone, weathercode);

            if (zone != lastZone || weatherUpdated)
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
