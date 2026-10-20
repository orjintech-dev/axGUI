@echo off
rem WebAssembly build (Windows). Requires emsdk (https://github.com/emscripten-core/emsdk),
rem CMake and Ninja on PATH (the ones bundled with Qt Creator work too).
rem   emsdk install latest  &&  emsdk activate latest   (once)
rem Usage:  web\build-web.bat [emsdk folder]     (default: C:\emsdk)
setlocal
set EMSDK_DIR=%~1
if "%EMSDK_DIR%"=="" set EMSDK_DIR=C:\emsdk
call "%EMSDK_DIR%\emsdk_env.bat" >nul || (echo emsdk not found: %EMSDK_DIR% & exit /b 1)
cd /d "%~dp0.."
call emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build-web -j || exit /b 1
echo.
echo Done: build-web\demos\<demo>\index.html
echo To run:  python -m http.server 8080 -d build-web\demos   and open  http://localhost:8080/showcase/
echo (emrun build-web\demos\showcase\index.html also works; opening the file directly does not: browsers won't load .data/.wasm over file://)
