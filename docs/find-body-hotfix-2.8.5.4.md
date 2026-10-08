# Find Body workflow hotfix — 2.8.5.4

## Fiji lunar candidate-selection follow-up

Bob's Fiji Saturn–Moon cases 7a and 7b reproduced a northern-hemisphere default
solution even though the solver also found a southern solution near his DR.
The old default chose the smallest absolute UTC correction before considering
position. Both branches fit the measured angles; the approximate position is
needed to distinguish them.

The calculation trail and Results page now share a selection policy which ranks
all UTC/position branches by proximity to valid DR coordinates. Explicit user
selection overrides that default and is retained when the Results mode changes.
All branches remain visible, with a warning to check DR and ambiguity. This does
not force a solution onto the DR position or change the underlying ephemerides.
Invalid/missing coordinates fall back to the nearest entered UTC. Legacy sights
have no position-availability flag, so the default (0,0) is conservatively treated
as unavailable; an observer actually there can select a branch explicitly.

Permanent headless tests exercise the complete Sight calculation with Bob's
screenshot inputs and the worksheet's slightly different pressure/eye height:

- 7a, no added lunar-distance error: default now approximately 0.73 NM from the
  stated position, rather than approximately 2,734 NM away.
- 7b, with deliberately added 1.5 arcminutes: default now approximately 1.43 NM
  from Bob's old production reference, rather than the northern branch. Its
  displacement from the original DR is expected because the input contains error.
- Missing/invalid DR, date-line proximity, multiple positions per UTC root and
  explicit overrides are also covered. Raw observations and UTC remain unchanged.

These are regression comparisons and forward-model consistency checks, not new
independent accuracy claims. This Saturn–Moon case uses the bundled analytical
ephemeris path, not the separately audited DE440 Sun–Moon path.

## Find workflow

Addresses Bob's report in issue #289, comment 5635145707. Version 2.8.5.3
combined accepting DR coordinates and copying estimated Hs, preventing a
position-only edit. This is a behavioural fix, not only button renaming.

- Close retains the edited DR position and live/magnetic settings in the open
  Sight Properties transaction. It never copies Hs. Close, not Copy, is the
  default action.
- Copy estimated Hs updates the appropriate ordinary/Moon/body altitude field
  immediately and leaves Find open. Repeated copies are deliberate actions.
- Reset position restores the coordinates and live-position setting shown on
  opening Find. It remains open and does not undo a deliberate Hs copy.
- Cancel discards position edits and closes Find without undoing explicitly
  copied Hs. Window X and Escape follow Cancel. This incorporates Bob's
  clarified workflow in comment 5635672658, while retaining Reset position.
- Reset sits alongside position controls; Copy sits beside estimated Hs;
  Cancel and Close are at the bottom right.
- Read-only Altitude (Ho) sits below Hc, using the same host angle formatting.
  Lunar helpers show their Moon/body reduced altitude, not lunar distance.
  Intercept is below Zn; Towards/Away remain standard disabled indicators.
- Hide Time / Show Time clarify the Time integrity panel visibility toggle.
- Closing is independent of whether an estimated altitude is available.
- Bearing sights retain position editing but disable Copy estimated Hs, since
  their measured bearing is not an altitude field.
- Save Changes in the parent keeps the edit; cancelling the parent still
  restores the original sight, including explicitly copied measurements.
- Position-only changes mark the parent dirty and recompute its results without
  replacing the observation. Astronomical and reduction engines, stored time
  conventions, XML and offline data are unchanged.

GUI regression coverage exercises the actual main-window edit transaction,
Sight Properties and nested Find popup through ordinary, lunar Moon and lunar
body routes. It checks position-only close/reopen, window X, Escape, reset,
repeated explicit copies, unavailable estimates, and cancelling the parent.
The follow-up adds Cancel/X/Escape after copying Hs, cancelling an accidentally
blank coordinate and live-position selection, Ho display/refresh, time-toggle
labels and unchanged XML throughout the popup transaction.
Additional cases cover bearing sights, coordinate Enter, and restoring the
initial manual-position mode after selecting live position.
Existing numerical, timezone, lunar/coastal GUI and independent-reference
accuracy tests remain release gates. Native layout is visually inspected.

Local release validation: 175 non-GUI tests, four timezone regression runs,
all three opt-in GUI suites, 39 Find workflow scenarios (normal and 150% font
scaling), and 379/379 production
independent-reference checks passed. The frozen baseline retains its 15 expected
accuracy failures. Offline HTML, PDF and embedded-image DOCX validation passed.
Native normal/scaled layouts were visually inspected (527×347 and 714×400).

The first hosted Ubuntu job timed out at `apt-get update` before compilation;
an unchanged-code retry then lost an Android dependency download over HTTP.
CI dependency setup now uses five acquisition retries, 30-second HTTP/HTTPS
timeouts and official Ubuntu HTTPS archive endpoints. Ubuntu index progress is
no longer suppressed. Package signature/TLS verification is not weakened.
Pure shell regression checks cover these settings and URL rewrites without
touching host package configuration. This does not change the plugin binary.
