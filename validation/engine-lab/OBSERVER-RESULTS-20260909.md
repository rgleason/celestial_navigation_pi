# Observer-astrometry experiment — 9 September 2026

**The enhanced candidate passes all 379 checks in the expanded accuracy suite,
including all 15 checks which failed in the frozen engine.** The 123 scenarios
produce 758 checks across both engines. The baseline still has its 15 failures;
the report preserves them. No observations, reference angles, selection rules
or tolerances were changed.

## Measured improvement

| Maximum absolute error in this grid | Frozen baseline | Enhanced candidate | Unchanged gate |
| --- | ---: | ---: | ---: |
| Airless altitude | 7.19767″ | 0.22633″ | 1″ |
| Airless lunar separation | 0.28617″ | 0.00621″ | 1″ |
| Recovered clock correction | 0.53516 s | 0.02735 s | 1 s |
| Recovered position | 0.14136 NM (~262 m) | 0.00737 NM (~14 m) | 0.1 NM |

These are maximum errors across the existing independent theoretical fixtures,
not measured accuracy of real sextant observations. The 34 inverse cases still
select the branch nearest known reference position; automatic ambiguity
resolution is not tested.

## Controlled sequence

| Candidate configuration | Failed candidate checks |
| --- | ---: |
| Enhancements disabled | 15 |
| Dated DUT1 only | 6 |
| DUT1 + incremental diurnal aberration | 12 |
| DUT1 + observer-specific light time and combined aberration | **0** |

DUT1 alone clears all nine altitude failures. The remaining six position
failures are not exactly the original six: the March 2025 eastern cases cross
the gate while the 2020 cases improve. Incremental diurnal aberration alone is
also incomplete and is retained as an explicit diagnostic, not the recommended
configuration.

The baseline checks are identical across all four stages. With enhancements
disabled, **every emitted A/B output is identical** throughout the expanded
suite. The original 18-scenario / 86-check regression suite also passes with
enhancements disabled.

## What changed in the candidate

1. **Earth rotation uses UT1 rather than assuming UTC.** The experiment accepts
   a dated, explicit UT1−UTC input and applies it to the Earth-rotation matrix.
   TT/TDB ephemeris timing is not shifted by this correction. Inputs come from
   separately archived, checksum/epoch-verified JPL quantity 49 responses.
2. **The observer enters the astrometry calculation before aberration.** For
   each trial position, the adapter obtains WGS84 station position/velocity,
   transforms them to the celestial frame, and constructs the observer's
   barycentric position and velocity. It iterates target light travel time to
   that observer, then applies aberration using the combined orbital and station
   velocity. The resulting direction is transformed into the local horizon.
3. **The same observer-aware provider is used by forward and inverse paths.**
   Each separately timed altitude uses its own epoch. Raw observation fields
   remain untouched; only the predicted angles change.

The old route subtracts a station vector from an already-aberrated geocentric
direction. For a nearby body such as the Moon, that is not equivalent to the
observer-specific ordering above. The controlled runs show that merely adding
one small angular correction does not resolve all of the coupled errors.

The implementation uses the existing, unmodified ERFA routines for
[station position/velocity](https://github.com/liberfa/erfa/blob/master/src/pvtob.c)
and [aberration](https://github.com/liberfa/erfa/blob/master/src/ab.c), with the
existing DE440s kernel. Reference definitions, including DUT1 and observer
quantities, are documented in the
[Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html).

## Verification and preservation

- 379/379 enhanced-candidate accuracy checks pass; baseline failures remain
  visible. Independent limb/contact and refraction gates still pass.
- Seven candidate control tests cover analytic aberration, observer callback
  position/epoch propagation, geographic rotation, unchanged geocentric
  ephemerides, timezone invariance, fractional midnight rollover, invalid or
  double corrections, and reproducible patch application.
- The original ten harness tests and twelve accuracy-harness tests pass.
- Existing CTest targets `celestial_tests` and `ui_utf8_literals` pass. These
  exercise the existing plugin build, not a GUI installation of this experiment.
- The frozen baseline manifest validates. Production `src/` and `eclipse/`,
  installed binaries and plugin version are unchanged.
- The candidate changes are saved as a source patch with before/after SHA256
  values under `experiments/`. The application script refuses to overwrite
  unrelated experimental work. Machine-readable ablation results are saved in
  [experiments/observer-results.json](experiments/observer-results.json).

## Limits and next step

This is a successful **experimental accuracy checkpoint**, not readiness for
production navigation. DUT1 is supplied externally and held constant over each
short fixture. General Earth-orientation coverage, provenance, interpolation,
offline behaviour and leap-boundary handling remain to be designed.

Polar motion is still zero; the remaining altitude residuals have not been
attributed conclusively. The observer model does not add a complete gravitational
light-deflection model or relativistic terrestrial-to-barycentric station
transformation. Semidiameters retain the geometric spherical-disc convention,
not resolved apparent-limb ray tracing. Refraction is unchanged. Existing
exclusions for inverse leap crossing, weak geometry, physical sea-horizon dip,
moving-observer truth and star/planet lunars still apply.

The geometric APIs can accept trial positions throughout a solve, but no new
GNSS-verified real observing session has been validated here. The references
also share related DE440/DE441 astronomy, so another independent pipeline and
new held-out observations remain valuable before integration.

Recommended next step: preserve this checkpoint, add held-out configurations
and independent observing sessions, then design production Earth-orientation
handling. Do not replace v2.8.5.1 on the strength of this grid alone.
