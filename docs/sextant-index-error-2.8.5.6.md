# Sextant Check index-error correction — 2.8.5.6

Version 2.8.5.6 makes a deliberately narrow correction to the Sextant Check
workflow. The existing topocentric, refracted apparent-angle prediction engine
is unchanged.

The check now records an independently measured index error with each raw
sextant reading. Using the plugin convention **on the arc +**, it calculates:

```
index-corrected apparent angle = raw observed angle - index error
residual profile correction = predicted apparent angle
                              - index-corrected apparent angle
```

New persistent profiles therefore contain only the remaining scale/centering
correction. This keeps session-dependent index error separate and prevents it
from being applied twice. The table shows the predicted apparent angle, raw
observation, index error, corrected observation and residual explicitly.

Profiles saved by older versions remain readable and retain their original
meaning. They are labelled as legacy total corrections to raw readings and
must not be combined with a separate index correction.

Regression coverage verifies positive and negative index errors, recovery of
the unchanged prediction after exactly one index and residual correction, and
profile independence from differing measured index errors. The complete test
suite and compact-layout GUI smoke tests are release gates.
