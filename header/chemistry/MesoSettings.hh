/// \file MesoSettings.hh
/// Computes mesh sizes and record times from mesoscopic-stage settings.
/// Dependencies: Standard library only.
/// Thread safety: Helpers use local state; write Current() only before concurrent readers start.
/// \brief Settings of the mesoscopic stage and the pure helpers derived from them.
///
/// Portable: standard library only, unit-agnostic doubles (the caller picks one
/// length unit and one time unit and keeps it). The process-wide Values object
/// is written by MesoMessenger at PreInit (master) and read by every worker's
/// TimeStepAction. Copy-paste portable like ChemistryRegistry (adjust the
/// include path prefix on copy).
#ifndef MesoSettings_h
#define MesoSettings_h 1

#include <vector>

namespace MesoSettings
{
/// Initial pixel cap for the scheduler's halving coarsening: ConvertIndex
/// multiplies index.x * newPixels in int before division. At 65536 -> 32768
/// the largest product fits signed 32-bit int; the next power of two does not
/// (G4DNAMesh.cc, G4DNAEventScheduler.cc; ADR 0006).
constexpr int kMaxPixels = 65536;

/// Among powers 2^0..2^30, chooses p minimizing |boxSide / p - targetVoxel|, ties going to
/// the larger p, then capped at maxPixels. Throws std::invalid_argument when
/// boxSide, targetVoxel or maxPixels is not > 0. Supply finite lengths in the
/// same unit. A custom non-power-of-two cap can produce a non-power-of-two result.
int PixelCount(double boxSide, double targetVoxel, int maxPixels = kMaxPixels);

/// True when maxPixels caps the chosen power of two; same units, preconditions
/// and invalid_argument behavior as PixelCount.
bool PixelCountCapped(double boxSide, double targetVoxel, int maxPixels = kMaxPixels);

/// start, logarithmic samples below end*(1-1e-9), then end, in one caller-chosen
/// time unit. The relative margin avoids near-duplicate final samples. Supply
/// finite times and a density whose sample count fits int and whose spacing
/// is representable. Throws invalid_argument for start >= end, start <= 0 or
/// perDecade < 1; allocation exceptions propagate.
std::vector<double> LogTimeGrid(double start, double end, int perDecade);

/// Process-wide settings, held in the caller's unit system. Defaults are in
/// Geant4 internal units (ns, mm), where ns = 1 and mm = 1.
struct Values
{
  double handOverTime = 5.;     ///< ns
  double voxelSize = 6.25e-6;   ///< mm (6.25 nm)
  int timesPerDecade = 10;
  /// /chem/meso/enable: whether the particle-based stage hands over to the
  /// mesoscopic stage. Read through StageEnabled(), never directly.
  bool enabled = true;
  /// True once /chem/meso/enable was issued (either value), so that the
  /// default can be told apart from an explicit "true".
  bool enabledExplicitly = false;
};

/// Process-lifetime settings reference; mutable only before concurrent readers start.
Values& Current();

/// Whether the mesoscopic stage runs: `values.enabled`, except under the SBS
/// time-step model (`sbsModel`), which has no hand-over; there the default
/// (true) silently means off. No validation, mutation or failure path.
bool StageEnabled(const Values& values, bool sbsModel);

/// True when the stage was enabled explicitly (/chem/meso/enable true) while
/// time-step model is SBS: a configuration error the caller reports. No exception here.
bool ExplicitlyEnabledWithSbs(const Values& values, bool sbsModel);
}  // namespace MesoSettings

#endif  // MesoSettings_h
