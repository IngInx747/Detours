#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "foo.h"

int __cdecl main(int argc, char ** argv)
{
    while (true)
    {
        foo();
        Sleep(1000);
    }
    return 0;
}