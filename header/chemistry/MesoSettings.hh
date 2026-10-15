/// \file MesoSettings.hh
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
/// Largest safe initial pixel count per side: G4DNAMesh::ConvertIndex computes
/// index.x * pixels / xmax in int, which overflows above 65536 (ADR 0006).
constexpr int kMaxPixels = 65536;

/// The power of 2 p >= 1 minimising |boxSide / p - targetVoxel|, ties going to
/// the larger p, then capped at maxPixels. Throws std::invalid_argument when
/// boxSide, targetVoxel or maxPixels is not > 0.
int PixelCount(double boxSide, double targetVoxel, int maxPixels = kMaxPixels);

/// True when the maxPixels cap changed the result of PixelCount.
bool PixelCountCapped(double boxSide, double targetVoxel, int maxPixels = kMaxPixels);

/// start, then start * 10^(k / perDecade) for every such value < end, then end.
/// Strictly increasing. Throws std::invalid_argument when start >= end,
/// start <= 0 or perDecade < 1.
std::vector<double> LogTimeGrid(double start, double end, int perDecade);

/// Process-wide settings, held in the caller's unit system. Defaults are in
/// Geant4 internal units (ns, mm), where ns = 1 and mm = 1.
struct Values
{
  double handOverTime = 5.;     ///< ns
  double voxelSize = 6.25e-6;   ///< mm (6.25 nm)
  int timesPerDecade = 10;
  bool spatialOutput = false;
  /// /chem/meso/enable: whether the particle-based stage hands over to the
  /// mesoscopic stage. Read through StageEnabled(), never directly.
  bool enabled = true;
  /// True once /chem/meso/enable was issued (either value), so that the
  /// default can be told apart from an explicit "true".
  bool enabledExplicitly = false;
};

Values& Current();

/// Whether the mesoscopic stage runs: `values.enabled`, except under the SBS
/// time-step model (`sbsModel`), which has no hand-over; there the default
/// (true) silently means off.
bool StageEnabled(const Values& values, bool sbsModel);

/// True when the stage was enabled explicitly (/chem/meso/enable true) while
/// the time-step model is SBS: a configuration error the caller reports.
bool ExplicitlyEnabledWithSbs(const Values& values, bool sbsModel);
}  // namespace MesoSettings

#endif  // MesoSettings_h
