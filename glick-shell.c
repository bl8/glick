#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *
get_shell (void)
{
  struct passwd *passwd;
  char *shell;
  
  passwd = getpwent ();
  if (passwd != NULL && passwd->pw_shell != NULL) {
    return passwd->pw_shell;
  }
  
  shell = getenv ("SHELL");
  if (shell)
    return shell;

  shell = getusershell ();
  if (shell)
    return shell;

  return "/bin/sh";
}

int
main (int argc, char *argv[])
{
  struct stat buf;
  char *dir;
  char *program;
  char **real_argv;
  int i, j;
  int res;
  int dir_fd;

  if (argc < 2) {
    fprintf (stderr, "Usage: glick-shell {directory} [program [args]]\n");
    return 1;
  }

  dir = argv[1];

  res = stat (dir, &buf);
  if (res != 0 || !S_ISDIR (buf.st_mode)) {
    fprintf (stderr, "Error: Specified location not a directory.\n");
    return 1;
  }

  if (argc >= 3)
    program = argv[2];
  else
    program = get_shell ();

  real_argv = malloc (sizeof (char *) * (argc + 1));
  i = 0;
  real_argv[i++] = program;
  
  for (j = 3; j < argc; j++) {
    real_argv[i++] = argv[j];
  }
  real_argv[i] = NULL;


  dir_fd = open (dir, O_RDONLY);
  if (dir_fd == -1) {
    perror ("Error opening directory: ");
    return 1;
  }
    
  res = dup2 (dir_fd, 1023);
  if (res == -1) {
    perror ("dup2 error: ");
    return 1;
  }
  close (dir_fd);

  putenv("GLICKROOT=/proc/self/fd/1023");

  execvp (program, real_argv);
  
  /* Error if we continue here */
  perror ("Error starting program");
  return 1;
}
