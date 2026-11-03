# Ticket 01: energy-cuts-command

**Status:** ✅ Done

## Local Test Result

`cmake --build build --config RelWithDebInfo --target sim`: OK.
Smoke (10 keV e-, beamOn 2, meso off, endTime 1 ns, energyCuts false, verbose 1): 0 `kill track at`; TrackLengths.csv: both `stopped`, end Ekin 25 meV.
Comparison at 50 keV, eLossMin 10 keV (replaces the 1 MeV run, which the user stopped as too big): cuts on -> 2 kill lines, primaries 14.3/11.7 um `killed` (~10 keV lost); cuts off -> 0 kill lines, 33.6/52.7 um `stopped`.

## Review Notes

All criteria met. Box cut untouched; early return extended with `!fEnergyCuts`. minKineticE now handled and deleted. Plan item 4 confirmed empirically: natural stops end at 25 meV (> 0), reported `stopped`.

## Blockers / Challenges

The 1 MeV cuts-on/off comparison was stopped by the user (too slow); replaced with a 50 keV / eLossMin 10 keV comparison.

## Commits

- 16c2d8f feat: add /primaryKiller/energyCuts switch and minKineticE handler (ticket 01)

## Time Spent

9m (ticket-start.js to ticket-complete.js)
