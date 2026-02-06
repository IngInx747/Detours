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
      printf("profiler.exe: Bad argument: %s\n", argv[arg]);
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
    CHAR szDllPath[1024];
    PCHAR pszFilePart = NULL;

    if (!GetFullPathNameA(rpszDllRaw, ARRAYSIZE(szDllPath), szDllPath, &pszFilePart)) {
      printf("profiler.exe: Error: %s is not a valid path name..\n", rpszDllRaw);
      return 9002;
    }

    rpszDllsOut.emplace_back(szDllPath);
    const auto &dllPath = rpszDllsOut.back();

    HMODULE hDll = LoadLibraryExA(dllPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hDll == NULL) {
      printf("profiler.exe: Error: %s failed to load (error %ld).\n", dllPath.c_str(), GetLastError());
      return 9003;
    }

    ExportContext ec { FALSE, 0 };
    DetourEnumerateExports(hDll, &ec, ExportCallback);
    FreeLibrary(hDll);

    if (!ec.fHasOrdinal1) {
      printf("profiler.exe: Error: %s does not export ordinal #1.\n", dllPath.c_str());
      printf("             See help entry DetourCreateProcessWithDllEx in Detours.chm.\n");
      return 9004;
    }
  }

  DWORD nDlls {};
  LPCSTR rpszDllsRef[1024];
  for (const auto &rpszDll : rpszDllsOut) {
    rpszDllsRef[nDlls++] = rpszDll.c_str();
  }

  //////////////////////////////////////////////////////////////////////////
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  CHAR szCommand[2048];
  CHAR szExe[1024];
  CHAR szFullExe[1024] = "\0";
  PCHAR pszFileExe = NULL;

  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  szCommand[0] = L'\0';

  StringCchCopyA(szExe, sizeof(szExe), argv[arg]);

  for (; arg < argc; arg++)
  {
    if (strchr(argv[arg], ' ') != NULL || strchr(argv[arg], '\t') != NULL) {
      StringCchCatA(szCommand, sizeof(szCommand), "\"");
      StringCchCatA(szCommand, sizeof(szCommand), argv[arg]);
      StringCchCatA(szCommand, sizeof(szCommand), "\"");
    }
    else {
      StringCchCatA(szCommand, sizeof(szCommand), argv[arg]);
    }

    if (arg + 1 < argc) {
      StringCchCatA(szCommand, sizeof(szCommand), " ");
    }
  }

  printf("profiler.exe: Starting: `%s'\n", szCommand);
  for (const auto &rpszDll : rpszDllsOut) {
    printf("profiler.exe:   with `%s'\n", rpszDll.c_str());
  }
  fflush(stdout);

  DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

  SetLastError(0);
  SearchPathA(NULL, szExe, ".exe", ARRAYSIZE(szFullExe), szFullExe, &pszFileExe);

  if (!DetourCreateProcessWithDllsA(szFullExe[0] ? szFullExe : NULL, szCommand,
    NULL, NULL, TRUE, dwFlags, NULL, NULL,
    &si, &pi, nDlls, rpszDllsRef, NULL))
  {
    DWORD dwError = GetLastError();
    printf("profiler.exe: DetourCreateProcessWithDllEx failed: %ld\n", dwError);
    ExitProcess(9009);
  }

  ResumeThread(pi.hThread);

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD dwResult = 0;
  if (!GetExitCodeProcess(pi.hProcess, &dwResult)) {
      printf("profiler.exe: GetExitCodeProcess failed: %ld\n", GetLastError());
      return 9010;
  }

  return dwResult;
}