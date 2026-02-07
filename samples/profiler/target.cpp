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
  int n = 10;
  if (argc > 1)
    n = atoi(argv[1]);

  for (int i = 0; i < n; i++)
  {
    foo();
  }
  return 0;
}