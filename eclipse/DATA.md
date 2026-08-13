# Offline eclipse data

The required base pack is the official JPL/NAIF `de440s.bsp` kernel plus the
small text manifest in `data/de440s.manifest`. Despite its name, `de440s` is
already the short DE440 subset: it covers 1850–2150 and is 32,726,016 bytes
(31.21 MiB). Creating another bespoke SPK subset would save little while
introducing avoidable provenance and interpolation risk. The kernel and both
optional runtime packs are versioned in this repository with Git LFS, so a
Git-LFS-enabled clone contains everything needed for completely offline use.

At runtime the engine never connects to a network. A user or package builder
imports `data/de440s.bsp` into the plugin's private eclipse-data directory.
The plugin checks its byte count, SHA-256 digest, DAF/SPK structure and segment
count before it is accepted. A truncated, modified or substituted file is
rejected. The same local import workflow applies to the optional PCK and LOLA
files.

The base installation budget is therefore approximately:

| Item | Installed size |
| --- | ---: |
| DE440s | 31.21 MiB |
| Manifest and time tables | under 1 MiB |
| Eclipse engine and ERFA code | under 5 MiB |
| DE440 lunar-orientation PCK (optional) | 12.27 MiB |
| Converted LOLA 64 ppd global limb grid (optional) | 506.25 MiB |

The optional grid is derived from NASA Goddard's 2024 MOON_PA 64 ppd LOLA
pixel grid. Its one-metre signed offsets preserve the source's useful vertical
precision while halving the floating-point source size. This global pack is
larger than an event-only profile, but it can refine C1–C4 for any observer and
any supported eclipse without another download or preprocessing run.

Build trees and the 701 MiB netCDF source terrain product are not runtime data. The guarded
working budget is 90 GiB, leaving 10 GiB below the user's requested 100 GB
ceiling. Normal development is expected to remain below 5 GiB even when a
LOLA source tile is staged temporarily.

Exact optional pack inputs
--------------------------

The lunar-orientation file is NAIF's
`moon_pa_de440_200625.bpc` (12,863,488 bytes). The terrain source is NASA
Goddard PGDA's `LDEM64_PA_pixel_202405.grd` (735,220,834 bytes), a
23,040 × 11,520 netCDF pixel grid in the same Moon principal-axes frame. The
developer-side converter produces `lola64-pa.bin` (530,841,624 bytes) as
signed one-metre offsets from a 1737.4 km lunar radius. Exact source and output
SHA-256 values are pinned in `data/*.manifest`; the plugin rejects any other
byte stream.

The source grid can be converted with:

```sh
lola-pack LDEM64_PA_pixel_202405.grd lola64-pa.bin
eclipse-cli verify-lola lola64-pa.bin
```

The netCDF source and converter are not installed with OpenCPN. The runtime
pack remains a deliberately separate optional data store at installation time
because it is useful only for terrain-sensitive contact refinements and is
much larger than the 31 MiB base ephemeris. It is included in the source
repository so an offline source build does not depend on a later download.

The kernel's authoritative source is NASA's Navigation and Ancillary
Information Facility (NAIF):
`https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp`.
The optional PCK is published at
`https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/moon_pa_de440_200625.bpc`,
and the LOLA source at
`https://pgda.gsfc.nasa.gov/data/LOLA_PA/LDEM64_PA_pixel_202405.grd`.
This URL is a packaging/provenance reference only; the application contains
no downloader and is fully functional with its installed data pack while
offline.
