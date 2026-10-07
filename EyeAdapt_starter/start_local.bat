@echo off
rem Adjust these paths to your installs
set DAYZ=C:\Program Files (x86)\Steam\steamapps\common\DayZ
set SERVER=C:\Program Files (x86)\Steam\steamapps\common\DayZServer
rem Folder that Addon Builder packs into (contains Addons\EyeAdapt.pbo)
set MOD=P:\@EyeAdapt

taskkill /F /IM DayZServer_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_x64.exe /T >nul 2>&1

cd /d "%SERVER%"
start "" DayZServer_x64.exe -config=serverDZ.cfg -port=2302 -profiles=profiles -mod=%MOD% -doLogs -adminLog
timeout /t 20
cd /d "%DAYZ%"
start "" DayZ_x64.exe -connect=127.0.0.1 -port=2302 -mod=%MOD% -nosplash -noPause -window -name=Dev
