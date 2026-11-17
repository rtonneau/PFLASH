// Shared harness of the patch-necessity suite (test/dose-rate-patches).
//
// Each fixture is run with --expect defect (on a kernel without its patch) or
// --expect fixed (on a kernel with it). It measures, prints machine-readable
// "RESULT key=value" lines, then checks the expectation of the selected mode.
// The fixture passes (exit 0) only when the demonstration holds: the defect is
// observed on the unpatched kernel, or the correct result on the patched one.
// Checks are explicit, not assert(): Release builds define NDEBUG.
#ifndef PATCH_CHECK_HH
#define PATCH_CHECK_HH

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace PatchCheck
{
enum class Expect { Defect, Fixed };

inline Expect ParseExpect(int argc, char** argv)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--expect") {
      const std::string v = argv[i + 1];
      if (v == "defect") return Expect::Defect;
      if (v == "fixed") return Expect::Fixed;
    }
  }
  std::cerr << "usage: " << argv[0] << " --expect defect|fixed [fixture options]\n";
  std::exit(2);
}

inline const char* Name(Expect e) { return e == Expect::Defect ? "defect" : "fixed"; }

// Value of "--<key> <value>" or the fallback.
inline std::string Option(int argc, char** argv, const std::string& key, const std::string& fallback)
{
  for (int i = 1; i + 1 < argc; ++i)
    if (std::string(argv[i]) == "--" + key) return argv[i + 1];
  return fallback;
}

class Checker
{
  public:
    Checker(std::string fixture, Expect expect) : fFixture(std::move(fixture)), fExpect(expect)
    {
      std::cout << "FIXTURE " << fFixture << " expect=" << Name(fExpect) << "\n";
    }

    template<typename T>
    void Result(const std::string& key, const T& value)
    {
      std::ostringstream os;
      os.precision(12);
      os << value;
      std::cout << "RESULT " << fFixture << "." << key << "=" << os.str() << "\n";
    }

    void Check(bool condition, const std::string& what)
    {
      std::cout << (condition ? "  ok   " : "  FAIL ") << what << "\n";
      if (!condition) ++fFailures;
    }

    // Print the full path of the loaded DLL 'dll' (e.g. "G4processes_core.dll"), so the
    // runner can check the kernel under test. (A function address taken in the exe
    // resolves to its import thunk, not to the DLL.)
    void Module(const std::string& key, const std::string& dll)
    {
#ifdef _WIN32
      char path[MAX_PATH] = {};
      HMODULE h = GetModuleHandleA(dll.c_str());
      if (h && GetModuleFileNameA(h, path, MAX_PATH) > 0)
        std::cout << "MODULE " << key << "=" << path << '\n';
      else
        std::cout << "MODULE " << key << "=<not loaded>" << '\n';
#else
      (void)key;
      (void)dll;
#endif
    }

    static bool Near(double a, double b, double tol = 1e-9) { return std::abs(a - b) <= tol; }

    int Finish() const
    {
      std::cout << "VERDICT " << fFixture << " " << Name(fExpect) << " "
                << (fFailures == 0 ? "PASS" : "FAIL") << "\n";
      return fFailures == 0 ? 0 : 1;
    }

  private:
    std::string fFixture;
    Expect fExpect;
    int fFailures = 0;
};
}  // namespace PatchCheck

#endif
