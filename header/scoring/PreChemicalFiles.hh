/// \file PreChemicalFiles.hh
/// \brief Staging-folder helpers for the per-event pre-chemical files.
///
/// Portable and project-agnostic: standard library only, no dependency on
/// other dnachem-min classes (adjust the include path prefix on copy).
/// Design record: the addendum of docs/adr/0005-manifest-per-dump.md.
///
/// Each event writes PreChemical_run<R>_event<E>.txt into a staging folder
/// (<outdir>/.pending_prechem/). At dump time MoveStaged() moves every staged
/// file into the dump, at a target chosen by the caller.

#ifndef PreChemicalFiles_hh
#define PreChemicalFiles_hh 1

#include <functional>
#include <string>
#include <vector>

namespace PreChemicalFiles
{
  /// "PreChemical_run<R>_event<E>.txt"
  std::string StagedFileName(int runId, int eventId);

  /// True only for an exact match of the StagedFileName pattern.
  bool ParseStagedFileName(const std::string& name, int& runId, int& eventId);

  /// outputDir + "/.pending_prechem", or ".pending_prechem" when outputDir is empty.
  std::string StagingDir(const std::string& outputDir);

  /// Creates the staging folder (and parents). On failure returns false and sets err.
  bool EnsureStagingDir(const std::string& outputDir, std::string& err);

  struct MoveResult
  {
    std::vector<std::string> moved;    ///< target filenames (prefix included)
    std::vector<std::string> failures; ///< messages; the file stays staged
  };

  /// Moves every regular staged file accepted by ParseStagedFileName, in
  /// numeric (run, event) order, to resolveTarget(stagedFileName). An existing
  /// target is removed first. A missing staging folder gives empty lists.
  MoveResult MoveStaged(const std::string& stagingDir,
                        const std::function<std::string(const std::string&)>& resolveTarget);
}

#endif
