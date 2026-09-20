# Horizon Event correction and documentation audit

This release addresses [issue 326](https://github.com/rgleason/celestial_navigation_pi/issues/326)
on top of Rick's 2.8.8 baseline. The version is 2.8.9.0. The same correction
is retained locally in the unreleased 2.9.0 branch without a version bump.

## Findings

Bob's screenshots enter sunrise at 2025-07-16 16:51:01 UTC with a true bearing
of 67 degrees, and sunset at 2025-07-16 05:55:56 UTC with 293 degrees. Both use
3 m eye height, 11 C, 1010 hPa, 2 seconds time uncertainty, 2 degrees bearing
uncertainty and 0.1 arcminute horizon uncertainty.

The old solver chose just one minimum along the altitude circle. With Bob's
sunrise inputs it selected approximately 24.3546 S, 152.1325 W, while another
equally valid nominal solution is 19.8762 N, 170.3048 W. This is a real branch
ambiguity, not proof that the observer was in either location. The old
integer-trace regression samples accidentally favoured the sampled position
and did not expose it.

The large filled disc came from treating a degree of observed bearing error
as approximately 60 NM of position error. Bob's 2-degree input therefore
produced a radius near 120 NM regardless of latitude geometry. That is not a
conservative confidence region: uncertainty follows the altitude constraint
and can be much larger in weak geometry. Reducing horizon uncertainty does
not shrink this bearing-dominated disc.

The replacement analytically inverts the astronomical triangle and retains
all isolated latitude branches. The chart retains the time-based altitude
LOP and uncertainty band, with nominal candidate markers rather than a filled
disc. The dialog explains ambiguity, angular bearing uncertainty and the fact
that Fix uses the altitude constraint, not the optional bearing. It also
reports a sunrise/westerly or sunset/easterly mismatch. Horizon centrelines
and nominal markers follow a saved DR shift with their altitude band.

The event dates also matter: the reported sunset precedes sunrise by
10 h 55 m 05 s. Near Hawaii that sunset is the previous local evening. If a
sunrise-followed-by-sunset exercise was intended, the sunset UTC date needs
checking. A moving vessel's observations must be propagated to the same epoch;
a Sun azimuth is not a straight terrestrial bearing line through an assumed
boat position. These points do not excuse the software defects above.

The physical model remains upper-limb contact with the sea horizon, including
dip, refraction, semidiameter and solar parallax. The
[USNO rise/set definitions](https://aa.usno.navy.mil/faq/RST_defs) describe the
underlying contact/refraction convention. It is not a separate precision-fix
method attributed to a particular manual.

## Documentation audit

The 2.8.x bundled manual was still labelled 2.8.5.6. This pass updates the
cover/version/date, removes the experimental label, and corrects or expands:

- the two running-fix motion modes, automatic use of saved DR shifts, epoch
  selection, input reporting and exclusion by stationary algorithms;
- Find's manual, live boat, chart cursor, last fix and waypoint position sources;
- lunar-pair ordering by measured timing sensitivity rather than the removed
  quality score;
- Horizon Event ambiguity, chart meaning, UTC dates and method provenance;
- outdated running-fix statements in the AsciiDoc guide.

The existing index-error/calibration separation and Windows runtime notes
already describe the recent behaviour. The local 2.9 documentation additionally
needs its planning modes, ecliptic/Moon-path views, Remarks and sight-log
backup/import/restore controls; these are not 2.8 features.

HTML source, packaged HTML, PDF and DOCX are regenerated together. The manual
validator now checks the cover against CMake's version, rejects the obsolete
experimental label, and detects stale generated HTML and DOCX version labels.

## Verification

Regression coverage includes Bob's exact observations, both hemispheres,
fractional trace angles, the date line, equinox and tangent/degenerate cases,
forward altitude/azimuth agreement, mismatched event direction, absence of a
filled disc, and saved-shift marker movement. A GTK dialog smoke test verifies
the ambiguity and refraction guidance with Bob's inputs. The plugin builds
with its C++11 configuration; the complete CTest suite and manual structural
checks pass. Rendered manual pages and the dialog are visually reviewed.
