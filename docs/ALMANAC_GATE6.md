# Voyage Almanac: completed calculator-free implementation

## Scope and safety level

The generator produces an independent, completely offline **OpenCPN Voyage
Almanac**. It now supports both scientific-calculator and calculator-free
paper backups. Calculator-free means that, after printing, a navigator needs
only the document, sextant, accurate watch, pencil and ordinary plotting tools:
no trigonometric calculator, computer, internet service or external book.

The completed generator includes:

- Passage Brief, Voyage Almanac and Celestial Navigator presets;
- route, fixed-position, latitude-band and global planning coverage;
- saved-route selection and a route-context-menu shortcut which preselects
  the route and default 150 NM planning corridor;
- hourly UTC-labelled Sun, Moon and Aries data;
- hourly Venus, Mars, Jupiter and Saturn GHA/declination;
- the 57 Nautical Almanac navigational stars, with Polaris separately stated;
- rise/set, twilight, transit, Moon information and ranked sight planning;
- lunar-distance opportunities and rate/sensitivity;
- correction, direct-reduction, plotting, running-fix, noon, Polaris, lunar
  and emergency reference pages;
- sight, running-fix, noon/Polaris, lunar-sequence and watch-log forms;
- 1-366 day output, daily or compacted planning cadence and monthly star epochs;
- optional voyage/date-aware direct Hc/Zn tables for rapid lookup;
- optional full-declination direct tables for deliberately large editions;
- a universal half-minute Ageton log-cosecant/log-secant table as the
  independent off-track fallback;
- 60 minute/second increment and v/d pages, dip/refraction/Moon correction
  tables and calculator-free instructions;
- altitude, lunar-distance/rate and correction overview graphs which aid
  planning but never replace precision tables;
- A4, Letter and A5 output plus duplex and booklet/signature imposition;
- exact structural page/PDF-page/sheet estimates, first-page preview and
  atomic PDF output.

The universal ephemeris is not filtered by a route. Route coverage affects
only location-dependent planning pages.

The generated tables are original numerical output, not scans or page copies.
The compact universal method follows Arthur A. Ageton's public 1931
secant-cosecant construction; direct Hc/Zn tables serve the same practical
purpose as NGA Pub. 229 but use a newly generated voyage-aware layout. Hourly
quantities, increments and v/d notation follow conventional Nautical Almanac
practice.

## Output spectrum

The eventual generator should retain a deliberately wide scope:

- **Passage Brief:** a small location-aware planning document;
- **Voyage Almanac:** the recommended route/date/corridor-specific paper
  backup, with compact tables and only useful bodies in planning lists;
- **Calculator-free Voyage Almanac:** adds every interpolation, correction
  and sight-reduction table required for pencil-only work; and
- **Full global annual almanac:** the largest, route-independent option for a
user who explicitly wants it. Full direct Hc/Zn coverage can run to tens of
thousands of pages, so the live estimate makes that choice explicit.

Compaction must never create a hidden dependency. Route and observability
filters may shorten sight suggestions, charts and duplicated planning
material, while universal data for every body included in the document stays
available if the vessel leaves the expected corridor. The paper-only safety
level requires a machine-checkable dependency manifest before it is described
as self-contained. Direct tables are a convenience layer; the universal
Ageton pages remain available if a route-pruned direct table no longer applies.

## Time provenance

Rows are labelled in UTC. Earth rotation is evaluated using UTC plus the
request's DUT1 value. No DUT1 is downloaded at runtime. If the user does not
provide one, the cover records the explicit 0.000-second assumption and a
warning.

## Release-build performance baseline

Measured on the development laptop on 2026-08-16 using the automated
`AlmanacPerformance` regression tests and the Release build:

| Voyage | Pages | Document assembly | PDF rendering | PDF size |
|---|---:|---:|---:|---:|
| 14 days, daily visual planning | 92 | 1.566 s | 0.010 s | 876,371 bytes |
| 93 days, daily visual planning | 487 | 10.321 s | 0.061 s | 5,320,500 bytes |
| 366 days, 30-day planning cadence | 793 | 18.984 s | not written in benchmark | — |

Peak resident memory for the 93-day test process was about 67,708 KiB. This
includes the complete test executable and test framework, so it is a useful
upper-context measurement rather than a clean incremental allocation figure.

The near-linear result predicts that ordinary annual ephemeris/planning output
would take tens of seconds, not hours, before full tabular sight-reduction
material is added.

## Performance and acceleration policy

Profile before changing the numerical engine. The current PDF writer accounts
for less than one percent of elapsed time; astronomical document assembly is
the useful target.

1. Cache shared body/epoch states so route-dependent altitude/azimuth work can
   reuse geocentric GHA/declination.
2. If profiling justifies it, generate days as independent jobs in a bounded CPU worker pool, then merge
   their semantic pages in date order. First confirm that all inherited
   ephemeris code and lazy catalogue initialisation are thread-safe.
3. Keep live page estimation structural; never calculate an annual almanac
   merely to update a checkbox summary.
4. Stream or batch exceptionally large full-direct-table editions if practical use shows the document model retaining too much memory.
5. Treat CUDA as a research option only after profiling. This workload is
   currently small, branch-heavy and dominated by cross-platform scalar
   routines; CPU caching and bounded parallelism offer much lower complexity
   and packaging risk.
