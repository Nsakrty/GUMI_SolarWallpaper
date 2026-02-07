windres icon.rc -O coff -o icon_res.o && g++ solarwallpaper.cpp icon_res.o -o solarwallpaper.exe -municode -mwindows
pause