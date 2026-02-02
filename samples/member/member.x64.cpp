//////////////////////////////////////////////////////////////////////////////
//
//  Test a detour of a member function (member.x64.cpp of member.exe)
//
//  Microsoft Research Detours Package
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  This example shows how to detour C++ member functions under the x64 arch.
//  Unlike member functions using the __thiscall calling convention in x86,
//  the x64 ones have "this" pointer in RCX and use normal C++ signatures.
//
#include <stdio.h>
#include <windows.h>
#include <detours.h>

//////////////////////////////////////////////////////////////// Target Class.
//
class CMember
{
  public:
    __declspec(noinline) void foo();

  // Class may have any member variables or virtual functions.

  public:
    virtual void bar() {}

  private:
    int i32_member_;
};

void CMember::foo()
{
    printf("  CMember::foo! (this:%p)\n", this);
}

//////////////////////////////////////////////////////////////// Detour Class.
//
static void (CMember::* original_foo)() = &CMember::foo;

void foo_detoured(CMember* _this)
{
    printf("  foo_detoured! (this:%p)\n", _this);
    (_this->*original_foo)();
}

//////////////////////////////////////////////////////////////////////////////
//
int main(int argc, char **argv)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&(PVOID&)original_foo, foo_detoured);

    LONG err = DetourTransactionCommit();
    if (err) printf("Error detouring CMember::foo(): %d\n", err);

    //////////////////////////////////////////////////////////////////////////
    //
    CMember target;

    printf("Calling CMember (w/o Detour):\n");
    ((&target)->*original_foo)();

    printf("Calling CMember (will be detoured):\n");
    target.foo();

    return 0;
}