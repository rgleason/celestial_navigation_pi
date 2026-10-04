# Celestial Navigation 2.9.3 — Android quick guide

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
The starting DR appears above the results. It defaults to the latest included
observation's saved DR. Choose another observation, enter a DR manually, or
explicitly select the current boat position. Boat position is never an implicit
replacement for missing saved DR.

Two observations can give two valid intersections far apart. Review both
positions and their distances from the starting DR, then select a candidate.
**Show selected fix on chart** remains disabled until you make that selection.
Changing the observations, DR, time correction or running-fix motion requires
selecting again. Tangent or unresolved geometry requires another sight.
Motion and timing matter for running observations; a formal uncertainty does
not include every source of navigation error.

**Plan** contains Sun/Moon and best-sight planning, voyage almanacs, and eclipse
search with local circumstances. Position and date choices belong to each task.
Long jobs show progress and a cancellation action. Missing optional data and
coverage limits are reported by the relevant tool; consult the full manual.

In **Plan > Sun & Moon / best sights**, set position, UTC instant, display time
basis, eye height and motion on **Context**. Check the resolved UTC before using
the results. **Events** lists rise, set, twilight, transit and Moon details.
Enter UTC explicitly for a local clock time skipped or repeated at a daylight-saving
change. Ship-zone offsets use hours east of UTC and can include half hours;
local mean time uses the planning longitude. The listed Moon-phase times are
approximate: the shared method found the June 2024 Full Moon about ten minutes
earlier than USNO. Use an authoritative almanac when an exact phase time matters.
**Bodies & Best Sights** has Sort and Direction above a scrollable list; select
a card to enable **Create sight**. The separate **Recommendations & sky** page
has visibility limits, suggested combinations and the sky plot. Choose **Sparse**, **Balanced** or **More** star labels on that page. Names
fit without overlap, brightest first; tap a plot dot to identify the body or
select its body card. The selected body has label priority. Use **-1 h / +1 h**
in Context to move the whole sky by one UTC hour; a moving observer advances
with the entered course and speed. The Moon path belongs to **SHA / Declination**;
the Local sky plot shows the current instant. Visibility guidance describes
brightness and twilight separately from altitude handling and fix geometry.
Suggested triads precede pairs; Polaris is optional for recommendations.
Lunar Candidates uses traditional companions by default; choose **All calculated
companions** to compare other bodies. Magnitude and cautions help assess a pair;
a planning preference is not a probability of seeing or measuring it.

**Almanac**
shows 25 hourly epochs for seven bodies and can export a CSV; its UTC column
keeps fractional seconds. Review the chosen folder and confirm before replacing
an existing file. **Noon & Polaris** takes corrected observed altitude, not the
raw sextant reading. Check whether the body is above the horizon and whether
the latitude branch is appropriate for your hemisphere.

LOLA-refined eclipse contact times are approximate. The retained 2027 comparison
found differences of up to 2.9 seconds from a full terrain scan. Displayed
hundredths of a second do not establish timing accuracy. Eclipse times use UT1;
future UTC also depends on Earth rotation.

## Offline ephemeris choice

For supported bodies and dates, an installed verified DE440s pack takes priority.
Otherwise the bundled Compact 0.2.0 engine supplies the Sun, Moon, planets and
navigation stars for 1972–2100. No large download is needed. In **Tools > Lunar
Tools > Advanced**, disable **Use compact analytical fallback (recommended)**
to use Classic Analytical instead. Classic also covers dates outside Compact's
range or unavailable Compact data. The corrected lunar solver applies to every
provider; inspect the calculation trail for the source and DUT1 availability.

## Specialized tools and documents

For a lunar observation, **Results** calculates the UTC candidates and opens
the result pages. **Check at entered UTC** checks the recorded instant; it does
not apply a recovered clock correction. On **Time**, choose whether the readings
are UTC or watch readings. If the distance and altitudes were measured at
different times, enable separate times and enter all three readings on the same
basis. Enable vessel advance only when the entered true COG and speed describe
the motion between readings. Review candidate branches, residuals and formal
uncertainty before storing a solution. Weak timing sensitivity can produce a
large clock uncertainty even when the position check is close to the DR.

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
