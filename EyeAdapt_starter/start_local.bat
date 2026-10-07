@echo off
rem Adjust these paths to your installs
set DAYZ=D:\SteamLibrary\steamapps\common\DayZ
set SERVER=D:\SteamLibrary\steamapps\common\DayZServer
rem Folder that Addon Builder packs into (contains Addons\EyeAdapt.pbo)
set BUILD=P:\@EyeAdapt
rem Mod is copied into both game folders and loaded by relative path
rem (loading it straight from P:\ coincided with "Game restart required" kicks)
set MOD=@EyeAdapt

taskkill /F /IM DayZServer_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_BE.exe /T >nul 2>&1

robocopy "%BUILD%" "%SERVER%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul
robocopy "%BUILD%" "%DAYZ%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul

cd /d "%SERVER%"
start "" DayZServer_x64.exe -config=serverDZ.cfg -port=2302 -profiles=profiles -mod=%MOD% -doLogs -adminLog
timeout /t 20
cd /d "%DAYZ%"
rem Client must run with BattlEye, plain DayZ_x64.exe gets kicked ("Game restart required").
rem Same call the DayZ Launcher uses: DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe <game args>
start "" DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe -connect=127.0.0.1 -port=2302 -mod=%MOD% -nosplash -noPause -window -name=Dev
