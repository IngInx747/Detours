//////////////////////////////////////////////////////////////////////////////
//
//  Test a detour of a member function (member.x64.cpp of member.exe)
//
//  Microsoft Research Detours Package
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  This DLL will detour a custom function foo() statically linked. The method
//  used in the example 'simple' doesn't work for functions in static library
//  because when the function is loaded, the EXE and DLL have their own copy
//  and what DLL hooks is not the one in the EXE. Hence, we have to look for
//  the function address explicitly at the EXE process.
//
#include <stdio.h>
#include <windows.h>
#include "detours.h"
#include "foo.h"

#include <DbgHelp.h>
#pragma comment (lib, "Dbghelp.lib")

static void *GetFunctionAddress(HANDLE hProcess, PCSTR Name)
{
    SYMBOL_INFO syminfo = { 0 };
    SymInitialize(hProcess, NULL, true);
    syminfo.SizeOfStruct = sizeof(syminfo);
    SymFromName(hProcess, Name, &syminfo);
    return (void*)syminfo.Address;
}

typedef void (*foo_functor)(void);

static void (*original_foo)(void) = NULL;

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

        printf("foo_payload.dll: Loaded.\n");
        fflush(stdout);

        original_foo = (foo_functor)GetFunctionAddress(GetCurrentProcess(), "foo");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)original_foo, detoured_foo);
        error = DetourTransactionCommit();

        if (error == NO_ERROR)
            printf("foo_payload.dll: Error detouring foo(): err = %ld\n", error);
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)original_foo, detoured_foo);
        error = DetourTransactionCommit();

        printf("foo_payload.dll: Unloaded\n");
        fflush(stdout);
    }
    return TRUE;
}