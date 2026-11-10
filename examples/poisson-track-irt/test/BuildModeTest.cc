#include <cstdio>
#include <string>

#include "BuildMode.hh"
#include "ExamplePhysicsTag.hh"
#include "LoadedModules.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

// The compile definition and the constants must agree.
void TestBuildModeNames()
{
#if PTIRT_PATCHED_GEANT4
  CHECK(kPatchedGeant4);
  CHECK(std::string(kBuildModeName) == "patched");
  CHECK(std::string(kCaptureStageName) == "chemistryStart");
  CHECK(std::string(kKernelFoldName) == "patched");
#else
  CHECK(!kPatchedGeant4);
  CHECK(std::string(kBuildModeName) == "stock");
  CHECK(std::string(kCaptureStageName) == "afterStartupContacts");
  CHECK(std::string(kKernelFoldName) == "stock");
#endif
}

void TestCompatibilityTag()
{
  const std::string tag = kPacketCompatibilityTag;
  CHECK(tag.rfind("ptirt2_", 0) == 0);
  CHECK(tag.find_first_of(" \t\r\n") == std::string::npos);
  const std::string expected = std::string("ptirt2_geant4-11.4.3-") + kBuildModeName +
                               "_G4EmDNAPhysics_option2_G4EmDNAChemistry_option3_IRT_syn_pureWater_age1ps-" +
                               kCaptureStageName;
  CHECK(tag == expected);
  // Each build names its own mode and stage, and never the other's.
  const std::string other = kPatchedGeant4 ? "stock" : "patched";
  CHECK(tag.find("-" + other + "_") == std::string::npos);
  CHECK(tag.find(kPatchedGeant4 ? "afterStartupContacts" : "chemistryStart") == std::string::npos);
}

void TestModuleFilter()
{
  CHECK(IsGeant4ModulePath("C:\\geant4\\bin\\G4global.dll"));
  CHECK(IsGeant4ModulePath("C:/geant4/bin/g4run.dll"));
  CHECK(IsGeant4ModulePath("G4clhep.dll"));
  CHECK(!IsGeant4ModulePath("C:\\Windows\\System32\\kernel32.dll"));
  CHECK(!IsGeant4ModulePath("C:\\g4dir\\bin\\zlib1.dll"));  // only the file name counts
  CHECK(!IsGeant4ModulePath("C:\\geant4\\bin\\"));
  CHECK(!IsGeant4ModulePath(""));
  CHECK(!IsGeant4ModulePath("G"));
}

// This test links no Geant4: the list is empty, but the call must succeed where
// supported and every entry must pass the filter.
void TestListLoadedModules()
{
  const auto modules = ListLoadedGeant4Modules();
  for (const auto& path : modules) CHECK(IsGeant4ModulePath(path));
#ifdef _WIN32
  CHECK(LoadedModulesSupported());
#else
  CHECK(!LoadedModulesSupported());
  CHECK(modules.empty());
#endif
}

}  // namespace

int main()
{
  TestBuildModeNames();
  TestCompatibilityTag();
  TestModuleFilter();
  TestListLoadedModules();
  std::puts("BuildModeTest passed");
  return 0;
}
