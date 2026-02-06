#include <stdio.h>
#include <windows.h>
#include <detours.h>
#include <strsafe.h>

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
  LPCSTR rpszDllsRaw[256];
  LPCSTR rpszDllsOut[256];
  DWORD nDlls = 0;

  for (DWORD n = 0; n < ARRAYSIZE(rpszDllsRaw); n++)
  {
    rpszDllsRaw[n] = NULL;
    rpszDllsOut[n] = NULL;
  }

  int arg = 1; for (; arg < argc && (argv[arg][0] == '-' || argv[arg][0] == '/'); arg++)
  {
    CHAR* argn = argv[arg] + 1;
    CHAR* argp = argn;
    while (*argp && *argp != ':' && *argp != '=')
      argp++;
    if (*argp == ':' || *argp == '=')
      *argp++ = '\0';

    switch (argn[0]) {
    case 'd':                                     // Set DLL Name
    case 'D':
      if (nDlls < ARRAYSIZE(rpszDllsRaw)) {
        rpszDllsRaw[nDlls++] = argp;
      }
      else {
        printf("profiler.exe: Too many DLLs.\n");
        break;
      }
      break;

    default:
      printf("profiler.exe: Bad argument: %s\n", argv[arg]);
      break;
    }
  }

  if (arg >= argc) {
      return 9001;
  }

  if (nDlls == 0) {
      return 9001;
  }

  /////////////////////////////////////////////////////////// Validate DLLs.
  for (DWORD n = 0; n < nDlls; n++)
  {
    CHAR szDllPath[1024];
    PCHAR pszFilePart = NULL;

    if (!GetFullPathNameA(rpszDllsRaw[n], ARRAYSIZE(szDllPath), szDllPath, &pszFilePart)) {
      printf("profiler.exe: Error: %s is not a valid path name..\n", rpszDllsRaw[n]);
      return 9002;
    }

    DWORD c = (DWORD)strlen(szDllPath) + 1;
    PCHAR psz = new CHAR[c];
    StringCchCopyA(psz, c, szDllPath);
    rpszDllsOut[n] = psz;

    HMODULE hDll = LoadLibraryExA(rpszDllsOut[n], NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hDll == NULL) {
      printf("profiler.exe: Error: %s failed to load (error %ld).\n", rpszDllsOut[n], GetLastError());
      return 9003;
    }

    ExportContext ec;
    ec.fHasOrdinal1 = FALSE;
    ec.nExports = 0;
    DetourEnumerateExports(hDll, &ec, ExportCallback);
    FreeLibrary(hDll);

    if (!ec.fHasOrdinal1) {
      printf("profiler.exe: Error: %s does not export ordinal #1.\n", rpszDllsOut[n]);
      printf("             See help entry DetourCreateProcessWithDllEx in Detours.chm.\n");
      return 9004;
    }
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
  for (DWORD n = 0; n < nDlls; n++) {
    printf("profiler.exe:   with `%s'\n", rpszDllsOut[n]);
  }
  fflush(stdout);

  DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

  SetLastError(0);
  SearchPathA(NULL, szExe, ".exe", ARRAYSIZE(szFullExe), szFullExe, &pszFileExe);

  if (!DetourCreateProcessWithDllsA(szFullExe[0] ? szFullExe : NULL, szCommand,
    NULL, NULL, TRUE, dwFlags, NULL, NULL,
    &si, &pi, nDlls, rpszDllsOut, NULL))
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

  for (DWORD n = 0; n < nDlls; n++) {
    if (rpszDllsOut[n] != NULL) {
      delete[] rpszDllsOut[n];
      rpszDllsOut[n] = NULL;
    }
  }

  return dwResult;
}