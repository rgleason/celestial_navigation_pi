# Celestial Navigation 2.8.13 — Android quick guide

Tap the stock Celestial Navigation icon in OpenCPN to open the workspace.
Four task buttons take you to **Observe**, **Fix**, **Plan** and **Tools**.
**Chart** hides the workspace; calculated overlays remain until you clear
them or change the corresponding observation. Tap the icon to return.

## Record and check an observation

In **Observe**, choose **New sight**. The section selector at the top of the
editor opens its measurement, correction, position and calculation pages.
Scroll each page to see all entries. Choose the body, sight type and limb
explicitly. Enter the actual sextant reading, eye height, signed index error,
environmental corrections and uncertainty; the manual explains each convention.

Use the calendar selector and separate time fields. Check the displayed time
zone; UTC and local calendar values are different representations of an instant.
Keep fractional seconds when needed. **Mark time** records an observation time;
**Tools > UTC, local and GNSS clock status** helps check the clock source.
Clock correction has its own explicit action and sign convention.

The **° / ′** buttons open precise angle or coordinate entry. Keep the hemisphere,
sign and units consistent. Check the assumed position, calculation and reduction
log before choosing **Save**. **Cancel** discards editor changes. Android Back
first dismisses an open keyboard or popup, then cancels the editor.

Select a saved observation card to edit, duplicate, include or exclude, delete,
or show that sight on the chart. Choose the sort order above the cards. Reopen
the observation to inspect its stored values.

## Compute, interpret and plot

In **Fix**, calculate a celestial fix or analyze a sight sequence. Review which
observations are included, the assumed position, residuals and uncertainty.
Use the available chart or waypoint action only after checking the result.
Motion and timing matter for running observations; a formal uncertainty does
not include every source of navigation error.

**Plan** contains Sun/Moon and best-sight planning, voyage almanacs, and eclipse
search with local circumstances. Position and date choices belong to each task.
Long jobs show progress and a cancellation action. Missing optional data and
coverage limits are reported by the relevant tool; consult the full manual.

In **Plan > Sun & Moon / best sights**, set position, UTC instant, display time
basis, eye height and motion on **Context**. Check the resolved UTC before using
the results. **Events** lists rise, set, twilight, transit and Moon details.
**Bodies & Best Sights** has Sort and Direction above a scrollable list; select
a card to enable **Create sight**. The separate **Recommendations & sky** page
has visibility limits, suggested combinations and the sky plot. **Almanac**
shows 25 hourly epochs for seven bodies and can export a CSV; its UTC column
keeps fractional seconds. Review the chosen folder and confirm before replacing
an existing file. **Noon & Polaris** takes corrected observed altitude, not the
raw sextant reading. Check whether the body is above the horizon and whether
the latitude branch is appropriate for your hemisphere.

## Specialized tools and documents

**Tools** opens lunar sessions, pairs and sextant checks, coastal sextant work,
clock correction, this guide and the full offline HTML/PDF manuals. Use the
section selector inside each tool for its different tasks.

Coastal calculations keep vertical and horizontal plot layers separately.
**Show plotted range/fix on chart** hides the editor and centres the calculated
geometry. **Clear chart plots** removes both layers while retaining entries.
**New / clear observation** resets both coastal pages after confirmation.

Scroll long reports with a swipe. The PDF viewer has explicit page navigation;
enter a page number and choose **Go**. Save generated files in a writable
OpenCPN folder and inspect the actual output. The full manual describes the
numerical methods, uncertainty, optional ephemerides and lunar UTC search span.
