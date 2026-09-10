# Celestial Navigation 2.8.5.3

This pass addresses Bob's *Distance by Vertical Angle* testing in issue #282.
It does not change the enhanced lunar engine, stored sights, ephemeris, or
offline Earth-rotation update/fallback workflow introduced in 2.8.5.2.

## Coastal changes

- Retain the published Bowditch Table 15 calculation. Sea-horizon mode now
  accepts zero and negative dip-corrected angles. The top must still lie on
  or above the visible sea horizon after index correction; waterline mode
  still requires a positive angle. Neither mode nor the measured reading is
  changed automatically. Negative corrected angles retain an explicit minus
  sign even when OpenCPN's generic degrees/minutes formatter loses negative
  zero degrees.
- Show estimated observer horizon, target geographic visibility, and the
  waterline-to-top angle at the waterline horizon. These use the selected
  method's standard-refraction convention, not a light's luminous range or
  a guarantee of visibility. A zero corrected angle is not maximum range.
- Reject below-horizon and unsupported ambiguous lower-target observations.
  Both implemented methods require the top above the observer; waterline
  mode also requires positive eye height. Use focal height versus tower
  height as appropriate, with water level expressed in the same height datum.
- Reuse the existing searchable waypoint picker for the vertical target and
  all three HSA landmarks. Coordinates remain editable; heights are not
  copied. Duplicate names retain distinct identities through filtering.
  Cancel preserves existing entries; accepting clears the corresponding old
  plot and asks for recalculation.
- Label HSA uncertainty as formal angular-input uncertainty, not total fix
  accuracy. Input is arcminutes (60 = 1 degree). Landmark-coordinate errors,
  shared instrument errors and unmodelled motion are not included.
- Rewrap guidance/results on resizing or changing tabs and constrain the
  scrolling form to the available width. Both pages remain vertically
  scrollable, with Close/Clear/New controls outside the scrolling content.
- Update offline HTML, PDF, editable DOCX and contributor documentation.

Dedicated radar-bearing/range inputs remain a separate development pass.

## Reference results and validation

Bob's Needles case (top 24 m, eye 3 m, index error -0.15 arcmin):

| Raw angle | Dip-corrected angle | Range |
|---|---|---|
| 2.9 arcmin | +0.005055 arcmin | 9.676436 NM |
| 2.8 arcmin | -0.094945 arcmin | 9.797358 NM |

Observer horizon is about 3.66 NM and geographic visibility about 14.01 NM.
Additional tests reproduce Bob's 9 m and 216 m examples and his first
three-landmark HSA fix (43°49.9315′ N, 69°4.4233′ W). Changing HSA uncertainty
from 0.2 to 60 arcmin scales its formal uncertainty by 300 without changing
the computed position.

Bowditch's signed Table 15 entries (-4 through +3 arcmin, 70 ft height
difference) are tested within 0.1 NM of the printed one-decimal table. Its
rounded printed constants and table entries are not identical (for example,
zero-angle formula 9.7597 NM versus tabulated 9.7 NM); reference entries and
tolerances are explicit, not fitted to the implementation.

Local release checks:

- 162 non-GUI C++ tests and UTF-8 UI literal check.
- Separate Coastal GUI test: both tabs, 900×760 and 720×500 native GTK
  layouts, signed result display, waypoint accept/cancel/filter and retained
  inputs. Wayland allocations are explicitly synchronized for snapshots.
- Separate existing Lunar Tools GUI regression.
- Production engine accuracy gate: 379/379 candidate checks; frozen 2.8.5.1
  baseline unchanged, retaining its 15 expected accuracy failures.
- Offline manual validation: 10 diagrams, embedded DOCX images, linked HTML
  and identical bundled/output PDF.

These are local Linux checks, not a claim that every platform or navigation
condition has been validated. Cross-platform builds are checked by PR CI.

Sources: [Bob's PDF](https://github.com/user-attachments/files/32034280/Distance.by.Vertical.Angle.PDF.pdf),
[issue #282](https://github.com/rgleason/celestial_navigation_pi/issues/282#issuecomment-5619063099),
[NGA Bowditch 2019 Volume II, Table 15 and its explanation](https://msi.nga.mil/api/publications/download?key=16693975/SFH00000/Bowditch_Vol_2_LoRes_2019.pdf).
