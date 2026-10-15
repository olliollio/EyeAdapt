@echo off
rem Signs the packed PBO. Run this AFTER every repack and BEFORE uploading to the Workshop.
rem Repacking changes the PBO checksum, so an older .bisign no longer matches and servers with
rem verifySignatures = 2 kick clients ("Missing PBO" / 0x00040073). The Workshop ships the .bisign as is.
set TOOLS=D:\SteamLibrary\steamapps\common\DayZ Tools
set KEY=D:\DayZ-Mods\keys\EyeAdapt.biprivatekey
set BUILD=P:\@EyeAdapt
if exist "%~dp0paths.local.bat" call "%~dp0paths.local.bat"

if not exist "%BUILD%\Addons\EyeAdapt.pbo" echo ERROR: %BUILD%\Addons\EyeAdapt.pbo not found, pack first & exit /b 1
if not exist "%KEY%" echo ERROR: private key not found: %KEY% & exit /b 1

del "%BUILD%\Addons\*.bisign" >nul 2>&1
"%TOOLS%\Bin\DsUtils\DSSignFile.exe" "%KEY%" "%BUILD%\Addons\EyeAdapt.pbo"
if errorlevel 1 echo ERROR: signing failed & exit /b 1
if not exist "%BUILD%\Addons\EyeAdapt.pbo.EyeAdapt.bisign" echo ERROR: no .bisign created & exit /b 1

echo.
echo Signed. Upload %BUILD% with the Publisher now, do not repack again before that.
dir "%BUILD%\Addons"
