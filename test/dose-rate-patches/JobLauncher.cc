// Run a fixture inside a Windows Job Object (committed-memory cap, kill on close,
// deadline), adapted from test/molecular-finder/JobLauncher.cc.
//
//   JobLauncher <capMiB> <seconds> <prefix> <exe> --expect defect|fixed [args...]
//   JobLauncher <capMiB> <seconds> <prefix> --expect defect|fixed <exe> [args...]
//
// In the second form --expect is the launcher's own and is not passed to the
// child (for programs that reject unknown options, e.g. dose-rate-study history).
// The child's stdout/stderr go to <prefix>.log/.err and are echoed after it
// ends, followed by RESULT lines (cap hit, exit code, peak memory) and the
// verdict. A memory-cap hit is the observed defect: with --expect defect it is
// a PASS (exit 0), with --expect fixed a FAIL. Otherwise the child's own verdict
// and exit code stand.
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
std::string Narrow(const std::wstring& w) { return std::string(w.begin(), w.end()); }

void Echo(const std::wstring& path)
{
  std::ifstream in(path);
  std::cout << in.rdbuf();
  std::cout.clear();
}
}  // namespace

int wmain(int argc, wchar_t** argv)
{
  if (argc < 7) {
    std::cerr << "JobLauncher capMiB seconds prefix exe --expect defect|fixed [args...]\n";
    return 2;
  }
  const unsigned long long capBytes = std::stoull(argv[1]) * 1024ull * 1024ull;
  const double deadline = std::stod(argv[2]);
  const std::wstring prefix = argv[3];
  bool expectDefect = false;
  int exeIndex = 4;
  if (std::wstring(argv[4]) == L"--expect") {
    expectDefect = std::wstring(argv[5]) == L"defect";
    exeIndex = 6;
  }
  else
    for (int i = 5; i + 1 < argc; ++i)
      if (std::wstring(argv[i]) == L"--expect") expectDefect = std::wstring(argv[i + 1]) == L"defect";

  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  const std::wstring outPath = prefix + L".log", errPath = prefix + L".err";
  HANDLE hout = CreateFileW(outPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE herr = CreateFileW(errPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hout == INVALID_HANDLE_VALUE || herr == INVALID_HANDLE_VALUE) return 3;

  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim{};
  lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  lim.ProcessMemoryLimit = capBytes;
  if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &lim, sizeof(lim))) return 4;
  HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT assoc{job, port};
  if (!port || !SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &assoc, sizeof(assoc)))
    return 5;

  std::wstring cmd;
  for (int i = exeIndex; i < argc; ++i) cmd += L"\"" + std::wstring(argv[i]) + L"\" ";
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hout;
  si.hStdError = herr;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(argv[exeIndex], cmd.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                      nullptr, nullptr, &si, &pi))
    return 7;
  if (!AssignProcessToJobObject(job, pi.hProcess)) {
    TerminateProcess(pi.hProcess, 1);
    return 8;
  }
  const auto start = std::chrono::steady_clock::now();
  ResumeThread(pi.hThread);

  std::string reason = "complete";
  auto drain = [&]() {
    DWORD message = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ov = nullptr;
    while (GetQueuedCompletionStatus(port, &message, &key, &ov, 0))
      if (message == JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT) reason = "memory_cap";
  };
  while (WaitForSingleObject(pi.hProcess, 10) == WAIT_TIMEOUT) {
    drain();
    if (reason == "complete"
        && std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >= deadline)
      reason = "timeout";
    if (reason != "complete") {
      TerminateJobObject(job, 0xE001);
      WaitForSingleObject(pi.hProcess, INFINITE);
      break;
    }
  }
  drain();
  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual{};
  QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &actual, sizeof(actual), nullptr);
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(hout);
  CloseHandle(herr);
  CloseHandle(job);
  CloseHandle(port);

  Echo(outPath);
  Echo(errPath);
  const bool capHit = reason == "memory_cap";
  std::cout << "RESULT launcher.capMiB=" << capBytes / (1024 * 1024) << "\n"
            << "RESULT launcher.capHit=" << (capHit ? 1 : 0) << "\n"
            << "RESULT launcher.reason=" << reason << "\n"
            << "RESULT launcher.exitCode=" << exitCode << "\n"
            << "RESULT launcher.peakMiB=" << actual.PeakProcessMemoryUsed / (1024. * 1024.) << "\n"
            << "RESULT launcher.seconds=" << elapsed << "\n";
  if (capHit) {
    std::cout << "VERDICT launcher " << (expectDefect ? "defect PASS" : "fixed FAIL") << "\n";
    return expectDefect ? 0 : 1;
  }
  if (reason == "timeout") {
    std::cout << "VERDICT launcher " << (expectDefect ? "defect" : "fixed") << " FAIL\n";
    return 1;
  }
  return static_cast<int>(exitCode);
}
