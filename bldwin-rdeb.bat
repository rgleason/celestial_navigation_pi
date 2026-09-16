@echo on
setlocal

REM ------------------------------------------------------------
REM 1. Define paths
REM ------------------------------------------------------------
set PLUGIN_ROOT=%cd%
set BUILD_DIR=%PLUGIN_ROOT%\build
set PLUGIN_BUILD=%BUILD_DIR%\RelWithDebInfo

set OCPN_ROOT=C:\Users\fcgle\source\opencpn
set OCPN_BUILD=%OCPN_ROOT%\build-win32\RelWithDebInfo
set OCPN_SOLUTION=%OCPN_ROOT%\build-win32\OpenCPN.sln

REM wxWidgets (used by CMake)
set wxDIR=C:\Users\fcgle\source\ocpn_wxWidgets
set wxWIN=C:\Users\fcgle\source\ocpn_wxWidgets
set wxWidgets_ROOT_DIR=%wxWIN%
set wxWidgets_LIB_DIR=%wxWIN%\lib\vc_dll

REM ------------------------------------------------------------
REM 2. Remove build directory if it exists
REM ------------------------------------------------------------
if exist "%BUILD_DIR%" (
    echo Removing existing build directory...
    rmdir /S /Q "%BUILD_DIR%"
)

echo Creating fresh build directory...
mkdir "%BUILD_DIR%"

REM ------------------------------------------------------------
REM 3. Run CMake configure + build
REM ------------------------------------------------------------
cd "%BUILD_DIR%"

echo Configuring plugin build...
cmake -T v143 -A Win32 -DOCPN_TARGET=MSVC ..

echo Building plugin (RelWithDebInfo)...
cmake --build . --config RelWithDebInfo

REM ------------------------------------------------------------
REM 4. Copy plugin DLL + PDB into OpenCPN plugin folder
REM ------------------------------------------------------------
echo Deploying plugin DLL and PDB into OpenCPN plugin directory...

copy /Y "%PLUGIN_BUILD%\celestial_navigation_pi.dll" "%OCPN_BUILD%\plugins\"
copy /Y "%PLUGIN_BUILD%\celestial_navigation_pi.pdb" "%OCPN_BUILD%\plugins\"

echo Plugin deployed successfully.

REM ------------------------------------------------------------
REM 5. Launch Visual Studio with OpenCPN solution
REM ------------------------------------------------------------
echo Launching Visual Studio for debugging...

start "" "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" "%OCPN_SOLUTION%"

echo.
echo Visual Studio is now open.
echo Select RelWithDebInfo and press F5 to debug OpenCPN with your plugin.
echo.

endlocal
