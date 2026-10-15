/// \file ResultsIndex.hh
/// \brief Pure builder of the results index: the top-level Manifest.json of
/// the output directory that lists every dump written so far.
///
/// Portable, project-agnostic unit: standard library and DataNode only (no
/// Geant4, no other dnachem-min class); adjust the include path prefix on
/// copy. RunManifest feeds it and writes the tree with JsonWriter.
///
/// Layout (top level): `schemaVersion`, `kind` ("resultsIndex"),
/// `outputDirAbsolute`, and `dumps[]`, one entry per dump in dump order:
/// `folder` (subfolder, "" for a flat dump), `prefix`, `manifest` (path of
/// the dump's own Manifest.json relative to the output directory, '/'
/// separated), `timestamp`, `events`, and `runs[]` (run, events, seed and the
/// beam of each /run/beamOn folded into the dump).
///
/// Collision: a flat dump with an empty prefix writes its own per-dump
/// manifest to the same path as the index (<outdir>/Manifest.json). Neither
/// may overwrite the other, so Build() takes that manifest as `base` and
/// returns it with a top-level `dumps` array added: one file that is both a
/// valid per-dump manifest and a valid index. Without such a dump the index
/// stands alone.

#ifndef ResultsIndex_h
#define ResultsIndex_h 1

#include "scoring/DataNode.hh"

#include <string>
#include <vector>

namespace ResultsIndex
{
  /// File name of the index, and of every per-dump manifest.
  inline const char *FileName() { return "Manifest.json"; }

  /// Builds one `dumps[]` entry. `runs` are the per-run entries of the
  /// dump's manifest (Objects); only run, events, seed, particle,
  /// beamEnergy_keV, position_um and direction are copied, those that are
  /// absent are skipped. `events` is the dump's total event count.
  DataNode MakeDumpEntry(const std::string &prefix, const std::string &subdir,
                         const std::string &timestamp, long long events,
                         const std::vector<DataNode> &runs);

  /// Builds the index tree from the dump entries (in dump order). If `base`
  /// is an Object (the manifest of an empty-prefix flat dump, see above), the
  /// result is a copy of it with `dumps` added (or replaced); otherwise a new
  /// Object with schemaVersion, kind, outputDirAbsolute and dumps.
  DataNode Build(const std::vector<DataNode> &dumps, const DataNode *base,
                 const std::string &outputDirAbsolute);
}

#endif // ResultsIndex_h
