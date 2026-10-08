# Numerical-audit follow-up — 11 September 2026

This follow-up is included in 2.8.5.3 while Rick's PR #296 remains unmerged.
It fixes the confirmed read-only audit findings without rewriting observations,
changing the XML format or replacing the enhanced lunar physical model.

## Corrections

1. **Generated almanacs:** convert legacy UTC date fields to actual instants
   before astronomical evaluation. Advance hours on instants, not local wall
   time, including the missing/repeated hours at DST transitions. Event output
   explicitly formats UTC. The generated manifest now uses the build version.
   Regenerate older almanacs made on non-UTC computers; in BST the old epoch
   could be one hour wrong (about 15 degrees of GHA).
2. **Ordinary Hc/Zn:** use east/north/up components and `atan2` to preserve the
   correct azimuth quadrant across the equator and dateline. At 0N, 0E with
   body GP 20N, 60W, Zn is 292.7959 degrees, not 247.2041; Hc remains
   28.02432 degrees. Undefined zenith/nadir/polar bearings display N/A.
3. **Sextant calibration:** local hour angle uses observer longitude minus
   body GP longitude, not their sum. Mirror-longitude tests check the symmetry.
4. **Running-fix uncertainty:** the inverse weighted normal matrix already
   contains the supplied angular variances. Remove the second dimensional
   variance factor; inflate by reduced chi-square when greater than one.
   Exact synthetic residuals no longer imply zero measurement uncertainty.
5. **Find Body:** Hc, Zn, estimated Hs and magnetic-model date use the effective
   corrected sight time. The recorded time, Hs, Ho and report are preserved.
   Remove two invalid vertical-sizer alignment flags from both generated UI
   code and its wxFormBuilder source, and fit the coordinate fields so the
   longitude hemisphere remains visible.
6. **Planner planetary parallax:** the ephemeris returns planetary distance
   in kilometres, not AU. Compute horizontal parallax from Earth radius divided
   by that distance. The ordinary sight/lunar planetary reductions already
   used kilometre distances correctly.
7. **Estimated Hs:** invert the actual forward altitude reduction by bracketed
   bisection on a working copy. This removes the mismatched inverse-refraction
   expression and makes the reported error a genuine forward-reduction residual.
   Invalid inputs, unbracketed solutions and failure to converge return N/A.
8. **Convergence safeguards:** running fixes, latitude-from-altitude and joint
   lunar-session fits cannot report success solely because the iteration limit
   was reached or damping made a rejected step tiny. Singular/nonconverged
   results are rejected; the planner explains an unsuccessful latitude solve.

## Validation

- New regressions reproduced six numerical failures before the fixes.
- 175 non-GUI tests pass; both opt-in GUI tests pass separately on GTK3.
- Ten numerical-audit tests also run in separate processes under UTC,
  Europe/London, America/New_York and Asia/Kolkata. January/August plus all
  four 2026 UK/US clock-change dates are checked hour by hour.
- 180 estimated-Hs cases cover Sun, Moon, Venus and Sirius, three limbs,
  natural/artificial/dip-short horizons and five altitudes. Each output goes
  through the production forward reduction; input preservation is checked.
- Running-fix tests cover exact/noisy data, unequal weights, doubled input
  uncertainty, singular geometry, invalid observations and iteration exhaustion.
- The independent Sun–Moon accuracy harness passes all 379 production checks
  over 123 scenarios. The frozen baseline retains its 15 expected failures;
  reference tolerances were not relaxed to obtain the production pass.
- Actual dialog tests check corrected-time Find Body values, lunar input
  preservation, Advanced-page layout and coastal waypoint/overlay behaviour.
  Native captures are inspected at large and small supported dialog sizes.
- Offline HTML/PDF/DOCX documentation is rebuilt and validated.

## Scope and limits

The Hs round trips prove consistency with the existing forward reduction, not
an independent atmospheric-refraction standard. The 379 reference checks
validate their sampled Sun–Moon cases, not every celestial/coastal workflow or
every date. Formal ellipses assume the entered angular errors and motion model;
systematic sextant, landmark, timing and atmospheric errors can dominate.
No universal or "space-agency level" operational accuracy is claimed.

Relevant mathematical references:
[USNO altitude/azimuth](https://aa.usno.navy.mil/faq/alt_az),
[NIST weighted least squares](https://itl.nist.gov/div898/handbook/pmd/section1/pmd143.htm),
[JPL Horizons](https://ssd.jpl.nasa.gov/horizons/manual.html).
