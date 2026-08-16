# Voyage Almanac: Gate 6 implementation note

## Scope and safety level

Gate 6 produces an independent, completely offline **OpenCPN Voyage
Almanac**. The default Voyage Almanac is calculator-complete: it includes the
ephemeris, star data, correction formulae, procedures and forms needed to
reduce supported sights with a scientific calculator.

It is not yet a pencil-only replacement for the complete Nautical Almanac plus
the several volumes of Pub. 229/249-style sight-reduction tables. The cover and
GUI state this explicitly. A later paper-only safety level must enforce the
presence of every tabular dependency before it can use that description.

Gate 6 includes:

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
- exact structural page/sheet estimates, first-page preview and atomic PDF
  output.

The universal ephemeris is not filtered by a route. Route coverage affects
only location-dependent planning pages.

## Planned output spectrum after Gate 6

The eventual generator should retain a deliberately wide scope:

- **Passage Brief:** a small location-aware planning document;
- **Voyage Almanac:** the recommended route/date/corridor-specific paper
  backup, with compact tables and only useful bodies in planning lists;
- **Calculator-free Voyage Almanac:** adds every interpolation, correction
  and sight-reduction table required for pencil-only work; and
- **Full global annual almanac:** the largest, route-independent option for a
  user who explicitly wants it.

Compaction must never create a hidden dependency. Route and observability
filters may shorten sight suggestions, charts and duplicated planning
material, while universal data for every body included in the document stays
available if the vessel leaves the expected corridor. The paper-only safety
level will require a machine-checkable dependency manifest before it is
described as self-contained.

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
| 14 days | 64 | 1.059 s | 0.005 s | 470,117 bytes |
| 93 days (Gate-6 maximum) | 301 | 6.912 s | 0.028 s | 2,622,881 bytes |

Peak resident memory for the 93-day test process was about 65,884 KiB. This
includes the complete test executable and test framework, so it is a useful
upper-context measurement rather than a clean incremental allocation figure.

The near-linear result predicts that ordinary annual ephemeris/planning output
would take tens of seconds, not hours, before full tabular sight-reduction
material is added.

## Acceleration plan for later gates

Profile before changing the numerical engine. The current PDF writer accounts
for less than one percent of elapsed time; astronomical document assembly is
the useful target.

1. Cache shared body/epoch states so route-dependent altitude/azimuth work can
   reuse geocentric GHA/declination.
2. Generate days as independent jobs in a bounded CPU worker pool, then merge
   their semantic pages in date order. First confirm that all inherited
   ephemeris code and lazy catalogue initialisation are thread-safe.
3. Keep live page estimation structural; never calculate an annual almanac
   merely to update a checkbox summary.
4. Stream or batch annual/full-table pages so the document model does not need
   to retain hundreds of megabytes.
5. Treat CUDA as a research option only after profiling. This workload is
   currently small, branch-heavy and dominated by cross-platform scalar
   routines; CPU caching and bounded parallelism offer much lower complexity
   and packaging risk.
