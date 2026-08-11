@echo off
REM build.bat -- builds the DLL, the launcher, and the Java jar into dist\.
REM
REM Needs: CMake, a 64-bit C++ compiler (MSVC or mingw-w64), and a JDK 17+.
REM Everything lands in dist\ ready to run. See README.md if any of this errors.

setlocal

if "%JAVA_HOME%"=="" (
  echo JAVA_HOME is not set. Point it at a JDK 17 or newer, e.g.
  echo    set JAVA_HOME=C:\Program Files\Java\jdk-21
  exit /b 1
)

echo [1/3] native ...
cmake -S . -B build -A x64 || exit /b 1
cmake --build build --config Release || exit /b 1

echo [2/3] java ...
if not exist build\classes mkdir build\classes
dir /s /b java\*.java > build\sources.txt
"%JAVA_HOME%\bin\javac" -d build\classes @build\sources.txt || exit /b 1
"%JAVA_HOME%\bin\jar" --create --file build\dist\kewlklient.jar -C build\classes . || exit /b 1

echo [3/3] config ...
if not exist build\dist\kewlklient.ini copy kewlklient.ini build\dist\ >nul

echo.
echo done -- everything is in build\dist\
echo   1. edit build\dist\kewlklient.ini and set java= to your JDK
echo   2. start the game and log in
echo   3. run build\dist\KewlKlient.exe and press the button
endlocal
