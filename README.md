# GUMI_SolarWallpaper

Solar altitude based dynamic wallpaper switcher for Windows.

Automatically changes your desktop wallpaper according to the real-time solar altitude angle at your location.

Designed as a lightweight, standalone background utility with system tray integration.

---

## Features

• Real-time solar altitude calculation
• Automatic wallpaper switching based on solar angle
• System tray icon with live status display
• Displays current solar altitude (two decimal precision)
• Displays current wallpaper state
• Automatic location detection (IP-based)
• Manual location configuration support
• Startup manager included
• Extremely lightweight (< 1 MB RAM typical)
• No internet required for runtime
• No telemetry
• No background services
• No installation required (portable)

---

## How it works

The program calculates solar altitude using:

• Your configured latitude
• Your configured longitude
• Current system time
• Solar correction table

Then selects wallpaper based on altitude range:

```
Altitude < -12°     → wallpaper 1 (night)
-12° to 10°         → wallpaper 4 (sunrise / sunset)
10° to 40°          → wallpaper 2 (day)
40° to 90°          → wallpaper 3 (noon)
```

Wallpaper switching occurs automatically when entering a new zone.

---

## Installation

Extract the folder anywhere.

Example:

```
GUMI_SolarWallpaper/
│
├─ solarwallpaper.exe
├─ startup.cmd
├─ stop.cmd
├─ config.ini
├─ icon.ico
│
└─ wallpaper/
   ├─ 1.jpg
   ├─ 2.jpg
   ├─ 3.jpg
   └─ 4.jpg
```

No installation required.

---

## First-time setup

Run:

```
startup.cmd
```

Select:

```
1. Set location
```

Then choose:

```
1. Manual input
or
2. Auto detect (IP)
```

Location will be saved automatically.

---

## Startup manager

Run:

```
startup.cmd
```

Available options:

```
1. Set location
2. Enable startup
3. Disable startup
4. Start now
5. Stop now
6. Exit
```

Displays:

```
Startup status
Process status
Current configured location
```

---

## System tray features

Tray icon displays:

• Current solar altitude
• Current wallpaper state

Right-click tray icon:

```
Reload config
Exit
```

---

## Stop program manually

Option 1:

```
startup.cmd → Stop now
```

Option 2:

```
stop.cmd
```

Option 3:

```
Right-click tray icon → Exit
```

---

## Configuration file

config.ini

```
[location]
lat=39.9042
lon=116.4074
```

Precision recommendation:

```
4–6 decimal places
```

Example accuracy:

```
4 decimals → ~11 meters
6 decimals → ~0.11 meters
```

---

## Requirements

Windows 10 / 11

No dependencies
No installation
No administrator privileges required

---

## Privacy

SolarWallpaper:

• Does NOT collect any data
• Does NOT track user activity
• Does NOT connect to internet during runtime

IP detection is used ONLY during setup when user explicitly selects auto-detect.

---

## Resource and attribution notice

AIGC Notice:

Some images are generated using AI tools. Original author permissions may not yet have been obtained.

This software is:

• Completely free
• Non-commercial
• Created for educational and technical practice

If any infringement exists, please contact for removal:

[Nsakrty@163.com](mailto:Nsakrty@163.com)

---

## Content attribution

GUMI character reference source:

I Can't Wait feat. GUMI
[https://www.bilibili.com/video/BV15GmhBaEZF](https://www.bilibili.com/video/BV15GmhBaEZF)

Illustrator: 米粥
[http://b23.tv/cgIAPqP](http://b23.tv/cgIAPqP)

Background images from:

SpaceEngine
[https://spaceengine.org/](https://spaceengine.org/)

---

## Author

Nsakrty

---

# 中文说明

基于太阳高度角的 Windows 动态壁纸切换工具。

程序根据你所在位置的实时太阳高度角自动切换桌面壁纸。

轻量、独立运行、无后台服务。

---

## 功能

• 实时太阳高度角计算
• 自动切换壁纸
• 托盘图标实时状态显示
• 显示太阳高度角（两位小数）
• 显示当前壁纸状态
• 支持自动定位（IP定位）
• 支持手动输入经纬度
• 自启动管理工具
• 极低内存占用（约 < 1 MB）
• 运行时无需联网
• 无遥测
• 无后台服务
• 绿色便携软件

---

## 工作原理

程序使用：

• 纬度
• 经度
• 当前时间
• 太阳修正表

计算太阳高度角，并根据区间切换壁纸：

```
高度角 < -12°     夜晚
-12° ~ 10°        日出 / 日落
10° ~ 40°         白天
40° ~ 90°         正午
```

进入新区间时自动切换壁纸。

---

## 安装方法

解压文件夹，例如：

```
GUMI_SolarWallpaper/
```

无需安装。

---

## 首次使用

运行：

```
startup.cmd
```

选择：

```
1. Set location
```

然后选择：

```
1 手动输入
或
2 自动定位（IP）
```

系统会自动保存配置。

---

## 启动管理器

运行：

```
startup.cmd
```

功能：

```
设置位置
启用开机启动
禁用开机启动
立即启动程序
立即停止程序
查看当前状态
```

显示：

```
开机启动状态
程序运行状态
当前位置配置
```

---

## 托盘功能

托盘显示：

• 当前太阳高度角
• 当前壁纸状态

右键菜单：

```
Reload config
Exit
```

---

## 停止程序

方法1：

```
startup.cmd → Stop now
```

方法2：

```
stop.cmd
```

方法3：

```
右键托盘图标 → Exit
```

---

## 配置文件

config.ini

```
[location]
lat=纬度
lon=经度
```

推荐精度：

```
4–6位小数
```

---

## 隐私说明

本软件：

• 不收集任何数据
• 不追踪用户行为
• 不在运行时联网

自动定位仅在用户主动选择时使用。

---

## AIGC提示

图片均为 AIGC 生成，目前尚未获得全部原作者授权。

本软件：

• 完全免费
• 非商业用途
• 仅用于学习与技术实践

如有侵权，请联系删除：

[Nsakrty@163.com](mailto:Nsakrty@163.com)

---

## 资源来源

GUMI人物参考来源：

I Can't Wait feat. GUMI
[https://www.bilibili.com/video/BV15GmhBaEZF](https://www.bilibili.com/video/BV15GmhBaEZF)

画师：米粥
[http://b23.tv/cgIAPqP](http://b23.tv/cgIAPqP)

背景来源：

SpaceEngine
[https://spaceengine.org/](https://spaceengine.org/)

---

## 作者

Nsakrty
"# GUMI_SolarWallpaper" 
