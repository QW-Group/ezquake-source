@echo off
setlocal enabledelayedexpansion

REM echo Compiling help files...
REM C:\Projects\quake\ezQuake\visual-studio\Debug\TestApp2.exe help_commands.json > .help_commands.temp
REM fc .help_commands.temp .help_commands.c > nul
REM if %ERRORLEVEL% GTR 0 copy /y .help_commands.temp .help_commands.c

REM C:\Projects\quake\ezQuake\visual-studio\Debug\TestApp2.exe help_variables.json > .help_variables.temp
REM fc .help_variables.temp .help_variables.c > nul
REM if %ERRORLEVEL% GTR 0 copy /y .help_variables.temp .help_variables.c

REM if exist menus.json (
REM 	echo Compiling menus...
REM 	C:\Projects\quake\ezQuake\visual-studio\Debug\TestApp2.exe menus.json > .menus.temp
REM 	fc .menus.temp .menus.c > nul
REM 	if !ERRORLEVEL! GTR 0 (
REM 		echo Updating menus...
REM 		copy /y .menus.temp .menus.c
REM 	)
REM ) else (
REM 	echo // no menus in this version > .menus.c
REM )

REM echo Checking git version...
REM <nul set /p =#define REVISION > C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM git rev-list HEAD | wc -l | tr -d -c 0-9 >> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM echo. >> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM <nul set /p =#define VERSION ^">> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM git rev-list HEAD | wc -l | tr -d -c 0-9 >> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM <nul set /p =~>> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM git rev-parse --short HEAD | tr -d -c 0-9a-f >> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM echo ^" >> C:\Projects\Quake\ezQuake\source2\.msversion.temp
REM fc C:\Projects\Quake\ezQuake\source2\.msversion.temp C:\Projects\Quake\ezQuake\source2\.msversion.h > nul
REM if %ERRORLEVEL% GTR 0 (
REM 	copy /y ".msversion.temp" ".msversion.h"
REM 	copy /y ".msversion.temp" "include/.msversion.h"
REM )

echo Checking git version...
git rev-list HEAD | find /c /v "" > .git-revision
set gitrev=
for /f %%x in (.git-revision) do set "gitrev=!gitrev!%%x"

echo Checking git hash...
git rev-parse --short HEAD > .git-hash
set githash=
for /f %%x in (.git-hash) do set "githash=!githash!%%x"

echo Writing .msversion.temp
echo #define REVISION %gitrev% > .msversion.temp
echo #define VERSION ^"%gitrev%~%githash%^" >> .msversion.temp

fc .msversion.temp .msversion.h > nul
if %ERRORLEVEL% GTR 0 copy /y ".msversion.temp" ".msversion.h"
