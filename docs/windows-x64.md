# Celestial Navigation 2.8.13 Windows x64 Alpha

The native Windows x64 job is an additional target; existing x86 jobs remain.
It uses the matching OpenCPN Windows x64 Preview SDK exported at core revision
94cf6909107e09df9793444e07ade87ad9816036, with the provenance, licences and
checksums in `ci/windows64-sdk.json` and `msvc/x64`. The bundled x86 API import
library is replaced only for an x64 MSVC build. An explicit native import
library is mandatory; an x86 fallback is rejected.

wxWidgets 3.2.8 headers, AMD64 development libraries and release DLLs come from
its official checksum-pinned release archives. CI checks SDK hashes and COFF
architecture, compiles with MSVC 2022 x64, rejects pointer truncation, verifies
native host imports and plugin entry points, and checks the packaged PE32+
DLL, root metadata, guide, manual PDF and analytical data. The editable DOCX
is retained separately. Metadata is `msvc-wx32-x64`, Windows version `10`,
architecture `x86_64`, plugin API `1.18` and plugin version `2.8.13.0`.

Windows x64 and Android are Alpha targets. A native build/package pass does not
qualify Windows GUI behavior, graphics drivers, charts or navigation devices.
Use the matching native x64 Preview host; ordinary Windows x86 OpenCPN cannot
load this DLL. Desktop numerical implementation is unchanged by the x64 target.
Publication remains on hold.

During initial qualification `validate_windows_x64_only` selects just the new
job to carry forward the existing 19 platform results. Its normal default is
false, so the complete validation suite has 20 jobs.

## Qualified build, 28 September 2026

[CircleCI workflow b0653c72](https://app.circleci.com/workflow/b0653c72-41e5-4524-8d17-7a5369ce94d0)
passed the native Windows x64 job148 at source1818c1e7f55f194bc50fba3ea0e89847b6aa88ea.
The downloaded package was independently checked for recorded hashes, root
metadata and its single AMD64 PE32+ plugin DLL. Package SHA256:
`12ca4e01647ce196862d6e7020940330b02c7d83949590dd4e12555b14f19937`.
DLL SHA256:
`eb16b7d02d465d04e0791077f6ca9d27bd1b3b61f254d76fb041021fdbdcb332`.

The other19 jobs passed once at8bdec0cb99f5600f742920f35624be5cabcc372e.
The intervening additions affect only native Windows x64 configuration/build
and documentation; all `src`, analytical data and existing-platform recipes
are unchanged. Their results are carried forward. No publication approval or
Windows GUI runtime acceptance is inferred from these build results.
