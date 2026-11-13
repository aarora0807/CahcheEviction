@echo off
REM ======================================================================
REM  run_windows.bat - double-click to build, test and run the simulator.
REM  Finds Visual Studio (any edition, incl. Build Tools) or MinGW g++ by
REM  itself. CMake is NOT needed.
REM ======================================================================
setlocal EnableExtensions
cd /d "%~dp0"
title Cache Eviction Simulator

set "LIB=src\AIPolicy.cpp src\AdaptivePolicy.cpp src\BeladyPolicy.cpp src\Cache.cpp src\Cli.cpp src\FIFOPolicy.cpp src\LFUPolicy.cpp src\LRUPolicy.cpp src\Metrics.cpp src\PolicyFactory.cpp src\PolicyTrainer.cpp src\ResultExporter.cpp src\Simulator.cpp src\Tracer.cpp src\WebExporter.cpp src\Workload.cpp src\Workloads.cpp"
set "TESTS=tests\test_main.cpp tests\test_fifo.cpp tests\test_lru.cpp tests\test_lfu.cpp tests\test_cache.cpp tests\test_workloads.cpp tests\test_trainer.cpp"

echo.
echo [1/4] Looking for a C++ compiler...
where cl >nul 2>nul && goto use_msvc

REM --- Visual Studio is usually not on PATH: ask vswhere where it is ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto try_gcc
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto try_gcc
if not exist "%VSPATH%\Common7\Tools\VsDevCmd.bat" goto try_gcc
echo       Found Visual Studio in "%VSPATH%"
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cd /d "%~dp0"
where cl >nul 2>nul && goto use_msvc

:try_gcc
where g++ >nul 2>nul && goto use_gcc
goto no_compiler

:use_msvc
echo       Using Microsoft C++ (cl.exe)
if not exist build mkdir build
if not exist build\obj_app mkdir build\obj_app
if not exist build\obj_test mkdir build\obj_test
echo [2/4] Building the simulator (about 30 seconds)...
cl /nologo /std:c++17 /EHsc /O2 /W3 /utf-8 /MP /Iinclude %LIB% src\main.cpp /Fobuild\obj_app\ /Febuild\cache_sim.exe > build\build_log.txt 2>&1
if errorlevel 1 goto build_failed
echo [3/4] Building and running the 43 tests...
cl /nologo /std:c++17 /EHsc /O1 /W3 /utf-8 /MP /Iinclude /Ithird_party /Itests %LIB% %TESTS% /Fobuild\obj_test\ /Febuild\unit_tests.exe >> build\build_log.txt 2>&1
if errorlevel 1 goto build_failed
goto run

:use_gcc
echo       Using GNU C++ (g++)
if not exist build mkdir build
echo [2/4] Building the simulator (about 30 seconds)...
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude %LIB% src\main.cpp -o build\cache_sim.exe -pthread > build\build_log.txt 2>&1
if errorlevel 1 goto build_failed
echo [3/4] Building and running the 43 tests...
g++ -std=c++17 -O1 -Iinclude -Ithird_party -Itests %LIB% %TESTS% -o build\unit_tests.exe -pthread >> build\build_log.txt 2>&1
if errorlevel 1 goto build_failed
goto run

:run
build\unit_tests.exe
if errorlevel 1 goto tests_failed
echo.
echo [4/4] Comparing all policies and preparing the visual dashboard...
build\cache_sim.exe --compare-all
if errorlevel 1 goto run_failed
build\cache_sim.exe --export-web
if errorlevel 1 goto run_failed
echo.
echo ======================================================================
echo  Done! The program built and all 43 tests passed.
echo  - CacheSim Studio:   opening now (a Desktop shortcut is added too)
echo  - Results tables:    the "results" folder (open the .csv files in Excel)
echo  - Run again later:   build\cache_sim.exe
echo ======================================================================
echo.
if /i "%CI%"=="true" goto end
call "%~dp0CacheSim Studio.bat"
choice /c YN /n /m "Also open the text menu here? [Y/N] "
if errorlevel 2 goto end
build\cache_sim.exe
goto end

:build_failed
echo.
echo  BUILD FAILED. Compiler messages (also saved in build\build_log.txt):
echo.
type build\build_log.txt
goto end_fail

:tests_failed
echo.
echo  Some tests failed - see the messages above.
goto end_fail

:run_failed
echo.
echo  The simulator stopped with an error - see the message above.
goto end_fail

:no_compiler
echo.
echo  No C++ compiler was found on this PC.
echo.
echo  Install ONE of these (both are free), then double-click this file again:
echo.
echo    Option 1: Visual Studio Community
echo              https://visualstudio.microsoft.com/vs/community/
echo              In the installer, tick "Desktop development with C++".
echo.
echo    Option 2: MinGW g++ from MSYS2
echo              https://www.msys2.org/
echo              then add C:\msys64\ucrt64\bin to your PATH.
echo.
goto end_fail

:end
endlocal
if /i not "%CI%"=="true" pause
exit /b 0

:end_fail
endlocal
if /i not "%CI%"=="true" pause
exit /b 1
