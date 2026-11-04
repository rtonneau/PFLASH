/// \file RunAccumulatorMessenger.cc
/// \brief Publish accumulated outputs and reset each completed dump.
/// Requires project output components and Geant4 UI/analysis services.
/// Invoke between runs on the master or serial thread, with workers idle.

#include "scoring/RunAccumulatorMessenger.hh"
#include "scoring/RunAccumulator.hh"
#include "scoring/RunManifest.hh"
#include "core/OutputDir.hh"
#include "core/DnaLogger.hh"
#include "scoring/Hdf5TableFile.hh"
#include "scoring/PreChemicalFiles.hh"
#include "scoring/PreChemicalRecorder.hh"
#include "scoring/RootTableFile.hh"
#include "scoring/ProductOutput.hh"
#include "scoring/OutputFormats.hh"
#include "scoring/MesoSpatialFile.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"
#include "scoring/ScoreSpecies.hh"

#include "G4UIcmdWithAString.hh"
#include "G4SDManager.hh"
#include "G4MultiFunctionalDetector.hh"

#include <fstream>
#include <string>
#include <vector>

namespace
{
void WriteSpeciesOutput(const G4String &prefix, std::vector<std::string> &files)
{
    auto *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    if (mfdet != nullptr)
    {
        G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
        auto *scorer = dynamic_cast<ScoreSpecies *>(mfdet->GetPrimitive(collectionId));
        if (scorer != nullptr)
        {
            ProductOutput::Write(OutputFormats::Product::Species, {scorer->ToTable()}, prefix, files);
            scorer->OutputAndClear(); // clears the scorer for the next dump cycle
        }
    }
}

void WriteMesoscopicOutput(const G4String &prefix, std::vector<std::string> &files)
{
    // Species counts of the mesoscopic stage (hand-over to end time).
    ProductOutput::Write(OutputFormats::Product::SpeciesMeso,
                         {RunAccumulator::GetAccumulatedMesoSpeciesCounter().ToTable()}, prefix, files);
}

// h5/root PreChemical tables: the rows accumulated by the per-event recorders
// are written whole into the staging folder (neither writer can append), so
// MoveStaged publishes them with the per-event txt/csv files. Nothing is
// staged when neither format is chosen.
void StagePreChemicalTables()
{
    const std::vector<OutputFormats::Format> formats =
        OutputFormats::Effective(OutputFormats::Product::PreChemical);
    const OutputTable table = PreChemicalRecorder::TakeStaged();
    for (OutputFormats::Format format : formats)
    {
        if (format != OutputFormats::Format::H5 && format != OutputFormats::Format::Root)
            continue;
        std::string err;
        if (!PreChemicalFiles::EnsureStagingDir(OutputDir::GetDirectory(), err))
        {
            G4Exception("RunAccumulatorMessenger::WriteAllAndReset", "PreChemicalStagingDir",
                        JustWarning, err.c_str());
            return;
        }
        const std::string path =
            PreChemicalFiles::StagingDir(OutputDir::GetDirectory()) + "/" +
            PreChemicalFiles::StagedTableFileName(OutputFormats::Extension(format));
        const bool ok = (format == OutputFormats::Format::H5)
                            ? Hdf5TableFile::Write(path, {table}, err)
                            : RootTableFile::Write(path, {table}, err);
        if (!ok)
        {
            const std::string message = "Cannot write " + path + ": " + err;
            G4Exception("RunAccumulatorMessenger::WriteAllAndReset", "PreChemicalTableFailed",
                        JustWarning, message.c_str());
        }
    }
}

void MoveSpatialOutput(const G4String &prefix, std::vector<std::string> &files)
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
}  // namespace

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
    WriteSpeciesOutput(prefix, files);

    // Reactions (+ catalogue of reaction ids)
    ProductOutput::Write(OutputFormats::Product::Reactions,
                         RunAccumulator::GetAccumulatedReactionCounter().ToTables(), prefix, files);

    // Physics interactions
    ProductOutput::Write(OutputFormats::Product::PhysicsInteractions,
                         {RunAccumulator::GetAccumulatedInteractionCounter().ToTable()}, prefix, files);

    // Per-event primary and secondary e- track lengths (physical stage).
    const TrackLengthTable &trackLengthTable = RunAccumulator::GetAccumulatedTrackLengthTable();
    ProductOutput::Write(OutputFormats::Product::TrackLengths, {trackLengthTable.ToTable()}, prefix, files);

    // Mesoscopic stage off (/chem/meso/enable false, or SBS): no SpeciesMeso files.
    if (MesoSettings::StageEnabled(MesoSettings::Current(),
                                   ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS))
    {
      WriteMesoscopicOutput(prefix, files);
    }

    // Pre-chemical files: stage the h5/root tables, then move the files staged
    // since the last dump (per-event txt/csv and the tables)
    // into this dump (target = prefix/subdir applied by OutputDir::Resolve).
    StagePreChemicalTables();
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
    MoveSpatialOutput(prefix, files);

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
