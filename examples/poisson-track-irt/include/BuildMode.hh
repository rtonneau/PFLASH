// BuildMode: which Geant4 this example was built against. Geant4-free.
//
// CMakeLists.txt defines PTIRT_PATCHED_GEANT4 as 0 or 1 (option
// PTIRT_PATCHED_GEANT4) after checking that the Geant4 headers agree with it:
//   0  stock 11.4.3: no ResetSourceActivations / RegisterSourceActivation.
//   1  11.4.3 with the timed-injection patches
//      (patches/geant4/11.4.3/timed-injection/README.md).
// The macro has no default on purpose: a file that forgot to get the definition
// would silently take the stock branch of an `#if`.
#pragma once

#ifndef PTIRT_PATCHED_GEANT4
#error "PTIRT_PATCHED_GEANT4 is not defined: CMakeLists.txt must define it as 0 or 1"
#endif

namespace ptirt
{

#if PTIRT_PATCHED_GEANT4

inline constexpr bool kPatchedGeant4 = true;
// "stock" or "patched", written to the tag, capture.json and the ledger.
inline constexpr const char kBuildModeName[] = "patched";
// Where capture reads the molecules: "chemistryStart" (the delayed lists, before
// any chemistry step) or "afterStartupContacts" (the first positive step).
inline constexpr const char kCaptureStageName[] = "chemistryStart";
// Which fold the kernel applies to in-box transport (G4ChemReboundTransportation).
inline constexpr const char kKernelFoldName[] = "patched";

#else

inline constexpr bool kPatchedGeant4 = false;
inline constexpr const char kBuildModeName[] = "stock";
inline constexpr const char kCaptureStageName[] = "afterStartupContacts";
inline constexpr const char kKernelFoldName[] = "stock";

#endif

}  // namespace ptirt
