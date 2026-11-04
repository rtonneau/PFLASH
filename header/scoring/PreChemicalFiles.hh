/// \file PreChemicalFiles.hh
/// \brief Name, discover and publish staged pre-chemical files.
/// Standard library only.
/// Callers coordinate filesystem changes and resolver access.
///
/// Portable and project-agnostic: standard library only, no dependency on
/// other dnachem-min classes (adjust the include path prefix on copy).
/// Design record: the addendum of docs/adr/0005-manifest-per-dump.md.
///
/// Staging folder: <outdir>/.pending_prechem/. Each event writes one
/// PreChemical_run<R>_event<E>.<ext> per per-event format (txt, csv); the
/// table formats (h5, root) stage one PreChemical.<ext> per dump. At dump time
/// MoveStaged() moves every staged file into the dump, at a target chosen by
/// the caller.

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

  /// Same with the extension ("txt" or "csv", no dot): "PreChemical_run<R>_event<E>.<ext>".
  std::string StagedFileName(int runId, int eventId, const std::string& extension);

  /// Staged table file of a table format: "PreChemical.<ext>" ("h5" or "root").
  std::string StagedTableFileName(const std::string& extension);

  /// Match the exact pattern with one to nine decimal digits per ID and the
  /// extension txt or csv. On false, runId/eventId are unchanged; string
  /// allocations may throw.
  bool ParseStagedFileName(const std::string& name, int& runId, int& eventId);

  /// Same, also returning the extension ("txt" or "csv"); extension is also
  /// unchanged on false.
  bool ParseStagedFileName(const std::string& name, int& runId, int& eventId,
                           std::string& extension);

  /// True for exactly "PreChemical.h5" and "PreChemical.root".
  bool IsStagedTableFileName(const std::string& name);

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

  /// Moves every regular staged file accepted by ParseStagedFileName or
  /// IsStagedTableFileName, in numeric (run, event) order (txt before csv for
  /// one event), then PreChemical.h5, then PreChemical.root, to resolveTarget(stagedFileName). An existing
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
