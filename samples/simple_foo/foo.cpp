#include "foo.h"
#include <stdio.h>

__declspec(noinline) void foo()
{
    printf("foo\n");
    fflush(stdout);
}