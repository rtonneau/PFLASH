/// \file OutputDirTest.cc
/// \brief Plain-assert unit tests for OutputDir (no test framework, no
/// Geant4 runtime -- exercises pure directory-configuration/path-join logic
/// only).

#include "core/OutputDir.hh"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
  fs::path TestRoot()
  {
    return fs::temp_directory_path() / "OutputDirTest";
  }

  void ResetTestRoot()
  {
    std::error_code ec;
    fs::remove_all(TestRoot(), ec);
    fs::create_directories(TestRoot(), ec);
  }
}

// --- Configure ---------------------------------------------------------

static void TestConfigureEmptyDirIsNoOp()
{
  G4String err;
  assert(OutputDir::Configure("", err));
  assert(err.empty());
}

static void TestConfigureCreatesMissingLeafDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "fresh";
  assert(!fs::exists(target));

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  assert(fs::exists(target));
  assert(fs::is_directory(target));
}

static void TestConfigureAcceptsAlreadyExistingDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "already-there";
  fs::create_directory(target);

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
}

static void TestConfigureRejectsPathThatIsAFile()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "im-a-file";
  std::ofstream(target.string()) << "x";

  G4String err;
  assert(!OutputDir::Configure(target.string().c_str(), err));
  assert(!err.empty());
}

static void TestConfigureRejectsMissingParent()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "no-such-parent" / "leaf";

  G4String err;
  assert(!OutputDir::Configure(target.string().c_str(), err));
  assert(!err.empty());
  assert(!fs::exists(target));
}

// --- ConfigureFromMacro --------------------------------------------------

static void TestConfigureFromMacroSetsDirWhenNoneConfiguredYet()
{
  ResetTestRoot();
  G4String err;
  assert(OutputDir::Configure("", err)); // simulates --dir absent

  fs::path target = TestRoot() / "macro-only";
  assert(OutputDir::ConfigureFromMacro(target.string().c_str(), err));
  assert(err.empty());
  assert(fs::exists(target));
  assert(fs::is_directory(target));
}

static void TestConfigureFromMacroNoOpWhenSamePathAlreadyConfigured()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "same-path";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err)); // simulates --dir <target>
  assert(OutputDir::ConfigureFromMacro(target.string().c_str(), err));
  assert(err.empty());
}

static void TestConfigureFromMacroConflictsWithDifferentCliPath()
{
  ResetTestRoot();
  fs::path cliTarget = TestRoot() / "from-cli";
  fs::path macroTarget = TestRoot() / "from-macro";

  G4String err;
  assert(OutputDir::Configure(cliTarget.string().c_str(), err)); // simulates --dir <cliTarget>
  assert(!OutputDir::ConfigureFromMacro(macroTarget.string().c_str(), err));
  assert(!err.empty());

  // State must be unchanged: Resolve() still uses the CLI-configured directory.
  fs::path expected = cliTarget / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

static void TestConfigureFromMacroConflictsWithDifferentEarlierMacroPath()
{
  ResetTestRoot();
  G4String err;
  assert(OutputDir::Configure("", err)); // simulates --dir absent

  fs::path firstTarget = TestRoot() / "macro-first";
  fs::path secondTarget = TestRoot() / "macro-second";

  assert(OutputDir::ConfigureFromMacro(firstTarget.string().c_str(), err));
  assert(!OutputDir::ConfigureFromMacro(secondTarget.string().c_str(), err));
  assert(!err.empty());

  fs::path expected = firstTarget / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

// --- Resolve -------------------------------------------------------------

static void TestResolveWithoutConfigureReturnsFilenameUnchanged()
{
  G4String err;
  OutputDir::Configure("", err); // explicit empty configure = "not set"

  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestResolveJoinsConfiguredDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "joined";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));

  fs::path expected = target / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

// --- SetPrefix / Resolve prefix -------------------------------------------

static void TestResolveWithoutPrefixLeavesFilenameUnchanged()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("");

  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestResolvePrependsPrefixToFilename()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("run1_");

  assert(OutputDir::Resolve("Species.Txt") == "run1_Species.Txt");

  OutputDir::SetPrefix(""); // restore for later tests
}

static void TestResolveAppliesPrefixBeforeJoiningDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "prefixed";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  OutputDir::SetPrefix("run2_");

  fs::path expected = target / "run2_Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  OutputDir::SetPrefix("");        // restore for later tests
  OutputDir::Configure("", err);   // restore for later tests
}

// --- ConfigureSubdir / Resolve subdir -------------------------------------

static void TestConfigureSubdirCreatesFolderUnderConfiguredDir()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-base";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(OutputDir::ConfigureSubdir("run01", err));
  assert(fs::is_directory(base / "run01"));

  fs::path expected = base / "run01" / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  assert(OutputDir::ConfigureSubdir("", err));
  OutputDir::Configure("", err); // restore for later tests
}

static void TestConfigureSubdirCreatesNestedFolders()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-nested";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(OutputDir::ConfigureSubdir("scan1/run01", err));
  assert(fs::is_directory(base / "scan1" / "run01"));

  OutputDir::ConfigureSubdir("", err);
  OutputDir::Configure("", err);
}

static void TestConfigureSubdirAcceptsAlreadyExistingFolder()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-existing";
  fs::create_directories(base / "run01");

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(OutputDir::ConfigureSubdir("run01", err));

  OutputDir::ConfigureSubdir("", err);
  OutputDir::Configure("", err);
}

static void TestConfigureSubdirRejectsAbsolutePath()
{
  ResetTestRoot();
  fs::path absolute = TestRoot() / "elsewhere";

  G4String err;
  OutputDir::Configure("", err);
  assert(!OutputDir::ConfigureSubdir(absolute.string().c_str(), err));
  assert(!err.empty());
  assert(!fs::exists(absolute));
  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestConfigureSubdirRejectsParentTraversal()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-escape";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(!OutputDir::ConfigureSubdir("../escaped", err));
  assert(!err.empty());
  assert(!fs::exists(TestRoot() / "escaped"));

  G4String err2;
  assert(!OutputDir::ConfigureSubdir("a/../../escaped", err2));
  assert(!err2.empty());

  OutputDir::Configure("", err);
}

static void TestConfigureSubdirRejectsPathThatIsAFile()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-file";
  fs::create_directories(base);
  std::ofstream(base / "run01") << "not a directory";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(!OutputDir::ConfigureSubdir("run01", err));
  assert(!err.empty());

  OutputDir::Configure("", err);
}

static void TestResolveCombinesSubdirAndPrefix()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-prefix";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(OutputDir::ConfigureSubdir("run02", err));
  OutputDir::SetPrefix("p_");

  fs::path expected = base / "run02" / "p_Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  OutputDir::SetPrefix("");
  OutputDir::ConfigureSubdir("", err);
  OutputDir::Configure("", err);
}

static void TestClearingSubdirRestoresPlainResolve()
{
  ResetTestRoot();
  fs::path base = TestRoot() / "sub-clear";

  G4String err;
  assert(OutputDir::Configure(base.string().c_str(), err));
  assert(OutputDir::ConfigureSubdir("run03", err));
  assert(OutputDir::ConfigureSubdir("", err));

  fs::path expected = base / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  OutputDir::Configure("", err);
}

// --- GetDirectory ------------------------------------------------------

static void TestGetDirectoryReturnsConfiguredDirAsGiven()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "reported";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  assert(OutputDir::GetDirectory() == G4String(target.string().c_str()));
}

static void TestGetDirectoryIsEmptyWhenNoneConfigured()
{
  G4String err;
  assert(OutputDir::Configure("", err));
  assert(OutputDir::GetDirectory().empty());
}

// --- Default directory ---------------------------------------------------

static void TestDefaultDirUsedLazilyWhenNothingConfigured()
{
  ResetTestRoot();
  fs::path def = TestRoot() / "results";
  G4String err;
  assert(OutputDir::Configure("", err));
  OutputDir::SetDefaultDir(def.string().c_str());
  assert(!fs::exists(def)); // lazy: nothing created yet

  fs::path expected = def / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
  assert(fs::is_directory(def));
  assert(OutputDir::GetDirectory() == G4String(def.string().c_str()));
  assert(OutputDir::ResolveInRoot("Manifest.json") == G4String((def / "Manifest.json").string().c_str()));

  OutputDir::SetDefaultDir("");
}

static void TestConfigureOverridesDefaultAndNeverCreatesIt()
{
  ResetTestRoot();
  fs::path def = TestRoot() / "results-unused";
  fs::path chosen = TestRoot() / "chosen";
  G4String err;
  assert(OutputDir::Configure("", err));
  OutputDir::SetDefaultDir(def.string().c_str());

  // /run/outputDir after the default was set (not yet used) still works.
  assert(OutputDir::ConfigureFromMacro(chosen.string().c_str(), err));
  assert(OutputDir::Resolve("a") == G4String((chosen / "a").string().c_str()));
  assert(!fs::exists(def));

  OutputDir::SetDefaultDir("");
  OutputDir::Configure("", err);
}

static void TestMacroDirAfterDefaultUsedIsRefused()
{
  ResetTestRoot();
  fs::path def = TestRoot() / "results-used";
  G4String err;
  assert(OutputDir::Configure("", err));
  OutputDir::SetDefaultDir(def.string().c_str());
  OutputDir::Resolve("a"); // first use materialises the default

  assert(!OutputDir::ConfigureFromMacro((TestRoot() / "late").string().c_str(), err));
  assert(!err.empty());

  OutputDir::SetDefaultDir("");
}

static void TestDefaultDirWithSubdirAndRootResolve()
{
  ResetTestRoot();
  fs::path def = TestRoot() / "results-sub";
  G4String err;
  assert(OutputDir::Configure("", err));
  OutputDir::SetDefaultDir(def.string().c_str());

  assert(OutputDir::ConfigureSubdir("sub_01", err));
  OutputDir::SetPrefix("p_");
  assert(fs::is_directory(def / "sub_01"));
  assert(OutputDir::Resolve("X") == G4String((def / "sub_01" / "p_X").string().c_str()));
  // Root resolution ignores prefix and subfolder.
  assert(OutputDir::ResolveInRoot("Manifest.json") == G4String((def / "Manifest.json").string().c_str()));

  OutputDir::SetPrefix("");
  OutputDir::ConfigureSubdir("", err);
  OutputDir::SetDefaultDir("");
}

static void TestResolveInRootWithoutAnyDirIsUnchanged()
{
  G4String err;
  assert(OutputDir::Configure("", err));
  OutputDir::SetDefaultDir("");
  assert(OutputDir::ResolveInRoot("Manifest.json") == "Manifest.json");
}

int main()
{
  TestConfigureEmptyDirIsNoOp();
  TestConfigureCreatesMissingLeafDirectory();
  TestConfigureAcceptsAlreadyExistingDirectory();
  TestConfigureRejectsPathThatIsAFile();
  TestConfigureRejectsMissingParent();
  TestConfigureFromMacroSetsDirWhenNoneConfiguredYet();
  TestConfigureFromMacroNoOpWhenSamePathAlreadyConfigured();
  TestConfigureFromMacroConflictsWithDifferentCliPath();
  TestConfigureFromMacroConflictsWithDifferentEarlierMacroPath();
  TestResolveWithoutConfigureReturnsFilenameUnchanged();
  TestResolveJoinsConfiguredDirectory();
  TestResolveWithoutPrefixLeavesFilenameUnchanged();
  TestResolvePrependsPrefixToFilename();
  TestResolveAppliesPrefixBeforeJoiningDirectory();
  TestConfigureSubdirCreatesFolderUnderConfiguredDir();
  TestConfigureSubdirCreatesNestedFolders();
  TestConfigureSubdirAcceptsAlreadyExistingFolder();
  TestConfigureSubdirRejectsAbsolutePath();
  TestConfigureSubdirRejectsParentTraversal();
  TestConfigureSubdirRejectsPathThatIsAFile();
  TestResolveCombinesSubdirAndPrefix();
  TestClearingSubdirRestoresPlainResolve();
  TestGetDirectoryReturnsConfiguredDirAsGiven();
  TestGetDirectoryIsEmptyWhenNoneConfigured();
  TestDefaultDirUsedLazilyWhenNothingConfigured();
  TestConfigureOverridesDefaultAndNeverCreatesIt();
  TestMacroDirAfterDefaultUsedIsRefused();
  TestDefaultDirWithSubdirAndRootResolve();
  TestResolveInRootWithoutAnyDirIsUnchanged();

  std::error_code ec;
  fs::remove_all(TestRoot(), ec);

  std::cout << "All OutputDir tests passed." << std::endl;
  return 0;
}
