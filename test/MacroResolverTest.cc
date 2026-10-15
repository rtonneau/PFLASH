/// \file MacroResolverTest.cc
/// \brief Plain-assert unit tests for MacroResolver (no Geant4 runtime).
#include "core/MacroResolver.hh"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;

namespace
{
void Touch(const fs::path& p)
{
  fs::create_directories(p.parent_path());
  std::ofstream(p) << "# test\n";
}
}  // namespace

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  const fs::path root = fs::temp_directory_path() / "MacroResolverTest_tmp";
  fs::remove_all(root);
  const fs::path exeDir = root / "exe";
  fs::create_directories(exeDir);
  const std::string exe = exeDir.string();

  // Nothing exists: three tried paths, in order.
  auto r = MacroResolver::Resolve("a.in", exe);
  assert(!r.found && r.tried.size() == 3);
  assert(r.tried[0] == "a.in");
  assert(r.tried[1] == (exeDir / "a.in").string());
  assert(r.tried[2] == (exeDir / "macro" / "a.in").string());

  // Only <exeDir>/macro/<arg>.
  Touch(exeDir / "macro" / "a.in");
  r = MacroResolver::Resolve("a.in", exe);
  assert(r.found && r.path == (exeDir / "macro" / "a.in").string());

  // <exeDir>/<arg> beats macro/.
  Touch(exeDir / "a.in");
  r = MacroResolver::Resolve("a.in", exe);
  assert(r.found && r.path == (exeDir / "a.in").string());

  // arg as given (absolute) beats both.
  const fs::path given = root / "given.in";
  Touch(given);
  r = MacroResolver::Resolve(given.string(), exe);
  assert(r.found && r.path == given.string());

  // A directory is not a macro.
  fs::create_directories(exeDir / "dir.in");
  r = MacroResolver::Resolve("dir.in", exe);
  assert(!r.found);

  fs::remove_all(root);
  std::cout << "MacroResolverTest passed\n";
  return 0;
}
