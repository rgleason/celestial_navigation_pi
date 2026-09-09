# Isolated celestial engine comparison lab

This is an experimental test facility, **not a plugin release or a replacement
for the installed plugin**. No numerical refinement has been applied yet.
First establish baseline/candidate equivalence; then change one component in
the candidate and compare both with independently saved reference results.

## What is frozen

`setup.sh` exports `src/` and `eclipse/` from commit
`85029280f8b224dccb55ee48f779e4373b870b82` (v2.8.5.1), including the ephemeris,
time conversion, ERFA and original `Sight.cpp` for audit. It creates two physical
copies, not symlinks. A separately copied headless adapter replaces the GUI
wiring; an opt-in bridge test checks it against the original plugin path.

The first pass builds the **Sun–Moon DE440 lunar distance and watch-session
engines only**. Ordinary altitude/fix solvers, analytical fallback and
planet/star lunars are not yet extracted. Full source export does not mean all
of those paths have been validated. Missing/out-of-range kernels fail explicitly
instead of silently selecting a different engine.

Layout after setup:

```text
.work/
  baseline/engine/    pinned production source (checksum guarded)
  baseline/driver/    frozen headless adapter and CMake file
  candidate/engine/   editable duplicate for numerical experiments
  candidate/driver/   editable duplicate of the adapter
  build-baseline/     independent executable and libraries
  build-candidate/    independent executable and libraries
  baseline.sha256    baseline source/adapter checksums
  report.json        outputs, differences, checks and provenance
```

Generated copies/builds are ignored by Git and can be reproduced from the
pinned revision. Keep any future candidate modifications as reviewed patches
on this experimental branch before removing generated directories. Setup
**refuses to overwrite** an existing `.work` directory. It never installs,
commits, pushes, or downloads anything automatically.

## Run locally

Requirements: Git (with the baseline commit), Bash, tar, GNU checksum utilities,
CMake >= 3.15, C++17/C compiler and Python >= 3.9. The lab does not require wx,
OpenCPN, GTest, a running GUI, or network access for normal runs.

From the repository root:

```sh
bash validation/engine-lab/build.sh
python3 validation/engine-lab/test_harness.py
python3 validation/engine-lab/run.py --kernel /absolute/path/to/de440s.bsp
```

Use the existing `eclipse/data/de440s.bsp` if available. The required SHA256 is
in `corpus.json`; do not replace the installed file. A missing or different
kernel is a failure, not a skipped comparison.

The runner removes display environment variables, checks frozen-source,
kernel and reference-response hashes, rebuilds both executables to prevent stale
binary comparisons, and compares every emitted output exactly by default.
Date arithmetic is explicit UTC with the production millisecond rounding.
Supported dates are post-1972; actual leap-second instants are excluded.

For a later numerical experiment, edit only `.work/candidate/engine/` and/or
`.work/candidate/driver/`, then run with `--allow-differences`. That option
allows and records A/B differences; **it does not disable independent-reference
or consistency checks**. Do not edit the original plugin source or baseline.
Do not change reference numbers or widen tolerances to make a candidate pass.

The reports preserve shared-reading residuals as `"nan"` (not counted again)
and unavailable uncertainties as `"inf"`, not fake zeroes. All other outputs
must be finite. Solver search bounds are deliberately narrow, ±60 s, for this
first corpus; multi-hour ambiguity exploration remains outside this test scope.
Reported solution equality covers emitted results, not every internal trace or
warning message.

## Observation and reference provenance

`corpus.json` separates these categories:

* **Reported real observations:** eight original Point Judith readings from
  [Bob's PDF](https://github.com/user-attachments/files/31986919/Lunars.Testing.Sept.8.2026.PDF.pdf).
  Individual times and angles are retained. Four distance readings are paired
  with before/after altitude readings using shared IDs: eight unique readings,
  not 24 independent measurements. The eight triples are merely an adapter to
  the existing session interface. Stationarity and 0.2′ uncertainties are
  explicit test assumptions. The published DR is not independently verified
  GNSS truth.
* **Published reduced example:** the PDF's averaged inputs remain a separate
  regression case, not extra independent observations. The printed Sun altitude
  is 51°58′, while the arithmetic mean of its two original readings is
  51°58.5′. Neither input set is silently “corrected” to match the other.
* **Incomplete intake:** August Linnman's published artificial-horizon readings
  are transcribed with a permanent source link. Missing precise ground truth,
  second-sight limb, weather and calibration context prevent their use as an
  accuracy pass/fail fixture. The observations span two days, not one watch.
* **Independent calculated reference:** five Sun/Moon configurations, spanning
  northern/southern/equatorial sites, a 400 m site and two dates. They test
  geocentric apparent RA/Dec and airless topocentric centre altitudes. They are
  not measurements and do not test atmospheric corrections or the sea horizon.
* **Synthetic consistency tests:** six limb/contact/motion scenarios use
  readings generated once by the frozen baseline, then passed to both solvers.
  They test recovery of an injected 23.75 s offset and position. They cannot
  establish independent physical accuracy.

The JPL reference responses are committed with exact queries, UTC acquisition
times, hashes, DE441/EOP headers and normalized numbers. Each run rederives the
expected numbers from the archived responses and verifies the reference grid.
To acquire a **new, separate** reference set explicitly:

```sh
python3 validation/engine-lab/fetch_horizons.py --output /new/reference/directory
```

This requires network access and refuses to overwrite an existing directory.
Reference acquisition fails if the service returns errors or an unexpected
table. A failed acquisition can leave an incomplete directory; it is not a
valid fixture until `manifest.json` exists and validates.

The predeclared limits are 0.1″ for geocentric RA/Dec and 1″ for the dated
topocentric altitude cases. Horizons' apparent-of-date frame differs slightly
from ERFA IAU06/00a (about 53 mas in the equinox origin). The baseline also omits
DUT1, polar motion and diurnal aberration. These budgets accommodate those
known differences for this grid; they are not universal accuracy guarantees.
Horizons DE441 and our DE440s are related ephemerides: the external pipeline is
independent, but its underlying astronomy is not wholly independent. A later
INPOP/IMCCE cross-check would add stronger model independence.

## Bridge and existing regression tests

Optional, using the existing plugin build and its normal dependencies:

```sh
cmake -S . -B build -DCELESTIAL_ENGINE_LAB_TESTS=ON
cmake --build build --target celestial_tests
build/test/celestial_tests --gtest_filter=EngineLabAdapter.*
ctest --test-dir build --output-on-failure
```

The bridge checks all nine `EphemerisSample` fields across four dates and four
offsets, including fractional seconds, midnight/month rollover and leap day,
to 1e-10 degree. It does not certify the plugin UI or independent astronomy.
The option defaults OFF and does not alter production numerical source.

## Expanded independent accuracy suite

The second pass adds 25 dated configurations (1972–2025), 120 archived Horizons
responses and separately archived DUT1 values for 17 epochs. It keeps the
original corpus and both engine snapshots unchanged. Run offline after setup:

```sh
python3 validation/engine-lab/test_accuracy.py
python3 validation/engine-lab/accuracy.py --kernel /absolute/path/to/de440s.bsp
```

**This suite currently exits 1 for genuine accuracy findings:** 123 scenarios,
758 checks, 30 failed checks (15 in each identical engine), no execution errors
and no A/B differences. See [the measured results](ACCURACY-RESULTS-20260909.md).
The original smaller suite still passes; neither result supersedes the other.
The machine-readable expanded report is `.work/accuracy-report.json`.

The new checks separate geocentric directions, airless topocentric directions,
limb/contact increments, atmospheric increments and inverse recovery. Independent
inverse inputs are constructed from JPL directions and diameters, never either
engine's forward output. Thirty-four inverse cases include an injected +23 s
watch correction and a stationary case with separately timed altitude readings.
Position and clock accuracy are assessed on the same branch, selected using
known reference position; this does not test automatic ambiguity resolution.

The public frozen inverse solver rejects zero pressure. These vacuum-reference
inverse tests therefore use 0.000001 hPa; a separate check bounds its altitude
effect below 0.000001 arcsecond at 4.5 degrees. This is a controlled numerical
limit, not a proposed observational pressure. Forward vacuum tests use exactly
zero. Inverse scans crossing the 2016 leap second are explicitly excluded because
elapsed-watch handling across a leap is outside the frozen adapter's domain;
their forward checks remain included and their failures remain reported.

Refraction is checked both against Horizons' approximate standard atmosphere and
against the published ray-traced model-atmosphere table in
[ERFA's refco source](https://github.com/liberfa/erfa/blob/master/src/refco.c).
The table values are verified against the frozen source documentation, not
generated by calling our bundled ERFA routine. The predeclared limits of 10″
and 2″ respectively are model-specific comparisons, not bounds on actual
atmospheric uncertainty. Refracted limb contact, sea-horizon dip and moving
observers still need independent benchmarks.

All limits and site-selection rules are in `accuracy-grid.json`. The raw
responses, exact queries and normalized values are checksum/epoch checked.
Optional acquisition of new fixtures is explicit and refuses existing paths:

```sh
python3 validation/engine-lab/fetch_accuracy.py --output /new/accuracy/directory
python3 validation/engine-lab/fetch_dut1.py --output /new/dut1/directory
```

The DUT1 projection is diagnostic only: it does not alter engine outputs,
silence failures or establish that all inverse-solve discrepancies have the
same cause. No new real-world observations have been promoted to absolute
truth; Point Judith remains a reported-observation regression with DR, not GNSS.

## Next development passes

1. Preserve this equivalence checkpoint before changing candidate mathematics.
2. Add a dated, explicit Earth-orientation input (DUT1 first), with recorded
   provenance and a clear separation between geocentric and topocentric checks.
   Match frames before interpreting small RA offsets as ephemeris errors.
3. Extend the independent limb/refraction checks to refracted contact and
   physical horizon observations; agreement with a baseline-generated sight
   is not sufficient evidence.
4. Curate more complete lunar sessions, especially Moon–star/planet observations
   and moving observers with independently recorded tracks, then extract those
   production paths. Do not invent missing metadata or equate a DR with truth.

The results here are a controlled development checkpoint, not justification
for changing v2.8.5.1 or claiming an absolute, error-free navigation solution.
