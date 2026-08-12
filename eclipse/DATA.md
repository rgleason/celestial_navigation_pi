# Offline eclipse data

The required base pack is the official JPL/NAIF `de440s.bsp` kernel plus the
small text manifest in `data/de440s.manifest`. Despite its name, `de440s` is
already the short DE440 subset: it covers 1850–2150 and is 32,726,016 bytes
(31.21 MiB). Creating another bespoke SPK subset would save little while
introducing avoidable provenance and interpolation risk.

At runtime the engine never connects to a network. A user or package builder
places `de440s.bsp` in the plugin's private eclipse-data directory. The plugin
checks its byte count, SHA-256 digest, DAF/SPK structure and segment count
before it is accepted. A truncated, modified or substituted file is rejected.

The base installation budget is therefore approximately:

| Item | Installed size |
| --- | ---: |
| DE440s | 31.21 MiB |
| Manifest and time tables | under 1 MiB |
| Eclipse engine and ERFA code | under 5 MiB |
| Optional event-specific lunar-limb packs | target under 25 MiB total |

Build trees and any source terrain products are not runtime data. The guarded
working budget is 90 GiB, leaving 10 GiB below the user's requested 100 GB
ceiling. Normal development is expected to remain below 5 GiB even when a
LOLA source tile is staged temporarily.

The kernel's authoritative source is NASA's Navigation and Ancillary
Information Facility (NAIF):
`https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp`.
This URL is a packaging/provenance reference only; the application contains
no downloader and is fully functional with its installed data pack while
offline.
