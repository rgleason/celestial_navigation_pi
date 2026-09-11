# Find Body workflow hotfix — 2.8.5.4

Addresses Bob's report in issue #289, comment 5635145707. Version 2.8.5.3
combined accepting DR coordinates and copying estimated Hs, preventing a
position-only edit. This is a behavioural fix, not only button renaming.

- Close retains the edited DR position and live/magnetic settings in the open
  Sight Properties transaction. It never copies Hs. Window X and Escape follow
  the same path. Close, not Copy, is the default action.
- Copy estimated Hs updates the appropriate ordinary/Moon/body altitude field
  immediately and leaves Find open. Repeated copies are deliberate actions.
- Reset position restores the coordinates and live-position setting shown on
  opening Find. It remains open and does not undo a deliberate Hs copy.
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
Additional cases cover bearing sights, coordinate Enter, and restoring the
initial manual-position mode after selecting live position.
Existing numerical, timezone, lunar/coastal GUI and independent-reference
accuracy tests remain release gates. Native layout is visually inspected.

Local release validation: 175 non-GUI tests, four timezone regression runs,
all three opt-in GUI suites, 26 Find workflow scenarios, and 379/379 production
independent-reference checks passed. The frozen baseline retains its 15 expected
accuracy failures. Offline HTML, PDF and embedded-image DOCX validation passed.
