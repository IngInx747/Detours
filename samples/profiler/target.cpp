#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

__declspec(noinline) void foo()
{
  Sleep(1000);
  printf("foo\n");
  fflush(stdout);
}

int main(int argc, char** argv)
{
  while (true)
  {
    foo();
  }
  return 0;
}