/// \file OutputDir.hh
/// \brief Process-wide output directory, set once from main() before any
/// worker thread is created.
///
/// Pure: no logging, no process exit. Configure(dir, err) sets the
/// process-wide output directory to dir, creating it if non-empty (its
/// parent must already exist); Resolve() joins that directory onto a
/// filename, or returns the filename unchanged if the configured directory
/// is empty (including before Configure() is ever called). Not
/// thread-synchronized -- callers must call Configure() before spawning any
/// worker thread that calls Resolve(). SetPrefix()/Resolve() also apply an
/// independent filename prefix (prepended before the directory join),
/// separate from the configured directory. ConfigureSubdir() adds an
/// optional subfolder between the two: <dir>/<subdir>/<prefix><filename>.
///
/// Default directory: SetDefaultDir() (called from sim.cc with
/// <exeDir>/results) names a directory used only when no Configure() /
/// ConfigureFromMacro() directory is set. It is applied lazily -- created
/// at the first Resolve()/GetDirectory()/ConfigureSubdir() that needs it,
/// under a mutex because that can be a worker thread -- so --dir and
/// /run/outputDir (a PreInit command, issued before any output exists) still
/// replace it and a run that sets one never creates it. Once the default has
/// been used, a /run/outputDir naming another directory is refused.

#ifndef OutputDir_h
#define OutputDir_h 1

#include "globals.hh"

namespace OutputDir
{
  /// Sets the configured output directory to dir. If dir is empty, records
  /// that and returns true immediately (no filesystem access). Otherwise
  /// creates only dir itself (non-recursive -- its parent must already
  /// exist); returns false and fills err if the parent is missing, or if
  /// dir exists but is not a directory.
  G4bool Configure(const G4String &dir, G4String &err);

  /// Sets the directory used when none is configured (empty = no default,
  /// the initial state: output then goes to cwd). Not created until needed.
  /// Resets the "default already used" state. Call before any output.
  void SetDefaultDir(const G4String &dir);

  /// Joins the output directory (configured, else default) with filename,
  /// or returns filename unchanged if there is none.
  G4String Resolve(const G4String &filename);

  /// Like Resolve(), but ignores the prefix and the subfolder: the file sits
  /// directly in the output directory (the results index uses it).
  G4String ResolveInRoot(const G4String &filename);

  /// The output directory: as given to Configure() / ConfigureFromMacro(),
  /// else the default directory (created now if needed), else empty. Ignores
  /// the prefix and the subfolder.
  G4String GetDirectory();

  /// Sets the configured output directory from a macro command, for use
  /// alongside (never together with a differing value from) Configure().
  /// If no directory is configured yet, behaves exactly like Configure(dir,
  /// err). If a directory is already configured (by an earlier Configure()
  /// or ConfigureFromMacro() call) and dir is the same raw string, this is a
  /// harmless no-op that returns true. If a directory is already configured
  /// and dir differs, state is left unchanged and this returns false with
  /// err describing the conflict.
  G4bool ConfigureFromMacro(const G4String &dir, G4String &err);

  /// Sets the filename prefix prepended (literally, no separator
  /// inserted) to every filename passed to Resolve(), until the next
  /// SetPrefix() call. Empty (the default) means no prefix.
  void SetPrefix(const G4String &prefix);

  /// Sets a subfolder (relative to the configured output directory, or to
  /// cwd if none is configured) that Resolve() inserts between the
  /// directory and the prefixed filename, until the next ConfigureSubdir()
  /// call. Creates it (and any missing intermediate folders) if needed; an
  /// already-existing folder is accepted. Empty subdir clears it (no
  /// filesystem access). Returns false and fills err -- leaving the
  /// previous subfolder unchanged -- if subdir is absolute, contains a ".."
  /// component, exists but is not a directory, or cannot be created.
  G4bool ConfigureSubdir(const G4String &subdir, G4String &err);
}

#endif // OutputDir_h
