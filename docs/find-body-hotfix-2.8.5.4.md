# Find Body workflow hotfix — 2.8.5.4

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
Native normal/scaled layouts were visually inspected (525×348 and 714×400).

The first hosted Ubuntu job timed out at `apt-get update` before compilation;
an unchanged-code retry then lost an Android dependency download over HTTP.
CI dependency setup now uses five acquisition retries, 30-second HTTP/HTTPS
timeouts and official Ubuntu HTTPS archive endpoints. Ubuntu index progress is
no longer suppressed. Package signature/TLS verification is not weakened.
Pure shell regression checks cover these settings and URL rewrites without
touching host package configuration. This does not change the plugin binary.
