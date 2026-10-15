#include "scoring/RunAccumulatorMessenger.hh"
#include "scoring/RunAccumulator.hh"
#include "scoring/RunManifest.hh"
#include "core/OutputDir.hh"
#include "core/DnaLogger.hh"
#include "scoring/PreChemicalFiles.hh"
#include "scoring/MesoSpatialFile.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"
#include "scoring/ScoreSpecies.hh"

#include "G4UIcmdWithAString.hh"
#include "G4SDManager.hh"
#include "G4MultiFunctionalDetector.hh"
#include "G4AnalysisManager.hh"

#include <fstream>
#include <string>
#include <vector>

RunAccumulatorMessenger::RunAccumulatorMessenger()
    : G4UImessenger()
{
    fpDumpCmd = new G4UIcmdWithAString("/run/dumpDataAndReset", this);
    fpDumpCmd->SetGuidance(
        "Write Species/SpeciesMeso/Reactions/PhysicsInteractions/TrackLengths output files and a "
        "Manifest.json for everything accumulated since the last dump (or program "
        "start), then reset all counters. Optional prefix is prepended "
        "literally to every output filename (no separator inserted). Fatal "
        "if prefix was already used earlier in this run.");
    fpDumpCmd->SetParameterName("prefix", /*omittable=*/true);
    fpDumpCmd->SetDefaultValue("");
    fpDumpCmd->AvailableForStates(G4State_Idle);

    fpDumpToDirCmd = new G4UIcmdWithAString("/run/dumpDataAndResetToDir", this);
    fpDumpToDirCmd->SetGuidance(
        "Same as /run/dumpDataAndReset, but writes the output files "
        "(unprefixed) into a subfolder of the output directory (or of cwd "
        "if none is set) instead of using a filename prefix. The subfolder "
        "is created if missing; nested names (scan1/run01) are allowed. "
        "Absolute paths and '..' are rejected. Fatal if the name was "
        "already used earlier in this run.");
    fpDumpToDirCmd->SetParameterName("subdir", /*omittable=*/false);
    fpDumpToDirCmd->AvailableForStates(G4State_Idle);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

RunAccumulatorMessenger::~RunAccumulatorMessenger()
{
    delete fpDumpCmd;
    delete fpDumpToDirCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    if (command == fpDumpCmd)
    {
        G4String err;
        if (!DumpAndReset(newValue, /*enforceUniqueness=*/true, err))
        {
            G4Exception("RunAccumulatorMessenger::SetNewValue", "DuplicateDumpPrefix",
                        FatalException, err.c_str());
        }
    }
    else if (command == fpDumpToDirCmd)
    {
        G4String err;
        if (newValue.empty() || !OutputDir::ConfigureSubdir(newValue, err))
        {
            if (err.empty())
                err = "subfolder name must not be empty";
            G4Exception("RunAccumulatorMessenger::SetNewValue", "InvalidDumpSubdir",
                        FatalException, err.c_str());
        }
        else if (!RunAccumulator::TryReserveSubdir(newValue, /*enforceUniqueness=*/true, err))
        {
            OutputDir::ConfigureSubdir("", err);
            G4Exception("RunAccumulatorMessenger::SetNewValue", "DuplicateDumpSubdir",
                        FatalException, err.c_str());
        }
        else
        {
            WriteAllAndReset(/*prefix=*/"", newValue);
        }
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4String RunAccumulatorMessenger::GetCurrentValue(G4UIcommand * /*command*/)
{
    return "";
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::FlushIfPending(const G4String &autoPrefix)
{
    if (!RunAccumulator::HasPendingData())
        return;

    G4String err;
    DumpAndReset(autoPrefix, /*enforceUniqueness=*/false, err);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4bool RunAccumulatorMessenger::DumpAndReset(const G4String &prefix, G4bool enforceUniqueness,
                                              G4String &err)
{
    if (!RunAccumulator::TryReservePrefix(prefix, enforceUniqueness, err))
        return false;

    WriteAllAndReset(prefix, /*subdir=*/"");
    return true;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::WriteAllAndReset(const G4String &prefix, const G4String &subdir)
{
    // The subfolder (if any) was already validated, created and reserved by
    // the caller; OutputDir::Resolve() applies it together with the prefix.
    OutputDir::SetPrefix(prefix);

    // Data files this dump writes (relative to the manifest's folder, prefix
    // included), listed in its Manifest.json.
    std::vector<std::string> files;

    // Species: the ScoreSpecies scorer is itself the persistent,
    // SD-registered accumulator (unlike energy/reactions/interactions,
    // which RunAccumulator tracks separately) -- same lookup Run::Run()
    // uses.
    auto *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    if (mfdet != nullptr)
    {
        G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
        auto *scorer = dynamic_cast<ScoreSpecies *>(mfdet->GetPrimitive(collectionId));
        if (scorer != nullptr)
        {
            scorer->ASCII();          // Species.Txt (human-readable)
            scorer->OutputAndClear(); // Species_nt_species(_all).csv, then clears the scorer
            files.push_back(prefix + "Species.Txt");
            files.push_back(prefix + "Species_nt_species.csv");
#ifdef _ScoreSpecies_FOR_ALL_EVENTS
            // Only written when ScoreSpecies is built with per-event output.
            files.push_back(prefix + "Species_nt_species_all.csv");
#endif
        }
    }

    // Reactions
    const ReactionCounter &reactionCounter = RunAccumulator::GetAccumulatedReactionCounter();
    std::ofstream reactionsOut(OutputDir::Resolve("Reactions.Txt"));
    reactionCounter.WriteAscii(reactionsOut);
    reactionsOut.close();
    files.push_back(prefix + "Reactions.Txt");

    G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetDefaultFileType("csv");
    reactionCounter.WriteCsv(analysisManager);
    files.push_back(prefix + "Reactions_nt_reactions.csv");

    std::ofstream metadataOut(OutputDir::Resolve("ReactionsMetadata.csv"));
    reactionCounter.WriteMetadata(metadataOut);
    metadataOut.close();
    files.push_back(prefix + "ReactionsMetadata.csv");

    // G4AnalysisManager's per-ntuple file registry (keyed by resolved
    // filename) isn't cleared by CloseFile() alone -- a later dump cycle
    // reusing the same ntuple names ("species"/"reactions") under a new
    // prefix would otherwise report "already in use" and silently drop or
    // rename its CSV output. Clear() (not Reset(), which only resets the
    // ntuple/histogram bookings, not the file-name registry) empties that
    // registry so each dump cycle starts clean. Harmless here -- this
    // project never uses G4AnalysisManager's histogram features that
    // Clear() also resets.
    analysisManager->Clear();

    // Physics interactions
    const PhysicsInteractionCounter &interactionCounter =
        RunAccumulator::GetAccumulatedInteractionCounter();
    std::ofstream interactionsOut(OutputDir::Resolve("PhysicsInteractions.Txt"));
    interactionCounter.WriteAscii(interactionsOut);
    interactionsOut.close();
    files.push_back(prefix + "PhysicsInteractions.Txt");

    std::ofstream interactionsCsv(OutputDir::Resolve("PhysicsInteractions.csv"));
    interactionCounter.WriteCsv(interactionsCsv);
    interactionsCsv.close();
    files.push_back(prefix + "PhysicsInteractions.csv");

    // Per-event primary and secondary e- track lengths (physical stage).
    const TrackLengthTable &trackLengthTable = RunAccumulator::GetAccumulatedTrackLengthTable();
    std::ofstream trackLengthsOut(OutputDir::Resolve("TrackLengths.Txt"));
    trackLengthTable.WriteAscii(trackLengthsOut);
    trackLengthsOut.close();
    files.push_back(prefix + "TrackLengths.Txt");

    std::ofstream trackLengthsCsv(OutputDir::Resolve("TrackLengths.csv"));
    trackLengthTable.WriteCsv(trackLengthsCsv);
    trackLengthsCsv.close();
    files.push_back(prefix + "TrackLengths.csv");

    // Mesoscopic stage off (/chem/meso/enable false, or SBS): no SpeciesMeso files.
    if (MesoSettings::StageEnabled(MesoSettings::Current(),
                                   ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS))
    {
      // Species counts of the mesoscopic stage (hand-over to end time).
      const MesoSpeciesCounter &mesoSpeciesCounter = RunAccumulator::GetAccumulatedMesoSpeciesCounter();
      std::ofstream mesoOut(OutputDir::Resolve("SpeciesMeso.Txt"));
      mesoSpeciesCounter.WriteAscii(mesoOut, RunAccumulator::GetAccumulatedEvents());
      mesoOut.close();
      files.push_back(prefix + "SpeciesMeso.Txt");

      std::ofstream mesoCsv(OutputDir::Resolve("SpeciesMeso.csv"));
      mesoSpeciesCounter.WriteCsv(mesoCsv);
      mesoCsv.close();
      files.push_back(prefix + "SpeciesMeso.csv");
    }

    // Pre-chemical files: move the per-event files staged since the last dump
    // into this dump (target = prefix/subdir applied by OutputDir::Resolve).
    const PreChemicalFiles::MoveResult moveResult = PreChemicalFiles::MoveStaged(
        PreChemicalFiles::StagingDir(OutputDir::GetDirectory()),
        [](const std::string &name) { return OutputDir::Resolve(name); });
    files.insert(files.end(), moveResult.moved.begin(), moveResult.moved.end());
    for (const std::string &failure : moveResult.failures)
    {
        G4Exception("RunAccumulatorMessenger::WriteAllAndReset", "PreChemicalMoveFailed",
                    JustWarning, failure.c_str());
    }
    DnaLogger::Print(DnaLogger::Level::Info,
                     "[RunAccumulatorMessenger] moved " + std::to_string(moveResult.moved.size()) +
                         " pre-chemical file(s) into the dump");

    // Meso spatial snapshots (/chem/meso/spatialOutput): move the staged HDF5
    // file into this dump (Resolve applies the prefix and subdir).
    {
        bool spatialMoved = false;
        std::string spatialErr;
        if (MesoSpatialFile::MoveStaged(OutputDir::GetDirectory(),
                                        OutputDir::Resolve(MesoSpatialFile::FileName()),
                                        spatialMoved, spatialErr))
        {
            if (spatialMoved)
            {
                files.push_back(prefix + MesoSpatialFile::FileName());
            }
        }
        else
        {
            G4Exception("RunAccumulatorMessenger::WriteAllAndReset", "MesoSpatialMoveFailed",
                        JustWarning, spatialErr.c_str());
        }
    }

    // Energy deposit, beam and everything else that describes this dump.
    RunManifest::Write(prefix, subdir, files);

    RunAccumulator::ClearAccumulated();
    OutputDir::SetPrefix("");
    G4String err;
    OutputDir::ConfigureSubdir("", err);

    // Plain G4cout (not DnaLogger): this is the run.successMarkers line the
    // project's own smoke-test verification checks for -- see
    // .claude/.claude-project.json and .claude/geant4-instructions.md.
    G4cout << "[RunAccumulatorMessenger] dumped and reset (prefix='" << prefix << "', subdir='"
           << subdir << "')" << G4endl;
}
