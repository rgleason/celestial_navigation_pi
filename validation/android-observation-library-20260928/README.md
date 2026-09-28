# Disposable observation library

Sorting and Delete All must use this temporary three-record library only.
Before replacing the loaded library, force-stop and retain the entire current
Sights.xml after verifying its exact known checksum; retain current config too.
The original observations and complete solution reports remain in the verified
backup and are restored byte for byte after execution. No original host objects
or user observations may be deleted through the UI.

Public Sun/Venus/Mars fixtures vary UTC/body/type/angle/visibility/colour. This
checks ordering and record identity, not astronomical accuracy. The expected
orders are declared before execution in `expected-orders140.json`. Existing
secondary comparison orders ties by inclusion/type/body/time/measurement/colour;
descending reverses these comparisons too. Colours have the same nonopaque
alpha and unique CSS red text111/205/39, giving lexicographic111→205→39.
Colour sorting is by the baseline colour representation, not colour-name order.

After normal load/save, retain a complete normalized fixture snapshot; each
actual sort must only permute those records, including selected-record identity.
Verify all12 choices and actual serialized order. Inspect portrait/landscape
popup rows48dp, swipe/Back without activation. Exercise actual Delete All No,
confirmation Back, Yes, empty actions and cold empty persistence, then restore
the protected original library bytes with guarded readback before further work.

Physical140 on runtime135/a3c42bf passed all twelve touch selections, complete
record preservation and selected Mars retention. The public result JSON records
exact expected/actual orders and saved fixture hashes. Portrait/landscape popup
rows are72px/48dp at font1.3. Swipe then Back and popup rotation-dismissal preserve
the whole file. Actual Delete All No/Back preserve bytes; Yes deletes3fixtures,
empty actions/guidance/NewCancel and offline cold empty persistence pass.
Guarded restoration returns all original library bytes and retains current config.
No original/private observation or report contents are published here.
