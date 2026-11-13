@echo off
REM run_all.bat - Windows: build with CMake, test, and regenerate all results.
cd /d "%~dp0\.."
cmake -S . -B build || exit /b 1
cmake --build build --config Release || exit /b 1
ctest --test-dir build -C Release --output-on-failure || exit /b 1
set EXE=build\cache_sim.exe
if not exist %EXE% set EXE=build\Release\cache_sim.exe
%EXE% --train
%EXE% --evaluate
%EXE% --compare-all
%EXE% --benchmark
%EXE% --trace --capacity 3 > results\trace_example.txt
echo All done. See results\
