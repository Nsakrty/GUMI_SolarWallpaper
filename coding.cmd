taskkill /f /im solarwallpaper.exe

g++ solarwallpaper.cpp icon_res.o -o solarwallpaper.exe ^
-static ^
-static-libgcc ^
-static-libstdc++ ^
-mwindows ^
-municode ^
-DUNICODE ^
-D_UNICODE ^
-O2 ^
-s ^
-lwininet

start solarwallpaper.exe