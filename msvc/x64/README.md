# Native Windows x64 host SDK

This small SDK archive is exported by the native Windows x64 Preview build,
using the same checked import library and zlib as its OpenCPN host. Its exact
source revision, build URL, archive SHA256 and wxWidgets archive hashes are
recorded in `../../ci/windows64-sdk.json`.

The archive contains `opencpn.lib`, zlib's `z.lib` / `z.dll` and headers, licences,
and a per-file checksum manifest. The native export checks AMD64 COFF import
libraries and the PE32+ zlib DLL. The CircleCI build checks the archive and file
hashes again before compiling. wxWidgets 3.2.8 is obtained separately from its
official, checksum-pinned x64 release archives.

Keeping this generated SDK alongside the build recipe makes future CI runs
independent of expiring workflow artifacts. Update it only from a native host
build and retain the source revision and hashes. See `../../docs/windows-x64.md`.
