#include <stdio.h>
#include <time.h>
#include <sstream>
#include <windows.h>
#include <detours.h>

#include <DbgHelp.h>
#pragma comment (lib, "Dbghelp.lib")

//////////////////////////////////////////////////////////////////////////

struct PipeScope
{
  ~PipeScope()
  {
    close();
  }

  void close()
  {
    if (hPipe && hPipe != INVALID_HANDLE_VALUE)
    {
      CloseHandle(hPipe);
      hPipe = INVALID_HANDLE_VALUE;
      printf("profiler_payload.exe: Pipe closed.\n");
      fflush(stdout);
    }
  }

  HANDLE& hPipe;
};

static HANDLE ghPipe;

static PipeScope gPipeScope { ghPipe };

//////////////////////////////////////////////////////////////////////////

static PVOID GetFunctionAddress(HANDLE hProcess, PCSTR Name)
{
  SYMBOL_INFO syminfo = { 0 };
  syminfo.SizeOfStruct = sizeof(syminfo);
  SymFromName(hProcess, Name, &syminfo);
  return (PVOID)syminfo.Address;
}

static LONG HookFunction(PVOID& pTarget, PVOID pDetour)
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)pTarget, pDetour);
  return DetourTransactionCommit();
}

static LONG UnhookFunction(PVOID& pTarget, PVOID pDetour)
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourDetach(&(PVOID&)pTarget, pDetour);
  return DetourTransactionCommit();
}

//////////////////////////////////////////////////////////////////////////

static void (*original_foo)(void) {};

static void detoured_foo()
{
  auto begin = clock();
  original_foo();
  auto end = clock();

  double secs = double(end - begin) / CLOCKS_PER_SEC;
  //printf("foo finished %lf\n", secs);
  //fflush(stdout);

  if (ghPipe > 0)
  {
    std::stringstream ss;
    ss << "foo finished " << secs << "\n";
    BOOL res = WriteFile(
      ghPipe,
      ss.str().c_str(),
      (DWORD)ss.str().size(),
      NULL, NULL);
  }
}

#include "defA.h" // class def is necessary

static void (A::*original_Afo)(void) {};

static void detoured_Afo(A *_this)
{
  auto begin = clock();
  (_this->*original_Afo)();
  auto end = clock();

  double secs = double(end - begin) / CLOCKS_PER_SEC;
  //printf("A::foo finished %lf\n", secs);
  //fflush(stdout);

  if (ghPipe > 0)
  {
    std::stringstream ss;
    ss << "A::foo finished " << secs << "\n";
    BOOL res = WriteFile(
      ghPipe,
      ss.str().c_str(),
      (DWORD)ss.str().size(),
      NULL, NULL);
  }
}

//////////////////////////////////////////////////////////////////////////

struct FunctionInfo
{
  PVOID &target;
  PVOID payload;
  LPCSTR name;
};

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  (void)hinst;
  (void)reserved;

  FunctionInfo funcInfos[]
  {
    { (PVOID&)original_foo, (PVOID)detoured_foo, "foo" },
    { (PVOID&)original_Afo, (PVOID)detoured_Afo, "A::foo" },
  };

  if (DetourIsHelperProcess()) return TRUE;

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    DetourRestoreAfterWith();
    printf("profiler_payload.dll: Loaded.\n"); fflush(stdout);

    HANDLE hProcess = GetCurrentProcess();
    SymInitialize(hProcess, NULL, true);

    for (auto &fi : funcInfos)
    {
      fi.target = GetFunctionAddress(hProcess, fi.name);
      if (!fi.target) printf("profiler_payload.dll: Error finding address of %s\n", fi.name);

      LONG error = HookFunction(fi.target, fi.payload);
      if (error) printf("profiler_payload.dll: Error hooking %s: %ld\n", fi.name, error);
    }

    ghPipe = CreateFileA(
      "\\\\.\\pipe\\profiler_pipe",
      GENERIC_WRITE,
      0,
      NULL,
      OPEN_EXISTING,
      0,
      NULL);
    if (ghPipe == NULL || ghPipe == INVALID_HANDLE_VALUE)
      printf("profiler_payload.dll: Error opening pipe: %d\n", GetLastError());
    else
      printf("profiler_payload.dll: Pipe opened: %p.\n", ghPipe);
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    for (auto &fi : funcInfos)
    {
      LONG error = UnhookFunction(fi.target, fi.payload);
      if (error) printf("profiler_payload.dll: Error unhooking %s: %ld\n", fi.name, error);
    }

    printf("profiler_payload.dll: Unloaded\n"); fflush(stdout);
  }
  return TRUE;
}