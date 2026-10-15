#!/bin/bash
SIM="/c/DEV/GEANT4/SIM/dnachem-min/build/sim.exe"
BASE="/d/DATA/Geant4"
cd "/c/DEV/GEANT4/SIM/dnachem-min/build" || exit 1

run() {
  local macro="$1" outdir="$2"
  mkdir -p "$outdir"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') START $macro -> $outdir (MT 8) ==="
  "$SIM" "$macro" --threads 8 --dir "$outdir" > "$outdir/run.log" 2>&1
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') DONE $macro (exit $rc) ==="
}

for o2 in 0 3 7 21; do
  run "beam_farokhi_proton_o2_${o2}pct_20um.in" "$BASE/261010_Farokhi2023_proton_20um_tracklen/o2_${o2}pct"
done

for o2 in 0 3 7 21; do
  run "beam_farokhi_proton_o2_${o2}pct_40um.in" "$BASE/261010_Farokhi2023_proton_40um_tracklen/o2_${o2}pct"
done

for o2 in 0pct 2p5pct 5pct 21pct; do
  for el in 1pct 2p5pct 5pct 7p5pct 10pct; do
    run "beam_farokhi_electron_o2_${o2}_elossmin_${el}.in" "$BASE/261010_Farokhi2023_electron_elossmin_tracklen/o2_${o2}/elossmin_${el}"
  done
done

echo "ALL DONE $(date '+%Y-%m-%d %H:%M:%S')"
