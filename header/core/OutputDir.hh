/// \file OutputDir.hh
/// \brief Process-wide output paths and lazy default-directory creation.
/// Uses Geant4 types but no kernel calls; not standard-library-only.
/// Configure before worker reads. Callers must coordinate later configuration,
/// prefix and subfolder changes with readers; only effective-root lookup and
/// SetDefaultDir are mutex-protected. Paths are strings, with no physical units.
///
/// The root is the configured directory, else the default set by SetDefaultDir,
/// else cwd. The default is created on first use; failure silently drops it and
/// falls back to cwd. Once used, a different ConfigureFromMacro path is refused.
/// Resolve combines <root>/<subdir>/<prefix><filename>; ResolveInRoot ignores
/// subdir and prefix. No logging, process exit or G4Exception: configuration
/// errors use bool + err, while allocation/path-conversion exceptions may escape.
/// err is only assigned on failure, not cleared on success.

#ifndef OutputDir_h
#define OutputDir_h 1

#include "globals.hh"

namespace OutputDir
{
  /// Sets the configured output directory to dir. If dir is empty, records
  /// that and returns true immediately (no filesystem access). Otherwise
  /// creates only dir itself (non-recursive -- its parent must already
  /// exist); returns false and fills err if the parent is missing, or if
  /// dir exists but is not a directory, or creation fails. Coordinate with readers.
  G4bool Configure(const G4String &dir, G4String &err);

  /// Sets the directory used when none is configured (empty = no default,
  /// the initial state: output then goes to cwd). Not created until needed.
  /// Resets the "default already used" state. Call before any output.
  void SetDefaultDir(const G4String &dir);

  /// Joins the output directory (configured, else default) with filename,
  /// applying the current subfolder and prefix even when the root is cwd.
  /// filename is normally a relative output filename; does not create the file.
  G4String Resolve(const G4String &filename);

  /// Like Resolve(), but ignores the prefix and the subfolder: the file sits
  /// directly in the output directory (the results index uses it).
  /// filename is normally relative; does not create the file.
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
  /// err describing the conflict. Also refuses a different path after the
  /// default root has been used. Coordinate configuration with readers.
  G4bool ConfigureFromMacro(const G4String &dir, G4String &err);

  /// Sets the filename prefix prepended (literally, no separator
  /// inserted) to every filename passed to Resolve(), until the next
  /// SetPrefix() call. Empty (the default) means no prefix.
  /// Coordinate with readers; no validation or filesystem access.
  void SetPrefix(const G4String &prefix);

  /// Sets a subfolder relative to the effective root (configured, default,
  /// or cwd) that Resolve() inserts between the
  /// directory and the prefixed filename, until the next ConfigureSubdir()
  /// call. Creates it (and any missing intermediate folders) if needed; an
  /// already-existing folder is accepted. Empty subdir clears it (no
  /// filesystem access). Returns false and fills err -- leaving the
  /// previous subfolder unchanged -- if subdir is absolute, contains a ".."
  /// component, exists but is not a directory, or cannot be created.
  /// Coordinate with readers; resolving the root can materialize the default.
  G4bool ConfigureSubdir(const G4String &subdir, G4String &err);
}

#endif // OutputDir_h
