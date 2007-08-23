#include <stdlib.h>

extern int ext2_main(int argc, char *argv[], void (*mounted) (void));

int
main (int argc, char *argv[])
{
  char *child_argv[5];

  child_argv[0] = "glick";
  child_argv[1] = argv[1];
  child_argv[2] = "-o";
  child_argv[3] = "ro";
  child_argv[4] = NULL;
  
  ext2_main (4, child_argv, NULL);
  return 0;
}
