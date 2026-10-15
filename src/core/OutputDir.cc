/// \file OutputDir.cc
/// \brief Implementation of OutputDir

#include "core/OutputDir.hh"

#include <filesystem>
#include <mutex>

namespace
{
  G4String gConfiguredDir = "";
  G4String gPrefix = "";
  G4String gSubdir = "";

  // Default directory (SetDefaultDir): created lazily on first use, so a run
  // that sets --dir or /run/outputDir never creates it. gDefaultUsed is true
  // once the default has been created/returned; from then on a later
  // ConfigureFromMacro to a different directory is refused (files may
  // already be there). The mutex covers the lazy creation, because the first
  // Resolve() can come from a worker thread.
  G4String gDefaultDir = "";
  G4bool gDefaultUsed = false;
  std::mutex gDefaultMutex;

  // The directory output goes to: the configured one, else the default
  // (materialised now), else empty (= cwd). If the default cannot be created
  // it is dropped and output falls back to cwd.
  G4String EffectiveDir()
  {
    std::lock_guard<std::mutex> lock(gDefaultMutex);
    if (!gConfiguredDir.empty())
      return gConfiguredDir;
    if (gDefaultDir.empty())
      return "";
    if (!gDefaultUsed)
    {
      std::error_code ec;
      std::filesystem::create_directories(std::filesystem::path(gDefaultDir.c_str()), ec);
      if (ec || !std::filesystem::is_directory(std::filesystem::path(gDefaultDir.c_str()), ec))
      {
        gDefaultDir = "";
        return "";
      }
      gDefaultUsed = true;
    }
    return gDefaultDir;
  }
}

void OutputDir::SetDefaultDir(const G4String &dir)
{
  std::lock_guard<std::mutex> lock(gDefaultMutex);
  gDefaultDir = dir;
  gDefaultUsed = false;
}

G4bool OutputDir::Configure(const G4String &dir, G4String &err)
{
  if (dir.empty())
  {
    gConfiguredDir = "";
    return true;
  }

  std::filesystem::path target(dir.c_str());
  std::error_code ec;

  if (std::filesystem::exists(target, ec))
  {
    if (!std::filesystem::is_directory(target, ec))
    {
      err = dir + ": already exists and is not a directory";
      return false;
    }
    gConfiguredDir = dir;
    return true;
  }

  if (!std::filesystem::create_directory(target, ec) || ec)
  {
    err = dir + ": could not create directory (does its parent exist?)";
    return false;
  }

  gConfiguredDir = dir;
  return true;
}

G4bool OutputDir::ConfigureFromMacro(const G4String &dir, G4String &err)
{
  if (!gConfiguredDir.empty())
  {
    if (dir == gConfiguredDir)
      return true;

    err = "output directory already set to '" + gConfiguredDir + "'; '" + dir
          + "' conflicts with it -- use only one of --dir or /run/outputDir, "
            "with matching values";
    return false;
  }

  if (gDefaultUsed && dir != gDefaultDir)
  {
    err = "output already went to the default directory '" + gDefaultDir + "'; '" + dir
          + "' came too late -- set /run/outputDir before the first output is written";
    return false;
  }

  return Configure(dir, err);
}

G4String OutputDir::Resolve(const G4String &filename)
{
  G4String name = gPrefix.empty() ? filename : G4String(gPrefix + filename);

  const G4String dir = EffectiveDir();
  if (dir.empty() && gSubdir.empty())
    return name;

  std::filesystem::path joined(dir.c_str());
  if (!gSubdir.empty())
    joined /= gSubdir.c_str();
  joined /= name.c_str();
  return G4String(joined.string().c_str());
}

G4String OutputDir::ResolveInRoot(const G4String &filename)
{
  const G4String dir = EffectiveDir();
  if (dir.empty())
    return filename;

  std::filesystem::path joined(dir.c_str());
  joined /= filename.c_str();
  return G4String(joined.string().c_str());
}

G4String OutputDir::GetDirectory()
{
  return EffectiveDir();
}

void OutputDir::SetPrefix(const G4String &prefix)
{
  gPrefix = prefix;
}

G4bool OutputDir::ConfigureSubdir(const G4String &subdir, G4String &err)
{
  if (subdir.empty())
  {
    gSubdir = "";
    return true;
  }

  std::filesystem::path sub(subdir.c_str());
  if (sub.has_root_path())
  {
    err = subdir + ": must be a relative path";
    return false;
  }
  for (const auto &part : sub)
  {
    if (part == "..")
    {
      err = subdir + ": must not contain a '..' component";
      return false;
    }
  }

  std::filesystem::path target(EffectiveDir().c_str());
  target /= sub;

  std::error_code ec;
  if (std::filesystem::exists(target, ec))
  {
    if (!std::filesystem::is_directory(target, ec))
    {
      err = subdir + ": already exists and is not a directory";
      return false;
    }
  }
  else if (!std::filesystem::create_directories(target, ec) || ec)
  {
    err = subdir + ": could not create directory";
    return false;
  }

  gSubdir = subdir;
  return true;
}
