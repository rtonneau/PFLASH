#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <chrono>
int wmain(int argc,wchar_t** argv){
 if(argc<5){std::cerr<<"launcher memoryMiB seconds prefix exe [args...]\n";return 2;}
 std::wstring prefix=argv[3],stdoutPath=prefix+L".log",stderrPath=prefix+L".err",metaPath=prefix+L".meta",modulesPath=prefix+L".modules";
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE hout=CreateFileW(stdoutPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);HANDLE herr=CreateFileW(stderrPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(hout==INVALID_HANDLE_VALUE||herr==INVALID_HANDLE_VALUE)return 3;
 HANDLE job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim{};lim.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;lim.ProcessMemoryLimit=std::stoull(argv[1])*1024*1024;if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&lim,sizeof(lim)))return 4;
 HANDLE port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,nullptr,0,1);JOBOBJECT_ASSOCIATE_COMPLETION_PORT assoc{job,port};if(!port||!SetInformationJobObject(job,JobObjectAssociateCompletionPortInformation,&assoc,sizeof(assoc)))return 5;
 MEMORYSTATUSEX mem{sizeof(mem)};GlobalMemoryStatusEx(&mem);if(mem.ullAvailPhys<3ull*1024*1024*1024){std::ofstream out(metaPath);out<<"reason system_floor_prelaunch\nfree_bytes "<<mem.ullAvailPhys<<"\n";return 6;}
 std::wstring cmd;for(int i=4;i<argc;++i){cmd+=L'"';cmd+=argv[i];cmd+=L'"';cmd+=L' ';}STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=hout;si.hStdError=herr;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(argv[4],cmd.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 7;
 if(!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,1);return 8;}auto start=std::chrono::steady_clock::now();ResumeThread(pi.hThread);std::set<std::wstring> paths;std::string reason="complete";bool cap=false;SIZE_T samplePeak=0;ULONGLONG minFree=mem.ullAvailPhys;
 while(WaitForSingleObject(pi.hProcess,10)==WAIT_TIMEOUT){PROCESS_MEMORY_COUNTERS pm{};pm.cb=sizeof(pm);if(GetProcessMemoryInfo(pi.hProcess,&pm,sizeof(pm)))samplePeak=max(samplePeak,pm.WorkingSetSize);HMODULE mods[2048];DWORD bytes=0;if(EnumProcessModules(pi.hProcess,mods,sizeof(mods),&bytes))for(DWORD i=0;i<bytes/sizeof(HMODULE);++i){wchar_t p[32768];if(GetModuleFileNameExW(pi.hProcess,mods[i],p,32768))paths.insert(p);}
  DWORD message=0;ULONG_PTR key=0;LPOVERLAPPED ov=nullptr;while(GetQueuedCompletionStatus(port,&message,&key,&ov,0)){if(message==JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT){cap=true;reason="job_memory_limit";TerminateJobObject(job,0xE001);}}
  GlobalMemoryStatusEx(&mem);minFree=min(minFree,mem.ullAvailPhys);if(mem.ullAvailPhys<3ull*1024*1024*1024){reason="system_floor";TerminateJobObject(job,0xE002);}if(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()>=std::stod(argv[2])){reason="timeout";TerminateJobObject(job,0xE003);}if(reason!="complete"){WaitForSingleObject(pi.hProcess,INFINITE);break;}
 }
 DWORD message=0;ULONG_PTR key=0;LPOVERLAPPED ov=nullptr;while(GetQueuedCompletionStatus(port,&message,&key,&ov,0)){if(message==JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT){cap=true;reason="job_memory_limit";}}
 DWORD exitCode=0;GetExitCodeProcess(pi.hProcess,&exitCode);JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual{};QueryInformationJobObject(job,JobObjectExtendedLimitInformation,&actual,sizeof(actual),nullptr);std::ofstream out(metaPath);out<<"reason "<<reason<<"\nexit_code "<<exitCode<<"\ncap_notification "<<cap<<"\nsampled_peak_bytes "<<samplePeak<<"\njob_peak_process_memory_bytes "<<actual.PeakProcessMemoryUsed<<"\nmin_free_bytes "<<minFree<<"\nelapsed_seconds "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";std::wofstream modout(modulesPath);for(auto &p:paths)modout<<p<<L"\n";
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);CloseHandle(port);CloseHandle(hout);CloseHandle(herr);return 0;
}
