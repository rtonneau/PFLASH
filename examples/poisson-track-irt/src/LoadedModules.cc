#include "LoadedModules.hh"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// psapi.h needs windows.h first.
#include <psapi.h>
#endif

namespace ptirt
{

bool IsGeant4ModulePath(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  return name.size() >= 2 && std::toupper(static_cast<unsigned char>(name[0])) == 'G' && name[1] == '4';
}

#ifdef _WIN32

bool LoadedModulesSupported() { return true; }

namespace
{

std::string ToUtf8(const std::wstring& wide)
{
  if (wide.empty()) return {};
  const int size =
      WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) throw std::runtime_error("cannot convert a module path to UTF-8");
  std::string utf8(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
  return utf8;
}

}  // namespace

std::vector<std::string> ListLoadedGeant4Modules()
{
  const HANDLE process = GetCurrentProcess();

  // The module list can grow between the size query and the fetch: retry until
  // the buffer is large enough.
  std::vector<HMODULE> modules(256);
  for (;;)
  {
    DWORD neededBytes = 0;
    if (!EnumProcessModules(process, modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)),
                            &neededBytes))
      throw std::runtime_error("EnumProcessModules failed (error " + std::to_string(GetLastError()) + ")");
    const std::size_t count = neededBytes / sizeof(HMODULE);
    if (count <= modules.size())
    {
      modules.resize(count);
      break;
    }
    modules.resize(count + 16);
  }

  std::vector<std::string> paths;
  for (const HMODULE module : modules)
  {
    std::wstring wide(32768, L'\0');  // long-path limit
    const DWORD length = GetModuleFileNameExW(process, module, wide.data(), static_cast<DWORD>(wide.size()));
    if (length == 0) continue;  // unloaded between the listing and now
    wide.resize(length);
    std::string path = ToUtf8(wide);
    if (IsGeant4ModulePath(path)) paths.push_back(std::move(path));
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

#else

bool LoadedModulesSupported() { return false; }

std::vector<std::string> ListLoadedGeant4Modules() { return {}; }

#endif

}  // namespace ptirt
