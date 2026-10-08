@echo off
rem Adjust these paths to your installs
set DAYZ=D:\SteamLibrary\steamapps\common\DayZ
set SERVER=D:\SteamLibrary\steamapps\common\DayZServer
rem Folder that Addon Builder packs into (contains Addons\EyeAdapt.pbo)
set BUILD=P:\@EyeAdapt
rem Mod is copied into both game folders and loaded by relative path
rem (loading it straight from P:\ coincided with "Game restart required" kicks)
set MOD=@EyeAdapt

rem 1 = also load VPPAdminTools (needs CF) for teleport/time/weather while testing, 0 = EyeAdapt only.
rem Admin access: Steam64 ID in %SERVER%\profiles\VPPAdminTools\Permissions\SuperAdmins\SuperAdmins.txt
set ADMIN=1
set WORKSHOP=%DAYZ%\!Workshop

taskkill /F /IM DayZServer_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_BE.exe /T >nul 2>&1

robocopy "%BUILD%" "%SERVER%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul
robocopy "%BUILD%" "%DAYZ%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul

set SERVERMODS=%MOD%
set CLIENTMODS=%MOD%
if "%ADMIN%"=="1" (
	rem Server has no !Workshop folder, so copy the workshop mods next to it; client loads them from !Workshop
	robocopy "%WORKSHOP%\@CF" "%SERVER%\@CF" /MIR /NJH /NJS /NDL /NFL >nul
	robocopy "%WORKSHOP%\@VPPAdminTools" "%SERVER%\@VPPAdminTools" /MIR /NJH /NJS /NDL /NFL >nul
	set SERVERMODS=@CF;@VPPAdminTools;%MOD%
	set CLIENTMODS=!Workshop\@CF;!Workshop\@VPPAdminTools;%MOD%
)

cd /d "%SERVER%"
start "" DayZServer_x64.exe -config=serverDZ.cfg -port=2302 -profiles=profiles "-mod=%SERVERMODS%" -doLogs -adminLog
rem Server is ready after ~22 s, the client needs ~40 s (BattlEye + engine init) before it connects,
rem so both can start almost together. Raise this if the client fails to connect.
timeout /t 3
cd /d "%DAYZ%"
rem Client must run with BattlEye, plain DayZ_x64.exe gets kicked ("Game restart required").
rem Same call the DayZ Launcher uses: DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe <game args>
rem No -window: windowed mode lifts the blacks on this PC (vanilla too), so judge night visuals in fullscreen only
start "" DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe -connect=127.0.0.1 -port=2302 "-mod=%CLIENTMODS%" -nosplash -noPause -name=Dev
