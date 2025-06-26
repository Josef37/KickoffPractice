@echo off
REM Requires 7-Zip installed

set ZIP_NAME=KickoffPractice.zip

REM Remove existing zip
if exist %ZIP_NAME% del %ZIP_NAME%

REM Create zip with inclusions and exclusions
"C:\Program Files\7-Zip\7z.exe" a %ZIP_NAME% ^
    data ^
    source ^
    -xr!source\.vs ^
    -xr!source\build ^
    -x!source\.gitignore ^
    -x!source\.gitattributes 

echo Created %ZIP_NAME% successfully
pause
