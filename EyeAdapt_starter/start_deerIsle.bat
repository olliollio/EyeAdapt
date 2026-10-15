@echo off
rem Same as start_local.bat, but on the community map Deer Isle (Workshop mod @DeerIsle, ID 1602372402)
rem Adjust these paths to your installs
set DAYZ=D:\SteamLibrary\steamapps\common\DayZ
set SERVER=D:\SteamLibrary\steamapps\common\DayZServer
rem Folder that Addon Builder packs into (contains Addons\EyeAdapt.pbo)
set BUILD=P:\@EyeAdapt
rem Mod is copied into both game folders and loaded by relative path
rem (loading it straight from P:\ coincided with "Game restart required" kicks)
set MOD=@EyeAdapt
rem Signing: the PBO is re-signed on every start (Addon Builder doesn't always sign)
set TOOLS=D:\SteamLibrary\steamapps\common\DayZ Tools
set KEY=D:\DayZ-Mods\keys\EyeAdapt.biprivatekey
rem Per-PC overrides (DAYZ, SERVER, BUILD, ...) go into paths.local.bat next to this file (gitignored)
if exist "%~dp0paths.local.bat" call "%~dp0paths.local.bat"

rem 1 = also load VPPAdminTools (needs CF) for teleport/time/weather while testing, 0 = EyeAdapt only.
rem Admin access: Steam64 ID in %SERVER%\profiles\VPPAdminTools\Permissions\SuperAdmins\SuperAdmins.txt
set ADMIN=0
set WORKSHOP=%DAYZ%\!Workshop

rem Server config: <DayZServer>\serverDZ_deerisle.cfg (real time, mission template dayzOffline.deerisle)
rem The mission folder %SERVER%\mpmissions\dayzOffline.deerisle must exist (not part of the @DeerIsle mod, get it from the Deer Isle server files)
set SERVERCFG=serverDZ_deerisle.cfg

taskkill /F /IM DayZServer_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_x64.exe /T >nul 2>&1
taskkill /F /IM DayZ_BE.exe /T >nul 2>&1

if not exist "%SERVER%\mpmissions\dayzOffline.deerisle" echo WARNING: mission folder %SERVER%\mpmissions\dayzOffline.deerisle is missing, the server will not start

if exist "%KEY%" (
	"%TOOLS%\Bin\DsUtils\DSSignFile.exe" "%KEY%" "%BUILD%\Addons\EyeAdapt.pbo"
	if errorlevel 1 echo WARNING: signing failed, the server will kick the client
) else (
	echo WARNING: private key not found: %KEY%
)

robocopy "%BUILD%" "%SERVER%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul
robocopy "%BUILD%" "%DAYZ%\%MOD%" /MIR /NJH /NJS /NDL /NFL >nul

rem Deer Isle is required on server and client. The server has no !Workshop folder, so copy the mod next to it; client loads it from !Workshop
rem The server checks signatures (verifySignatures = 2): the map mod's own keys go into the server's keys folder
robocopy "%WORKSHOP%\@DeerIsle" "%SERVER%\@DeerIsle" /MIR /NJH /NJS /NDL /NFL >nul
robocopy "%WORKSHOP%\@DeerIsle\keys" "%SERVER%\keys" *.bikey /NJH /NJS /NDL /NFL >nul

set SERVERMODS=@DeerIsle;%MOD%
set CLIENTMODS=!Workshop\@DeerIsle;%MOD%
if "%ADMIN%"=="1" (
	robocopy "%WORKSHOP%\@CF" "%SERVER%\@CF" /MIR /NJH /NJS /NDL /NFL >nul
	robocopy "%WORKSHOP%\@VPPAdminTools" "%SERVER%\@VPPAdminTools" /MIR /NJH /NJS /NDL /NFL >nul
	set SERVERMODS=@CF;@VPPAdminTools;@DeerIsle;%MOD%
	set CLIENTMODS=!Workshop\@CF;!Workshop\@VPPAdminTools;!Workshop\@DeerIsle;%MOD%
)

cd /d "%SERVER%"
start "" DayZServer_x64.exe -config=%SERVERCFG% -port=2302 -profiles=profiles "-mod=%SERVERMODS%" -doLogs -adminLog
rem Server is ready after ~22 s (Deer Isle takes longer), the client needs ~40 s (BattlEye + engine init) before it connects,
rem so both can start almost together. Raise this if the client fails to connect.
timeout /t 3
cd /d "%DAYZ%"
rem Client must run with BattlEye, plain DayZ_x64.exe gets kicked ("Game restart required").
rem Same call the DayZ Launcher uses: DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe <game args>
rem No -window: windowed mode lifts the blacks on this PC (vanilla too), so judge night visuals in fullscreen only
start "" DayZ_BE.exe 0 1 1 -exe DayZ_x64.exe -connect=127.0.0.1 -port=2302 "-mod=%CLIENTMODS%" -nosplash -noPause -name=Dev
