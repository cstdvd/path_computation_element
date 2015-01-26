
#define _XOPEN_SOURCE	600
#define _GNU_SOURCE	1
#define _BSD_SOURCE	1
#define _ISOC99_SOURCE	1

#include <stdio.h>

static inline const char *
lws_signame_f(int x)
{
  static char	signame[10];
  snprintf(signame, sizeof(signame), "%d", x);
  return(signame);
}
#define lws_signame(x)	lws_signame_f(x)

#define __dead2

