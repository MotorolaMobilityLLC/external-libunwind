/* libunwind - a platform-independent unwind library
   Copyright (C) 2003 Hewlett-Packard Co
	Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <errno.h>
#include <fcntl.h>
#include <libunwind.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/wait.h>

int nerrors;
int verbose = 1;

#define panic(args...)					\
	do { fprintf (stderr, args); ++nerrors; } while (0)

static unw_addr_space_t as;
static struct UPT_info *ui;

void
do_backtrace (pid_t target_pid)
{
  unw_proc_info_t pi;
  unw_word_t ip, sp;
  unw_cursor_t c;
  char buf[512];
  int ret;

  ret = unw_init_remote (&c, as, ui);
  if (ret < 0)
    panic ("unw_init_remote() failed: ret=%d\n", ret);

  do
    {
      unw_get_reg (&c, UNW_REG_IP, &ip);
      unw_get_reg (&c, UNW_REG_SP, &sp);
      unw_get_proc_name (&c, buf, sizeof (buf), NULL);

      printf ("%016lx %-32s (sp=%016lx)\n", (long) ip, buf, (long) sp);

      unw_get_proc_info (&c, &pi);
      printf ("\tproc=%016lx-%016lx\n\thandler=%lx lsda=%lx",
	      (long) pi.start_ip, (long) pi.end_ip,
	      (long) pi.handler, (long) pi.lsda);

#if UNW_TARGET_IA64
      {
	unw_word_t bsp;

	unw_get_reg (&c, UNW_IA64_BSP, &bsp);
	printf (" bsp=%lx", bsp);
      }
#endif
      printf ("\n");

      ret = unw_step (&c);
      if (ret < 0)
	{
	  unw_get_reg (&c, UNW_REG_IP, &ip);
	  printf ("FAILURE: unw_step() returned %d for ip=%lx\n",
		  ret, (long) ip);
	}
    }
  while (ret > 0);

  if (ret < 0)
    panic ("unwind failed with ret=%d\n", ret);

  printf ("================\n\n");
}

int
main (int argc, char **argv)
{
  int status, pid, state = 1;
  pid_t target_pid;

  as = unw_create_addr_space (&_UPT_accessors, 0);
  if (!as)
    panic ("unw_create_addr_space() failed");

  if (argc == 1)
    {
      char *args[] = { "self", "/bin/ls", "/usr", NULL };

      /* automated test case */
      verbose = 0;
      argv = args;
    }

  target_pid = fork ();
  if (!target_pid)
    {
      /* child */

      if (!verbose)
	dup2 (open ("/dev/null", O_WRONLY), 1);

      ptrace (PTRACE_TRACEME, 0, 0, 0);
      execve (argv[1], argv + 1, environ);
      _exit (-1);
    }

  ui = _UPT_create (target_pid);

  while (1)
    {
      pid = wait4 (-1, &status,  0, 0);
      if (pid == -1)
	{
	  if (errno == EINTR)
	    continue;

	  panic ("wait4() failed (errno=%d)\n", errno);
	}
      if (WIFSIGNALED (status) || WIFEXITED (status)
	  || (WIFSTOPPED (status) && WSTOPSIG (status) != SIGTRAP))
	{
	  if (WIFEXITED (status))
	    {
	      if (WEXITSTATUS (status) != 0)
		panic ("child's exit status %d\n", WEXITSTATUS (status));
	      break;
	    }
	  else if (WIFSIGNALED (status))
	    panic ("child terminated by signal %d\n", WTERMSIG (status));
	  else
	    panic ("child got signal %d\n", WSTOPSIG (status));
	}

#if 1
      if (!state)
	do_backtrace (target_pid);

      state ^= 1;
      ptrace (PTRACE_SYSCALL, target_pid, 0, 0);		/* continue */
#else
      do_backtrace (target_pid);
      ptrace (PTRACE_SINGLESTEP, target_pid, 0, 0);		/* continue */
#endif
    }

  _UPT_destroy (ui);

  if (nerrors)
    {
      printf ("FAILURE: detected %d errors\n", nerrors);
      exit (-1);
    }
  if (verbose)
    printf ("SUCCESS\n");

  return 0;
}
