# Explicit correction defaults fixture

Public inputs entered by the physical tablet keyboard: eye3.25m, temperature
12°C, pressure1008hPa, index error−1.75arcmin, short-dip distance2.5NM.
First artificial horizon=true/short dip=false; then short dip=true/artificial
horizon=false. Their mutual exclusion is baseline behavior, not a new correction.
This is an exact settings/UI persistence check, not an astronomical accuracy case.

Expected before execution: explicit Set As Defaults writes all seven exact keys
immediately; Cancel the observation creates no sight. A new sight after offline
cold restart retains all seven defaults. Ordinary unsaved edits+Cancel and
invalid pressure−1 followed by Set As Defaults must leave saved defaults and
the complete Sights.xml unchanged. Inspect landscape content movement to the
final action, keyboard rotation, invalid-message Back and header Cancel.
Retain complete original Sights bytes and restore only these task-created keys
after strict current-state checks; never replace the complete current config.

Runtime132 failed cold persistence despite hot New Sight retaining fields.
Runtime133/8418700 actual replay passes both seven-key modes, offline cold New
Sight, landscape final action/keyboard rotation, negative-pressure refusal and
Cancel with full Sights equality. Invalid attempt leaves the entire config
byte-identical. Targeted cleanup removes only the seven task-created keys,
all other current bytes retained; WiFi enabled and portrait restored.
`tablet-defaults138.json` contains only these public quantities and outcomes.
