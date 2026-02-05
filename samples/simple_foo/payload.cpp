#include <stdio.h>
#include <windows.h>
#include "detours.h"
#include "foo.h"

static void (*original_foo)(void) = foo;

void WINAPI detoured_foo()
{
    original_foo();
    printf("detoured foo\n");
    fflush(stdout);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    LONG error;
    (void)hinst;
    (void)reserved;

    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();

        printf("foo_payload.dll: Starting.\n");
        fflush(stdout);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)original_foo, detoured_foo);
        error = DetourTransactionCommit();

        if (error == NO_ERROR) {
            printf("foo_payload.dll: Detoured foo().\n");
        }
        else {
            printf("foo_payload.dll: Error detouring foo(): err = %ld\n", error);
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)original_foo, detoured_foo);
        error = DetourTransactionCommit();

        printf("foo_payload.dll: Removed foo(): %ld\n", error);
        fflush(stdout);
    }
    return TRUE;
}