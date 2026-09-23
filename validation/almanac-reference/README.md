# Almanac reference audit

This is a reproducible, opt-in comparison for the 2.9 development Almanac.
The USNO fetcher is the only part that needs network access. It saves the
reference values and exact source URLs; the comparison test then runs offline.
It does not modify the plugin or installed OpenCPN.

After configuring the existing Linux test build with `test/CMakeLists.txt`, run:

```sh
cmake --build build-linux --target celestial_tests -j3
CELNAV_REFERENCE_SCHEDULE=/tmp/celnav-schedule.tsv \
  build-linux/test/celestial_tests \
  --gtest_filter=AlmanacReferenceAudit.ExportUtcAndDut1Schedule
python3 validation/almanac-reference/fetch_usno.py \
  /tmp/celnav-schedule.tsv /tmp/celnav-reference.tsv /tmp/celnav-manifest.json
CELNAV_REFERENCE_FIXTURE=/tmp/celnav-reference.tsv \
CELNAV_REFERENCE_OUTPUT=/tmp/celnav-comparison.tsv \
  build-linux/test/celestial_tests \
  --gtest_filter=AlmanacReferenceAudit.CompareStoredUsnoRowsAndPrintedAlmanac
python3 validation/almanac-reference/fetch_horizons_moon.py \
  /tmp/celnav-horizons.tsv
python3 validation/almanac-reference/summarize_usno.py \
  /tmp/celnav-comparison.tsv /tmp/celnav-manifest.json \
  /tmp/celnav-horizons.tsv /tmp/celnav-report.md
```

The stored results are in `results-2026-09-22/`; the report has its own
revision, separate from plugin version 2.9.0.0. The exact DE440s file used in
that run is identified by SHA-256 in the report. No kernel is stored here.

USNO accepts a fractional-second time in its input and echoes it in its
response, but direct comparison showed its celestial-navigation values were
calculated at the truncated whole second. The fetcher therefore queries the
two surrounding whole seconds and interpolates to the plugin's matched UT1.
Do not compare a UTC hour directly with an unadjusted USNO UT1 time.
