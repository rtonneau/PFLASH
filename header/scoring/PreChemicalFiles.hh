/// \file PreChemicalFiles.hh
/// \brief Name, discover and publish staged pre-chemical files.
/// Standard library only.
/// Callers coordinate filesystem changes and resolver access.
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
  /// Format runId/eventId as "PreChemical_run<R>_event<E>.txt"; no validation.
  /// For a parseable result, use nonnegative IDs with at most nine decimal digits.
  std::string StagedFileName(int runId, int eventId);

  /// Match the exact pattern with one to nine decimal digits per ID.
  /// On false, runId/eventId are unchanged; string allocations may throw.
  bool ParseStagedFileName(const std::string& name, int& runId, int& eventId);

  /// outputDir + "/.pending_prechem", or ".pending_prechem" when outputDir is empty.
  std::string StagingDir(const std::string& outputDir);

  /// Create the staging folder and parents. Filesystem errors return false
  /// with err set; success leaves err unchanged. Allocations may throw.
  bool EnsureStagingDir(const std::string& outputDir, std::string& err);

  /// Partial publication results: successful names and per-operation errors.
  struct MoveResult
  {
    std::vector<std::string> moved;    ///< target filenames (prefix included)
    std::vector<std::string> failures; ///< messages; the file stays staged
  };

  /// Moves every regular staged file accepted by ParseStagedFileName, in
  /// numeric (run, event) order, to resolveTarget(stagedFileName). An existing
  /// target is removed first. A non-directory staging path (including an
  /// error checking it) gives empty lists. Directory-scan and move errors are
  /// collected in failures; per-entry file-type check errors skip that entry.
  /// Successful moves remain published. A failed rename may follow
  /// deletion of the old target. Exceptions from resolveTarget/allocations
  /// propagate. The caller must stop writers before publishing staged files.
  MoveResult MoveStaged(const std::string& stagingDir,
                        const std::function<std::string(const std::string&)>& resolveTarget);
}

#endif
