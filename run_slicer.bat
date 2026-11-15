@echo off
rem Builds the project and starts the Chaos Slicer demo.
rem   run_slicer.bat         build, then open the Chaos Slicer demo window
rem   run_slicer.bat test    build, then run all automated tests
setlocal
set "QT_DIR=C:\Qt\6.8.3\mingw_64"
set "QT_TOOLS=C:\Qt\Tools"
set "PATH=%QT_DIR%\bin;%QT_TOOLS%\mingw1310_64\bin;%QT_TOOLS%\CMake_64\bin;%PATH%"
cd /d "%~dp0"

if not exist build\CMakeCache.txt (
    echo Configuring...
    cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=%QT_DIR% || goto :fail
)

echo Building...
cmake --build build -j8 || goto :fail

if /i "%~1"=="test" (
    ctest --test-dir build --output-on-failure || goto :fail
    echo.
    echo All tests passed.
    goto :end
)

start "" build\ui\slicer_demo.exe
goto :end

:fail
echo.
echo Failed. See the messages above.
exit /b 1

:end
endlocal
