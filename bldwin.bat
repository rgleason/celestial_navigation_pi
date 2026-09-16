@echo on
setlocal enabledelayedexpansion


REM ---------------------------------------
REM Use Git for Windows tar, gzip and bash
REM ----------------------------------------
set PATH=C:\Program Files\Git\usr\bin;%PATH%

REM ------------------------------------------------------------
REM 0. Inject wxWidgets paths for PluginConfigure.cmake
REM ------------------------------------------------------------
set WX_ROOT=C:\Users\fcgle\source\ocpn_wxWidgets
set WX_LIB=%WX_ROOT%\lib\vc_dll
set WX_INC=%WX_ROOT%\include;%WX_LIB%\mswu

set CMAKE_PREFIX_PATH=%WX_ROOT%
set CMAKE_INCLUDE_PATH=%WX_INC%
set CMAKE_LIBRARY_PATH=%WX_LIB%


REM ------------------------------------------------------------
REM 1. Define paths
REM ------------------------------------------------------------
set PLUGIN_ROOT=%cd%
set BUILD_DIR=%PLUGIN_ROOT%\build
set PLUGIN_BUILD=%BUILD_DIR%\RelWithDebInfo

set OCPN_ROOT=C:\Users\fcgle\source\opencpn
set OCPN_BUILD=%OCPN_ROOT%\build\RelWithDebInfo
set OCPN_SOLUTION=%OCPN_ROOT%\build\OpenCPN.sln

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
REM 4. Generate Tarball and XML metadata
REM ------------------------------------------------------------
echo Running CPack to generate tarball and XML metadata...
cmake --build . --config RelWithDebInfo --target package

REM ------------------------------------------------------------
REM 5. Insert metadata.xml into tarball (DOUBLE‑TAR REPACK)
REM ------------------------------------------------------------
REM ------------------------------------------------------------
REM Insert metadata.xml into tarball (DOUBLE‑TAR REPACK)
REM ------------------------------------------------------------

set PATH=C:\Program Files\Git\usr\bin;%PATH%

for %%f in (celestial_navigation_pi-*.xml) do set XML_FILE=%%f
for %%f in (celestial_navigation_pi-*.tar.gz) do set TARBALL=%%f

echo Using XML: %XML_FILE%
echo Using TARBALL: %TARBALL%

REM Extract outer tar.gz → inner.tar
gzip -d -c "%TARBALL%" > inner.tar

REM Extract inner.tar into repack/
rmdir /s /q repack 2>nul
mkdir repack
tar -xf inner.tar -C repack

REM Copy metadata.xml INTO repack directory
copy /Y "%XML_FILE%" "repack\metadata.xml"

REM Identify plugin directory
set PLUGIN_DIR=
for /d %%D in (repack\*) do (
    echo %%~nxD | findstr /i "celestial_navigation_pi" >nul
    if !errorlevel! == 0 (
        set PLUGIN_DIR=%%~nxD
    )
)

if "%PLUGIN_DIR%"=="" (
    echo ERROR: Plugin directory not found!
    exit /b 1
)

echo Plugin directory: %PLUGIN_DIR%

REM Derive correct inner tar name from tarball
set INNER_NAME=%TARBALL:.gz=%

echo Correct inner tar name: %INNER_NAME%

REM Rebuild inner tar with correct name
tar -cf "%INNER_NAME%" -C repack metadata.xml "%PLUGIN_DIR%"

REM Safety check
for %%A in ("%INNER_NAME%") do set SIZE=%%~zA
echo Inner tar size: %SIZE%

if "%SIZE%"=="0" (
    echo ERROR: Inner tar is empty — aborting.
    exit /b 1
)

REM Recompress using correct name
gzip -c "%INNER_NAME%" > "%TARBALL%"

REM Cleanup
del "%INNER_NAME%"

REM Cleanup
rmdir /s /q repack
del inner.tar

echo SUCCESS: metadata.xml inserted.

REM ------------------------------------------------------------
REM 6. Copy plugin DLL + PDB into OpenCPN plugin folder
REM ------------------------------------------------------------
echo Deploying plugin DLL and PDB into OpenCPN plugin directory...

copy /Y "%PLUGIN_BUILD%\celestial_navigation_pi.dll" "%OCPN_BUILD%\plugins\"
copy /Y "%PLUGIN_BUILD%\celestial_navigation_pi.pdb" "%OCPN_BUILD%\plugins\"

echo Plugin deployed successfully.

REM ------------------------------------------------------------
REM 7. Run Cloudsmith upload script
REM ------------------------------------------------------------
echo Running Cloudsmith upload script...
REM bash "%PLUGIN_ROOT%\cloudsmith_upload2.sh"

echo Cloudsmith upload complete.

endlocal
