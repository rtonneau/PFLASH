// LoadedModules: which Geant4 libraries this process actually loaded.
//
// The ledger records the full paths so a run can show that every G4 module came
// from the Geant4 install the build was configured against (and not from another
// Geant4 earlier on PATH). Geant4-free.
#pragma once

#include <string>
#include <vector>

namespace ptirt
{

// True where the loaded modules can be listed (Windows). Elsewhere
// ListLoadedGeant4Modules() returns an empty list.
bool LoadedModulesSupported();

// Full paths (UTF-8) of the loaded modules whose file name starts with "G4"
// (case-insensitive), sorted so the result does not depend on load order.
// Throws std::runtime_error if the operating system refuses to list them.
std::vector<std::string> ListLoadedGeant4Modules();

// The filter used above, on its own: true when the file name of `path`
// (after the last '/' or '\') starts with "G4", ignoring case.
bool IsGeant4ModulePath(const std::string& path);

}  // namespace ptirt
