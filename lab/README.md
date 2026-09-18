# Celestial Navigation Lab 0.1

Celestial Navigation Lab is a standalone, offline comparison tool. It does not
need OpenCPN and does not alter the installed Celestial Navigation plugin.
It compiles the plugin's current `Sight` and ephemeris sources into a small
desktop front end so that a specific case can be rerun in three modes:

- **Automatic (2.9):** use DE440s for a supported body when a verified kernel
  is available, otherwise use the analytical calculation.
- **DE440s required:** never represent an analytical fallback as a DE440s
  result. A missing, corrupt, unsupported or out-of-coverage kernel is reported.
- **Analytical:** force the established analytical calculation even with a
  verified DE440s kernel present.

The v0.1 body selector contains Sun, Moon, Mercury and Venus. These are the
centres supported by the compact DE440s kernel. The two v0.1 test types are
**Altitude sight** (including Hs-to-Ho corrections, Hc, Zn and intercept) and
**Ephemeris / look angle** (GHA, declination, Hc and Zn). A single lunar-distance
test, multi-sight sets and batch reference management are planned for a later
lab version; v0.1 does **not** claim to test them.

## Install on macOS

1. Download `Celestial-Navigation-Lab-0.1.0-macOS-universal.dmg` and its
   `.sha256` file from the [experimental Lab 0.1.0 release](https://github.com/pob220/celestial_navigation_pi/releases/tag/lab-v0.1.0).
   Verify the checksum if possible. These are Lab downloads, not OpenCPN
   plugin packages.
2. Open the DMG and drag **Celestial Navigation Lab** to **Applications**.
3. This experimental build is ad-hoc signed, not Apple-notarized. If macOS
   blocks first launch, use Finder's **Open** contextual action and review the
   warning. Do not bypass a warning for a file from an untrusted source.
4. The app works analytically without DE440s. To compare the DE440s path,
   select an existing `de440s.bsp` file using **Choose…**. The app verifies
   the file locally before using it. It never downloads data automatically.

The compact kernel covers 1850–2150, but this app's automatic UTC-to-TT
conversion for DE440s requires 1972 or later. Outside supported coverage,
**Automatic** falls back to the analytical path and **DE440s required** reports
why it cannot run. The app uses fractional UTC seconds (milliseconds in its
current input and plugin interface). A lunar limb terrain/orientation pack is
neither required nor used in v0.1.

## Run a case

Enter the UTC date and time, body and signed decimal coordinates (north and
east positive). For an altitude sight enter sextant altitude in decimal
degrees, limb, eye height, index correction in arcminutes, pressure in hPa and
temperature in °C. **Calculate** runs analytical and DE440s side by side when
the latter is available. The selected primary mode is reported below the
table. **Detailed calculation** text is displayed at the bottom.

Optional reference values are entered in the editable **Reference** column as
decimal degrees, except intercept in nautical miles. Differences are shown in
arcminutes, except intercept differences in nautical miles. Circular GHA/Zn
differences use the shortest signed angle. Record the publication, page and
conventions in **Reference source / page**. Only compare values defined the
same way: geocentric versus topocentric, apparent versus geometric, centre
versus limb, UT1 versus UTC, and the publication's rounding precision all
matter. Blank reference cells mean no reference comparison.

**Save test** writes the inputs and reference values to a `.cnlab` file.
**Open test** reloads that file and recalculates it. **Export results** writes
a tab-separated table for a report. A `.cnlab` case may include a local kernel
path; on another machine, select that machine's verified kernel after opening
the case. No sight is added to OpenCPN.

## Build / verify from source

On Linux, configure this repository with its normal plugin dependencies and
`-DOCPN_BUILD_TEST=ON -DCELESTIAL_BUILD_LAB=ON`, then build target
`celestial_navigation_lab`. The executable supports `--smoke-test`, which
checks analytical calculation, explicit forced-DE failure and Automatic
fallback. Run the existing CTest suite as a regression check.

On the repository's macOS CI runner, `ci/build-celestial-lab-macos.sh` builds
the universal `.app`, bundles its non-system libraries and analytical data,
ad-hoc signs it, and creates a drag-and-drop DMG. The script writes only to
its dedicated build and artifact directories, apart from installing its
declared build dependencies in the ephemeral CI environment. It does not
publish an OpenCPN plugin package.

This is a validation tool, not a certified navigation instrument. Confirm
results against appropriate independent references before drawing accuracy
conclusions.
