#include <stdio.h>
#include <windows.h>
#include <detours.h>
#include <strsafe.h>
#include <vector>
#include <string>

struct ExportContext
{
  BOOL fHasOrdinal1;
  ULONG nExports;
};

static BOOL CALLBACK ExportCallback(
  _In_opt_ PVOID pContext,
  _In_ ULONG nOrdinal,
  _In_opt_ LPCSTR pszSymbol,
  _In_opt_ PVOID pbTarget)
{
  (void)pContext;
  (void)pbTarget;
  (void)pszSymbol;

  ExportContext* pec = (ExportContext*)pContext;

  if (nOrdinal == 1) {
      pec->fHasOrdinal1 = TRUE;
  }
  pec->nExports++;

  return TRUE;
}

struct HandleScope
{
  ~HandleScope() { close(); }

  void close()
  {
    if (h_ && h_ != INVALID_HANDLE_VALUE)
    {
      CloseHandle(h_);
      h_ = INVALID_HANDLE_VALUE;
      printf("[Profiler]: Pipe closed.\n");
      fflush(stdout);
    }
  }

  HANDLE h_ = INVALID_HANDLE_VALUE;
};

static HANDLE &the_pipe()
{
  static HandleScope scope__ {};
  return scope__.h_;
}

int main(int argc, char** argv)
{
  std::vector<LPCSTR> rpszDllsRaw {};

  int arg = 1; for (; arg < argc && (argv[arg][0] == '-' || argv[arg][0] == '/'); arg++)
  {
    CHAR* argn = argv[arg] + 1;
    CHAR* argp = argn;
    while (*argp && *argp != ':' && *argp != '=')
      argp++;
    if (*argp == ':' || *argp == '=')
      *argp++ = '\0';

    switch (argn[0]) {
    case 'd': // Set DLL Name
    case 'D':
      rpszDllsRaw.push_back(argp);
      break;

    default:
      printf("[Profiler]: Bad argument: %s\n", argv[arg]);
      break;
    }
  }

  if (arg >= argc) {
    return 9001;
  }

  if (rpszDllsRaw.empty()) {
    return 9001;
  }

  /////////////////////////////////////////////////////////// Validate DLLs.

  std::vector<std::string> rpszDllsOut {};

  for (const auto &rpszDllRaw : rpszDllsRaw)
  {
    CHAR buf[1024];

    if (!GetFullPathNameA(rpszDllRaw, ARRAYSIZE(buf), buf, NULL))
    {
      printf("[Profiler]: Error: %s is not a valid path name..\n", rpszDllRaw);
      return 9002;
    }

    rpszDllsOut.emplace_back(buf);
    const auto &dllPath = rpszDllsOut.back();
    HMODULE hDll = LoadLibraryExA(dllPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

    if (hDll == NULL)
    {
      printf("[Profiler]: Error: %s failed to load (error %ld).\n", dllPath.c_str(), GetLastError());
      return 9003;
    }

    ExportContext ec { FALSE, 0 };
    DetourEnumerateExports(hDll, &ec, ExportCallback);
    FreeLibrary(hDll);

    if (!ec.fHasOrdinal1)
    {
      printf("[Profiler]: Error: %s does not export ordinal #1.\n", dllPath.c_str());
      printf("              See help entry DetourCreateProcessWithDllEx in Detours.chm.\n");
      return 9004;
    }
  }

  DWORD nDlls {};
  LPCSTR rpszDllsRef[1024];
  for (const auto &rpszDll : rpszDllsOut)
    rpszDllsRef[nDlls++] = rpszDll.c_str();

  //////////////////////////////////////////////////////////////////////////
  std::string exePath(argv[arg]);
  std::string command {};

  for (; arg < argc; arg++)
  {
    if (strchr(argv[arg], ' ') || strchr(argv[arg], '\t'))
    {
      command.append("\"");
      command.append(argv[arg]);
      command.append("\"");
    }
    else
      command.append(argv[arg]);

    if (arg + 1 < argc)
      command.append(" ");
  }

  printf("[Profiler]: Starting: `%s`\n", command.c_str());
  for (const auto &rpszDll : rpszDllsOut)
    printf("[Profiler]: with `%s`\n", rpszDll.c_str());
  fflush(stdout);

  //////////////////////////////////////////////////////////////////////////
  HANDLE &pipe = the_pipe();

  pipe = CreateNamedPipeA(
    "\\\\.\\pipe\\profiler_pipe",
    PIPE_ACCESS_INBOUND,
    PIPE_TYPE_BYTE | PIPE_WAIT,
    1,
    0,
    0,
    0,
    NULL);
  
  if (!pipe || pipe == INVALID_HANDLE_VALUE)
  {
    printf("[Profiler]: CreateNamedPipe failed: %ld\n", GetLastError());
    return 9005;
  }
  else printf("[Profiler]: Pipe opened: %p\n", pipe);

  //////////////////////////////////////////////////////////////////////////
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  SetLastError(0);

  CHAR buf[1024] = "\0";
  SearchPathA(NULL, exePath.c_str(), ".exe", ARRAYSIZE(buf), buf, NULL);
  exePath.clear();
  exePath.append(buf);

  DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

  if (!DetourCreateProcessWithDllsA(
    exePath.c_str(),
    const_cast<LPSTR>(command.c_str()),
    NULL,
    NULL,
    TRUE,
    dwFlags,
    NULL,
    NULL,
    &si,
    &pi,
    nDlls,
    rpszDllsRef,
    NULL))
  {
    printf("[Profiler]: DetourCreateProcessWithDllEx failed: %ld\n", GetLastError());
    ExitProcess(9009);
  }

  // Wait for the child process starts running..
  ResumeThread(pi.hThread);

  // Blocks until the child process connects to the pipe.
  ConnectNamedPipe(pipe, NULL);

  //////////////////////////////////////////////////////////////////////////
  while (true)
  {
    char buf[1024]{};
    DWORD dwRead = 0;

    // Blocks until the child process writes to the pipe or closes the pipe.
    BOOL res = ReadFile(
      pipe,
      buf,
      sizeof(buf) - 1,
      &dwRead,
      NULL);
  
    if (!res)
    {
      DWORD err = GetLastError();
      if (err == ERROR_BROKEN_PIPE)
        break;
    }
    else if (dwRead > 0)
    {
      buf[dwRead] = '\0';
      printf("[Profiler]: %s", buf);
      fflush(stdout);
    }
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD dwResult = 0; if (!GetExitCodeProcess(pi.hProcess, &dwResult))
  {
    printf("[Profiler]: GetExitCodeProcess failed: %ld\n", GetLastError());
    return 9010;
  }

  return dwResult;
}