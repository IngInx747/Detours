#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "defA.h"

#pragma optimize( "", off )
__declspec(noinline)
void foo()
{
  Sleep(1000);
  printf("foo\n");
  fflush(stdout);
}
#pragma optimize( "", on )

#pragma optimize( "", off )
__declspec(noinline)
void A::foo()
{
  Sleep(1000);
  printf("A::foo\n");
  fflush(stdout);
}
#pragma optimize( "", on )

int main(int argc, char** argv)
{
  int n = 10;
  if (argc > 1)
    n = atoi(argv[1]);

  A a;

  for (int i = 0; i < n; i++)
  {
    foo();
    a.foo();
  }
  return 0;
}