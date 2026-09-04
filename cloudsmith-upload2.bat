@echo off
setlocal enabledelayedexpansion

REM ---------------------------------------------------------------------------
REM  Local MSVC Cloudsmith Upload Script (Correct Double‑Tar Repack)
REM ---------------------------------------------------------------------------

REM Determine plugin root and build directory
set PLUGIN_ROOT=%~dp0
set BUILD_DIR=%PLUGIN_ROOT%build

cd /d "%BUILD_DIR%"

REM Identify XML and tarball
for %%f in (celestial_navigation_pi-*.xml) do set XML_FILE=%%f
for %%f in (celestial_navigation_pi-*.tar.gz) do set TARBALL=%%f

echo Using XML:      %XML_FILE%
echo Using TARBALL:  %TARBALL%

REM ---------------------------------------------------------------------------
REM 1. Prepare metadata.xml (copy into repack later)
REM ---------------------------------------------------------------------------

REM Extract outer tar.gz → inner.tar
echo Extracting outer tar.gz...
gzip -d -c "%TARBALL%" > inner.tar

REM ---------------------------------------------------------------------------
REM 3. Extract inner.tar into repack/
REM ---------------------------------------------------------------------------
echo Extracting inner tar...
rmdir /s /q repack 2>nul
mkdir repack

tar -xf inner.tar -C repack

REM ---------------------------------------------------------------------------
REM 4. Copy metadata.xml INTO repack directory (CRITICAL FIX)
REM ---------------------------------------------------------------------------
echo Copying metadata.xml into repack...
copy /Y "%XML_FILE%" "repack\metadata.xml"

REM ---------------------------------------------------------------------------
REM 5. Identify plugin directory inside inner tar
REM ---------------------------------------------------------------------------
for /d %%D in (repack\celestial_navigation_pi-*) do set PLUGIN_DIR=%%D

echo Plugin directory inside tar: %PLUGIN_DIR%

REM ---------------------------------------------------------------------------
REM 6. Rebuild inner tar with metadata.xml + plugin directory
REM ---------------------------------------------------------------------------
echo Rebuilding inner tar...
tar -cf new_inner.tar -C repack metadata.xml "%PLUGIN_DIR:repack\=%"

REM ---------------------------------------------------------------------------
REM 7. Compress new inner tar back into tar.gz
REM ---------------------------------------------------------------------------
echo Compressing new inner tar...
gzip -c new_inner.tar > "%TARBALL%"

REM ---------------------------------------------------------------------------
REM 8. Cleanup
REM ---------------------------------------------------------------------------
echo Cleaning up...
rmdir /s /q repack
del inner.tar
del new_inner.tar

echo SUCCESS: metadata.xml inserted into inner tar.
echo Tarball rebuilt correctly.

endlocal
