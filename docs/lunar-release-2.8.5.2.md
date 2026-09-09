# Celestial Navigation 2.8.5.2

## Production changes

The enhanced Sun–Moon lunar engine now calculates light time to each trial
observer on the WGS84 ellipsoid and combines orbital and station-rotation
velocity in apparent aberration. It uses the same model for individual,
time-tagged and watch-sequence solves. Celestial angular relationships remain
vector/spherical geometry; the observer's position and local horizon use the
ellipsoid. Raw sight measurements and recorded watch times are not rewritten.
The existing sight XML format and saved-solution workflow remain compatible.

This is the tested observer-astrometry experiment promoted to production,
not a replacement ephemeris or a change to reference values/tolerances.
DE440s must already be installed for this Sun–Moon path. Moon–star,
Moon–planet, analytical fallback and ordinary altitude reduction have not
been upgraded to this model. The frozen 2.8.5.1 comparison engine remains
unchanged in the engine lab.

## Offline Earth-rotation data

DUT1 means UT1 minus UTC: the small correction relating observed Earth
rotation to civil time. The binary includes 20,343 daily IERS records,
1972-01-01 through 2027-09-11 UTC (last epoch at midnight). Final C04 history
is followed by rapid and predicted values. Provenance, source URLs and
SHA-256 hashes are in `data/earth-orientation-provenance.json`.

The solver selects data for each sight/trial's date, not today's date.
Interpolation uses UT1 minus TAI so a UTC leap is not smeared into the
preceding day. The application does not represent the leap second itself as
23:59:60; inverse searches straddling that instant remain outside validation.

There is **no expiry lockout**. Beyond available records, solving continues
with UT1 = UTC and the available UTC leap history. Reports and lunar results
warn that precision is reduced; formal fit uncertainty excludes this
approximation. Future Earth rotation cannot be known precisely decades ahead,
and today's leap history cannot predict all future UTC changes. The fallback
does not promise the same sub-arcsecond accuracy as dated IERS data.
DE440s' separate approximate 1850–2150 coverage is unchanged.

### Optional user update

Open **Lunar Tools → Advanced → Check / download update…**. This is the only
action here that connects to IERS. It fetches the official `finals2000A.all`
file (about 4 MB), validates its dates, daily continuity, quality flags,
numeric bounds and leap history, then prepares an immutable in-memory table
and atomically saves the validated bytes. No compiler or restart is needed.
Repeated equivalent data is reported as already up to date. A file with
shorter coverage cannot replace the active/bundled coverage.

**Import local file…** accepts the same official file transferred from another
computer. The installed file is stored under the plugin's private data folder
as `earth-rotation/finals2000A.all`. Startup loads this local file only; no
scheduled/startup network check occurs. Solving never downloads anything.
Updates replace rapid/predicted values and extend coverage without discarding
the final C04 historical solution. Bulletin B final values are preferred when
present in the update. Validated future UTC leap changes can be used without
recompiling; the last known leap offset is retained after DUT1 coverage ends.

A failed/cancelled download, invalid file or failed save leaves the active
table unchanged. Invalid cached data at startup is ignored in favour of the
bundle. The bundled data is always available independently of cache files.
Downloading is optional: high-accuracy offline operation is already supported
within bundled coverage.

## Validation and limits

The production accuracy run uses the actual production engine and its dated
bundled DUT1, **not the lab's externally supplied DUT1 values**. Against the
archived independent JPL Horizons and published refraction references:

- 123 scenarios; 379 checks per engine.
- Production: all 379 checks pass; frozen baseline retains its 15 failures.
- Maximum airless altitude discrepancy about 0.228 arcseconds.
- Maximum theoretical inverse clock error about 0.028 seconds and position
  error about 0.0075 NM (14 metres), on this grid.
- No thresholds were relaxed and no reference observations were edited.

The [complete production report](../validation/engine-lab/production-2.8.5.2-results.json)
records source/binary hashes, every check and the retained baseline failures.
The release CTest run passed all 158 non-GUI tests plus the UTF-8 source guard;
the opt-in GUI test passed separately (159 C++ tests exercised in total).

These are model/reference comparisons, not a promise of 14-metre real sextant
fixes. Theoretical inverse cases use independent JPL observations; ordinary
sextant, horizon and atmospheric errors remain dominant. Polar motion,
geoid/local vertical, resolved lunar limb topography and full atmospheric
ray tracing are not modelled. Multiple mathematical solutions still require
independent navigation judgement.

Additional tests cover dated interpolation/leaps, damaged update rejection,
atomic-save failure, offline download failure, raw-input preservation and
successful fallback solves in 2035, 2075, 2100 and 2140. Those future tests
establish availability/self-consistency, not independent future accuracy.

The actual Lunar Tools dialog was GUI-tested on Linux/wxGTK3 at 1120×780 and
its 880×650 minimum: the Advanced controls fit with no overlap or horizontal
scrolling. Native-widget renders were visually inspected. The main action
column is unchanged. Windows/macOS GUI behaviour still needs platform testing.

### Reproduce

Build with `OCPN_BUILD_TEST=ON` and `CELESTIAL_ENGINE_LAB_TESTS=ON`, with the
verified DE440s test kernel available as `eclipse/data/de440s.bsp`. Run:

```sh
ctest --test-dir build --output-on-failure
CELESTIAL_RUN_UI_TESTS=1 build/test/celestial_tests --gtest_filter=LunarUiSmoke.TimeEntryAndResultModesPreserveRecordedInputs
CELESTIAL_IERS_TEST_FILE=/path/to/finals2000A.all build/test/celestial_tests --gtest_filter='Dut1*'
python3 validation/engine-lab/accuracy.py --kernel eclipse/data/de440s.bsp --production-build build --gate candidate --output validation/engine-lab/.work/accuracy-production-2.8.5.2.json
```

The lab's frozen baseline must first be prepared as documented in
`validation/engine-lab/README.md`. GUI tests need an accessible display and
use isolated mock-host state, never the user's sights. Their screenshots are
written to `/tmp/celestial-advanced-1120.png` and `-880.png`.

`tools/update_dut1.py --archive /new/archive/directory` is a separate explicit
maintainer acquisition tool for refreshing the compiled bundle. It is never
run by the build or by the plugin. A maintainer refresh also requires reviewing
the built-in leap history and re-running the accuracy gate.

Primary references: [IERS products](https://data.iers.org/eop.php),
[official finals2000A format](https://maia.usno.navy.mil/ser7/readme.finals2000A),
[JPL Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html),
[ERFA observer station model](https://github.com/liberfa/erfa/blob/master/src/pvtob.c),
[ERFA aberration](https://github.com/liberfa/erfa/blob/master/src/ab.c).
