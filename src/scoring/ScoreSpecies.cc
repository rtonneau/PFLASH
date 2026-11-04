/// \file ScoreSpecies.cc
/// \brief Accumulate Particle-based stage molecule counts and G-value sums.
/// Requires project settings/output and Geant4 scoring/chemistry services.
/// Worker-local scorers; merge and publish only at coordinated run boundaries.

/// \file ScoreSpecies.cc
/// \brief Implementation of the ScoreSpecies class

// This example is provided by the Geant4-DNA collaboration
// Any report or published results obtained using the Geant4-DNA software
// shall cite the following Geant4-DNA collaboration publication:
// Med. Phys. 37 (2010) 4692-4708
// J. Comput. Phys. 274 (2014) 841-882
// The Geant4-DNA web site is available at http://geant4-dna.org
//
// ScoreSpecies.cc
//
#include "scoring/ScoreSpecies.hh"

#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"
#include "core/OutputDir.hh"

#include "G4Event.hh"
#include "G4UnitsTable.hh"

#include <G4EventManager.hh>
#include <G4MolecularConfiguration.hh>
#include <G4MoleculeCounter.hh>
#include <G4Scheduler.hh>
#include <G4SystemOfUnits.hh>
#include <globals.hh>

#include <cmath>

namespace
{
  constexpr int kParticleRecordSteps = 60;
  constexpr double kLogBase = 10.0;
  constexpr double kRecordsPerDecade = 10.0;
  constexpr int kLastParticleRecord_ps = 999999;
  constexpr double kGValueReferenceEnergy_eV = 100.;
}

/**
 \file ScoreSpecies.cc
 \class ScoreSpecies
  This is a primitive scorer class for molecular species.
  The number of species is recorded for all times (predetermined or
  user chosen). It also scores the energy deposition in order to compute the
  radiochemical yields.
*/

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ScoreSpecies::ScoreSpecies(G4String name, G4int depth)
    : G4VPrimitiveScorer(name, depth),
      fEdep(0),
      fHCID(-1),
      fEvtMap(0)
{
  fNEvent = 0;
  // 10 log-spaced points per decade from 1 ps to 1 us (6 decades: exponents
  // 0.0, 0.1, ..., 5.9), then an explicit point just under 1 us (999999 ps,
  // not 1e6 ps) as a safety margin below the scheduler's default 1 us end
  // time -- same margin the previous hardcoded list used.
  for (int i = 0; i < kParticleRecordSteps; ++i) {
    AddTimeToRecord(std::pow(kLogBase, i / kRecordsPerDecade) * CLHEP::picosecond);
  }
  AddTimeToRecord(kLastParticleRecord_ps * CLHEP::picosecond);
  fEdep = 0;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ScoreSpecies::~ScoreSpecies()
{
  ;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool ScoreSpecies::ProcessHits(G4Step *aStep, G4TouchableHistory *)
{
  G4double edep = aStep->GetTotalEnergyDeposit();

  if (edep == 0.)
    return FALSE;

  edep *= aStep->GetPreStepPoint()->GetWeight(); // (Particle Weight)
  G4int index = GetIndex(aStep);
  fEvtMap->add(index, edep);
  fEdep += edep;

  return TRUE;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::Initialize(G4HCofThisEvent *HCE)
{
  fEvtMap = new G4THitsMap<G4double>(GetMultiFunctionalDetector()->GetName(), GetName());

  if (fHCID < 0)
  {
    fHCID = GetCollectionID(0);
  }

  HCE->AddHitsCollection(fHCID, (G4VHitsCollection *)fEvtMap);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::EndOfEvent(G4HCofThisEvent *)
{
  if (G4EventManager::GetEventManager()->GetConstCurrentEvent()->IsAborted())
  {
    fEdep = 0.;
    return;
  }

  // get the first, and in this case only, counter
  auto counter = G4MoleculeCounterManager::Instance()->GetMoleculeCounter<G4MoleculeCounter>(0);
  if (counter == nullptr)
  {
    G4Exception("ScoreSpecies::EndOfEvent", "BAD_REFERENCE", FatalException,
                "The molecule counter could not be received!");
  }

  auto indices = counter->GetMapIndices();

  if (indices.empty())
  {
    G4cout << "No molecule recorded, energy deposited= " << G4BestUnit(fEdep, "Energy") << G4endl;
    ++fNEvent;
    fEdep = 0.;
    return;
  }



  // TimeStepAction mutes the molecule counter from the hand-over to the end
  // of the event, so it holds frozen hand-over counts at later times: score
  // only the record times up to the hand-over (SpeciesMeso.* covers the
  // rest). No mesoscopic stage when it is switched off (SBS or /chem/meso/enable false)
  // or the hand-over is not before the end time.
  const G4double handOverTime = MesoSettings::Current().handOverTime * ns;
  const G4bool sbs = ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS;
  const G4bool hasMesoStage = MesoSettings::StageEnabled(MesoSettings::Current(), sbs) &&
                              handOverTime < G4Scheduler::Instance()->GetEndTime();

  for (auto moleculeIndex : indices)
  {
    for (auto recordTime : fTimeToRecord)
    {
      if (hasMesoStage && recordTime > handOverTime)
        break; // fTimeToRecord is sorted

      double moleculeCount = counter->GetNbMoleculesAtTime(moleculeIndex, recordTime);

      if (moleculeCount < 0)
      {
        G4cerr << "N molecules not valid < 0 " << G4endl;
        G4Exception("", "N<0", FatalException, "");
      }

      SpeciesInfo &molInfo = fSpeciesInfoPerTime[recordTime][moleculeIndex.Molecule];
      molInfo.fNumber += moleculeCount;
      molInfo.fNumberSq += static_cast<double>(moleculeCount) * moleculeCount;
      double gValue = (moleculeCount / (fEdep / eV)) * kGValueReferenceEnergy_eV;
      molInfo.fG += gValue;
      molInfo.fG2 += gValue * gValue;


    }
  }

  ++fNEvent;


  fEdep = 0.;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::AbsorbResultsFromWorkerScorer(G4VPrimitiveScorer *workerScorer)
{
  ScoreSpecies *right =
      dynamic_cast<ScoreSpecies *>(dynamic_cast<G4VPrimitiveScorer *>(workerScorer));

  if (right == 0)
  {
    return;
  }
  if (right == this)
  {
    return;
  }

  MergeSpeciesTotalsFrom(right);
  //---------------------------------------------------------

  // The worker scorer persists across /run/beamOn calls (unlike Run), so
  // empty it now that it has been absorbed -- otherwise the next run would
  // merge its cumulative map into the master a second time.
  right->fSpeciesInfoPerTime.clear();

  fNEvent += right->fNEvent;
  right->fNEvent = 0;
  right->fEdep = 0.;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::DrawAll()
{
  ;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::PrintAll()
{
  G4cout << " MultiFunctionalDet  " << detector->GetName() << G4endl;
  G4cout << " PrimitiveScorer " << GetName() << G4endl;
  G4cout << " Number of events " << fNEvent << G4endl;
  G4cout << " Number of energy deposition recorded " << fEvtMap->entries() << G4endl;

  for (auto itr : *fEvtMap->GetMap())
  {
    G4cout << "  copy no.: " << itr.first << "  energy deposit: " << *(itr.second) / GetUnitValue()
           << " [" << GetUnit() << "]" << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

OutputTable ScoreSpecies::ToTable() const
{
  // Molecular configurations with the same name (if any) share one row.
  std::map<double, std::map<std::string, SpeciesSchema::Row>> merged;
  for (const auto &timeEntry : fSpeciesInfoPerTime)
  {
    for (const auto &speciesEntry : timeEntry.second)
    {
      SpeciesSchema::Row &row = merged[timeEntry.first][speciesEntry.first->GetName()];
      row.time_ns = timeEntry.first / ns;
      row.species = speciesEntry.first->GetName();
      row.nEvents = fNEvent;
      row.countSum += speciesEntry.second.fNumber;
      row.countSumSq += speciesEntry.second.fNumberSq;
      row.gSum += speciesEntry.second.fG;
      row.gSumSq += speciesEntry.second.fG2;
    }
  }
  std::vector<SpeciesSchema::Row> rows;
  for (const auto &byTime : merged)
    for (const auto &bySpecies : byTime.second)
      rows.push_back(bySpecies.second);
  return SpeciesSchema::Build("species",
                              "Species: per-event molecule counts and G-values, Particle-based stage",
                              rows);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::OutputAndClear()
{
  if (G4Threading::IsWorkerThread())
    return;

  fNEvent = 0;
  fSpeciesInfoPerTime.clear();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ScoreSpecies::MergeSpeciesTotalsFrom(ScoreSpecies *right)
{
  {
    SpeciesMap::iterator timeEntry = right->fSpeciesInfoPerTime.begin();
    SpeciesMap::iterator timeEnd = right->fSpeciesInfoPerTime.end();

    for (; timeEntry != timeEnd; ++timeEntry)
    {
      InnerSpeciesMap &speciesByConfiguration = timeEntry->second;
      InnerSpeciesMap::iterator speciesEntry = speciesByConfiguration.begin();
      InnerSpeciesMap::iterator speciesEnd = speciesByConfiguration.end();

      for (; speciesEntry != speciesEnd; ++speciesEntry)
      {
        SpeciesInfo &molInfo = fSpeciesInfoPerTime[timeEntry->first][speciesEntry->first];
        molInfo.fNumber += speciesEntry->second.fNumber;
        molInfo.fNumberSq += speciesEntry->second.fNumberSq;
        molInfo.fG += speciesEntry->second.fG;
        molInfo.fG2 += speciesEntry->second.fG2;

      }
    }
  }
}
