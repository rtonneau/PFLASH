/// \file RunAccumulatorMessenger.hh
/// \brief Publish accumulated outputs and reset each completed dump.
/// Requires project output components and Geant4 UI/analysis services.
/// Invoke between runs on the master or serial thread, with workers idle.

#ifndef RUN_ACCUMULATOR_MESSENGER_HH
#define RUN_ACCUMULATOR_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file RunAccumulatorMessenger.hh
    UI command to flush the accumulated species/reaction/interaction data to
    disk (plus a Manifest.json describing the dump, see RunManifest) and reset
    the underlying counters, from a macro file:
    /run/dumpDataAndReset [prefix]

    Idle-state only (issue it between /run/beamOn calls). prefix (optional,
    default "") is prepended literally (no separator inserted) to every
    output filename written by this flush. Reusing a prefix already used
    earlier in this process is a fatal error -- see
    RunAccumulator::TryReservePrefix.

    /run/dumpDataAndResetToDir <subdir> is the same flush, but writes the
    (unprefixed) files into <subdir> under the output directory instead --
    created if missing, relative and without "..", unique per process (see
    OutputDir::ConfigureSubdir, RunAccumulator::TryReserveSubdir).

    Also exposes FlushIfPending(), called once from sim.cc right before the
    run manager is destroyed, as a safety net so data accumulated but never
    manually flushed isn't silently lost.
*/
/// Owns dump UI commands; requires live scoring/analysis services when flushing.
class RunAccumulatorMessenger : public G4UImessenger
{
public:
    /// Register the two Idle-state dump commands; owns their allocated objects.
    RunAccumulatorMessenger();
    /// Delete the owned UI commands; does not flush accumulated data.
    virtual ~RunAccumulatorMessenger();

    /// Dispatch prefix/subdir text; invalid or repeated destinations raise
    /// FatalException. Unknown commands are ignored; output failures use the
    /// existing writer/JustWarning paths and are not transactional.
    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    /// Return an empty value for any command; dump commands have no current value.
    virtual G4String GetCurrentValue(G4UIcommand *command);

    /// If any data is pending, flushes it under autoPrefix (bypassing the
    /// prefix-uniqueness check). No-op otherwise. Call before run-manager
    /// destruction with workers idle; autoPrefix is used literally. Existing
    /// output errors remain possible and do not prevent every reset operation.
    void FlushIfPending(const G4String &autoPrefix);

private:
    /// Reserves prefix (see enforceUniqueness), then WriteAllAndReset(prefix, "").
    G4bool DumpAndReset(const G4String &prefix, G4bool enforceUniqueness, G4String &err);

    /// Writes every output file under the given prefix and (already
    /// configured and reserved) subfolder, then resets all counters and
    /// clears both from OutputDir. subdir also describes the manifest/index entry.
    void WriteAllAndReset(const G4String &prefix, const G4String &subdir);

    G4UIcmdWithAString *fpDumpCmd;
    G4UIcmdWithAString *fpDumpToDirCmd;
};

#endif // RUN_ACCUMULATOR_MESSENGER_HH
