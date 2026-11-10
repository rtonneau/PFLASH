// CoincidenceCheck: finds molecules that sit at exactly the same position.
// Geant4-free.
//
// Why. The molecule finder of the stock kernel cannot handle two molecules at one
// point, and a reaction between them would have no distance to act on. Two
// molecules at one point arise legitimately in one case only: the products of one
// start-up reaction (the stock capture, CaptureRecorder.hh) are created at one
// point and carry the same nonzero productGroup. Everything else is refused:
//   - two precursors (productGroup 0) at one point;
//   - a precursor and a product at one point;
//   - products of different start-up reactions at one point;
//   - molecules of different packets (sources) at one point, e.g. a packet injected
//     twice at one offset, or two offsets that bring molecules together.
// Nothing is nudged or merged: the error says what was found.
//
// Equality is exact (operator== on each coordinate), found by sorting, so a
// distance of one ulp is not a coincidence.
#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ContactCount.hh"

namespace ptirt
{

inline constexpr const char* kCoincidenceErrorToken = "PTIRT_COINCIDENT_MOLECULES";

struct CoincidenceMolecule
{
  std::array<double, 3> position{};  // any length unit; compared exactly
  std::size_t packet = 0;            // which packet (source) the molecule belongs to
  int productGroup = 0;              // PacketMolecule::productGroup; 0 = precursor
};

struct CoincidenceReport
{
  // Every molecule that shares its position with another, and the number of
  // distinct shared positions.
  std::size_t coincidentMolecules = 0;
  std::size_t positions = 0;
  // Positions where every molecule is from one packet and has one nonzero
  // productGroup (allowed) ...
  std::size_t exemptMolecules = 0;
  std::size_t exemptPositions = 0;
  // ... and all the others (refused).
  std::size_t forbiddenMolecules = 0;
  std::size_t forbiddenPositions = 0;
  // For each exempt position (in position order), the indices into the input
  // vector of the molecules there, ascending.
  std::vector<std::vector<std::size_t>> exemptMembers;
};

// Throws std::invalid_argument if a coordinate is not finite.
CoincidenceReport FindCoincidentPositions(const std::vector<CoincidenceMolecule>& molecules);

// Positions only: no molecule has a product group, so every coincidence is refused.
CoincidenceReport FindCoincidentPositions(const std::vector<std::array<double, 3>>& positions);

// "PTIRT_COINCIDENT_MOLECULES count=<forbidden molecules> positions=<forbidden positions>"
std::string DescribeForbiddenCoincidences(const CoincidenceReport& report);

// What the allowed coincidences hold, for capture.json and the ledger.
struct CoincidenceSummary
{
  std::size_t exemptPositions = 0;
  std::size_t exemptMolecules = 0;
  // Pairs at an exempt position whose species have a positive reaction radius
  // (they are in contact at distance 0), in total and by PairLabel.
  std::size_t reactivePairs = 0;
  std::map<std::string, std::size_t> perPair;

  void Add(const CoincidenceSummary& other);
};

// species[i] is the species of input molecule i (same indexing as the vector that
// gave `report`); radius as for CountInternalContacts.
CoincidenceSummary SummarizeExempt(const CoincidenceReport& report, const std::vector<std::string>& species,
                                   const ContactRadiusFn& radius);

}  // namespace ptirt
