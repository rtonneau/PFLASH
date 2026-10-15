/// \file RunManifest.cc
/// \brief Implementation of RunManifest

#include "scoring/RunManifest.hh"

#include "actions/Run.hh"
#include "chemistry/ChemistryRegistry.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"
#include "core/OutputDir.hh"
#include "geometry/DetectorConstruction.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "scoring/DataNode.hh"
#include "scoring/JsonWriter.hh"
#include "scoring/ResultsIndex.hh"
#include "scoring/RunAccumulator.hh"
#include "scoring/TrackLengthTable.hh"

#include "G4Exception.hh"
#include "G4MTRunManager.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Version.hh"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <vector>
#include <sstream>

namespace
{
  G4String gMacroName;
  std::chrono::steady_clock::time_point gProcessStart = std::chrono::steady_clock::now();
  std::optional<std::chrono::steady_clock::time_point> gPreviousDump;

  // Results index (<outdir>/Manifest.json): one entry per dump so far, and
  // the manifest of the empty-prefix flat dump if there was one (it shares
  // the index's path, so the index is written on top of it, see ResultsIndex).
  std::vector<DataNode> gDumpEntries;
  std::optional<DataNode> gRootDumpManifest;

  // Writes `tree` as JSON to `path`; a failed open is a JustWarning.
  void WriteTree(const G4String &path, const DataNode &tree, const char *code,
                 const std::string &consequence)
  {
    std::ofstream out(path);
    if (!out)
    {
      G4Exception("RunManifest::Write", code, JustWarning,
                  ("could not open '" + path + "' for writing; " + consequence).c_str());
      return;
    }
    JsonWriter::Write(out, tree);
  }

  std::string Timestamp()
  {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return stream.str();
  }

  // G4VERSION_TAG looks like "$Name: geant4-11-04-patch-01 $".
  std::string Geant4Version()
  {
    std::string tag = G4VERSION_TAG;
    const std::string head = "$Name:";
    if (tag.compare(0, head.size(), head) == 0)
      tag.erase(0, head.size());
    while (!tag.empty() && (tag.back() == '$' || tag.back() == ' '))
      tag.pop_back();
    const std::string::size_type first = tag.find_first_not_of(' ');
    return first == std::string::npos ? std::string() : tag.substr(first);
  }

  DataNode Vec3(const double *values)
  {
    return DataNode::MakeArray().Push(values[0]).Push(values[1]).Push(values[2]);
  }

  // Per-column mean and SEM of the accumulated track-length table. With no
  // rows the means are null (there is nothing to average).
  DataNode TrackLengthSummary(const TrackLengthTable &table)
  {
    const TrackLengthTable::Summary s = table.Summarize();
    const bool any = s.nEvents > 0;
    auto stat = [any](const TrackLengthTable::Stat &v) {
      DataNode node = DataNode::MakeObject();
      node.Add("mean", any ? DataNode(v.mean) : DataNode());
      node.Add("sem", any ? DataNode(v.sem) : DataNode());
      return node;
    };
    DataNode node = DataNode::MakeObject();
    node.Add("events", static_cast<long>(s.nEvents));
    node.Add("stopped", static_cast<long>(s.nStopped));
    node.Add("escaped", static_cast<long>(s.nEscaped));
    node.Add("killed", static_cast<long>(s.nKilled));
    node.Add("primaryLength_nm", stat(s.primaryLength_nm));
    node.Add("primaryEkin0_keV", stat(s.primaryEkin0_keV));
    node.Add("primaryEkinEnd_keV", stat(s.primaryEkinEnd_keV));
    node.Add("secondaryFirstGen_nm", stat(s.secondaryFirstGen_nm));
    node.Add("nSecondaryFirstGen", stat(s.nSecondaryFirstGen));
    node.Add("secondaryAll_nm", stat(s.secondaryAll_nm));
    node.Add("nSecondaryAll", stat(s.nSecondaryAll));
    return node;
  }
}

void RunManifest::SetMacroName(const G4String &macro)
{
  gMacroName = macro;
}

void RunManifest::MarkProcessStart()
{
  gProcessStart = std::chrono::steady_clock::now();
}

void RunManifest::RecordRun(const Run &run, double wallTime_s)
{
  DataNode entry = DataNode::MakeObject();
  entry.Add("run", run.GetRunID());
  entry.Add("events", static_cast<long>(run.GetNumberOfEvent()));
  if (run.HasBeam())
  {
    const Run::Beam &beam = run.GetBeam();
    entry.Add("particle", beam.particle);
    entry.Add("beamEnergy_keV", beam.energy_keV);
    entry.Add("position_um", Vec3(beam.position_um));
    entry.Add("direction", Vec3(beam.direction));
  }
  else
  {
    entry.Add("particle", DataNode());
    entry.Add("beamEnergy_keV", DataNode());
    entry.Add("position_um", DataNode());
    entry.Add("direction", DataNode());
  }
  entry.Add("energyDeposit_eV", run.GetSumDose() / eV);
  entry.Add("seed", run.GetSeed());
  entry.Add("wallTime_s", wallTime_s);
  RunAccumulator::AddRunEntry(entry);
}

void RunManifest::Write(const G4String &prefix, const G4String &subdir,
                        const std::vector<std::string> &files)
{
  const auto now = std::chrono::steady_clock::now();
  const double elapsedSinceStart = std::chrono::duration<double>(now - gProcessStart).count();
  const double elapsedSincePreviousDump =
      std::chrono::duration<double>(now - gPreviousDump.value_or(gProcessStart)).count();
  gPreviousDump = now;

  G4RunManager *runManager = G4RunManager::GetRunManager();
  auto *mtRunManager = dynamic_cast<G4MTRunManager *>(runManager);

  const ChemistryRegistry::Chemistry *chemistry = ChemistryRegistry::Selected();

  // Environment set on the chemistry world (pH, scavengers), if the detector
  // has one.
  const auto *detector =
      dynamic_cast<const DetectorConstruction *>(runManager->GetUserDetectorConstruction());
  const auto *chemistryWorld =
      (detector != nullptr) ? dynamic_cast<const DnaChemistryWorld *>(detector->GetChemistryWorld())
                            : nullptr;
  DataNode scavengers = DataNode::MakeArray();
  if (chemistryWorld != nullptr)
  {
    for (const ScavengerSpec::Entry &entry : chemistryWorld->GetScavengers())
      scavengers.Push(DataNode::MakeObject()
                          .Add("species", std::string(entry.species))
                          .Add("molarity_M", entry.molarity));
  }

  // Initial mesh pixel count per side (same formula as TimeStepAction; the
  // cell size actually used is voxelSize_nm unless the 65536 cap applied,
  // ADR 0006). Needs the chemistry world for the box size.
  const bool sbs = ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS;
  const bool mesoOn = MesoSettings::StageEnabled(MesoSettings::Current(), sbs);
  DataNode mesoPixels;
  if (mesoOn && chemistryWorld != nullptr)
    mesoPixels = DataNode(MesoSettings::PixelCount(2. * chemistryWorld->GetHalfBox(),
                                                   MesoSettings::Current().voxelSize * mm));

  const std::string dir = OutputDir::GetDirectory();
  std::error_code ec;
  const std::filesystem::path absolute =
      std::filesystem::absolute(std::filesystem::path(dir.empty() ? "." : dir), ec);

  DataNode fileList = DataNode::MakeArray();
  for (const std::string &file : files)
    fileList.Push(file);

  DataNode runs = DataNode::MakeArray();
  for (const DataNode &entry : RunAccumulator::GetRunEntries())
    runs.Push(entry);

  // The manifest's entries, in output order. Add or remove a line here to
  // change what Manifest.json states; JsonWriter needs no change.
  DataNode manifest = DataNode::MakeObject();
  manifest.Add("schemaVersion", 1);
  manifest.Add("timestamp", Timestamp());
  manifest.Add("elapsedSinceStart_s", elapsedSinceStart);
  manifest.Add("elapsedSincePreviousDump_s", elapsedSincePreviousDump);
  manifest.Add("geant4Version", Geant4Version());
  manifest.Add("macro", std::string(gMacroName));
  manifest.Add("chemistry", (chemistry != nullptr) ? std::string(chemistry->name) : std::string());
  manifest.Add("scavengers", scavengers);
  manifest.Add("pH", (chemistryWorld != nullptr) ? chemistryWorld->GetpH() : 7.);
  manifest.Add("halfBox_um",
               (chemistryWorld != nullptr) ? DataNode(chemistryWorld->GetHalfBox() / um) : DataNode());
  manifest.Add("chemistryEndTime_ns", G4Scheduler::Instance()->GetEndTime() / ns);
  manifest.Add("timeStepModel", std::string(ChemUtils::GetCurrentTimeStepModelName()));
  manifest.Add("mesoEnabled", mesoOn);
  manifest.Add("chemistryModel", mesoOn ? "IRT_syn+mesoscopic" : (sbs ? "SBS" : "IRT_syn"));
  manifest.Add("handOverTime_ns",
               mesoOn ? DataNode(MesoSettings::Current().handOverTime) : DataNode());
  manifest.Add("voxelSize_nm",
               mesoOn ? DataNode(MesoSettings::Current().voxelSize * mm / nm) : DataNode());
  manifest.Add("mesoPixels", mesoPixels);
  manifest.Add("mesoTimesPerDecade",
               mesoOn ? DataNode(MesoSettings::Current().timesPerDecade) : DataNode());
  manifest.Add("mesoSpatialOutput", mesoOn && MesoSettings::Current().spatialOutput);
  manifest.Add("runMode", (mtRunManager != nullptr) ? "MT" : "Serial");
  manifest.Add("threads", (mtRunManager != nullptr) ? mtRunManager->GetNumberOfThreads() : 1);
  manifest.Add("outputDirAsConfigured", dir);
  manifest.Add("outputDirAbsolute", ec ? dir : absolute.lexically_normal().string());
  manifest.Add("prefix", std::string(prefix));
  manifest.Add("subdir", std::string(subdir));
  manifest.Add("totalEvents", RunAccumulator::GetAccumulatedEvents());
  manifest.Add("totalEnergyDeposit_eV", RunAccumulator::GetAccumulatedEnergy() / eV);
  manifest.Add("trackLengths", TrackLengthSummary(RunAccumulator::GetAccumulatedTrackLengthTable()));
  manifest.Add("files", fileList);
  manifest.Add("runs", runs);

  // Results index: this dump joins the list. A flat dump with an empty prefix
  // has its per-dump manifest at the index's path (<outdir>/Manifest.json);
  // that file is then written once, as the per-dump manifest plus the
  // top-level "dumps" array, here and by every later dump. Any other dump
  // writes its own manifest (unchanged) and the index beside/above it.
  gDumpEntries.push_back(ResultsIndex::MakeDumpEntry(
      prefix, subdir, Timestamp(), RunAccumulator::GetAccumulatedEvents(),
      RunAccumulator::GetRunEntries()));
  if (prefix.empty() && subdir.empty())
    gRootDumpManifest = manifest;

  const std::string absoluteDir = ec ? dir : absolute.lexically_normal().string();
  const DataNode *rootBase = gRootDumpManifest ? &*gRootDumpManifest : nullptr;

  const G4String path = OutputDir::Resolve("Manifest.json");
  if (prefix.empty() && subdir.empty())
  {
    WriteTree(path, ResultsIndex::Build(gDumpEntries, rootBase, absoluteDir), "ManifestWriteFailed",
              "the dump's data files are written but it has no manifest");
    return;
  }

  WriteTree(path, manifest, "ManifestWriteFailed",
            "the dump's data files are written but it has no manifest");
  WriteTree(OutputDir::ResolveInRoot(ResultsIndex::FileName()),
            ResultsIndex::Build(gDumpEntries, rootBase, absoluteDir), "IndexWriteFailed",
            "the dump's data files and manifest are written but the results index is not updated");
}
