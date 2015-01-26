
#define _XOPEN_SOURCE	600
#define _GNU_SOURCE	1
#define _BSD_SOURCE	1
#define _ISOC99_SOURCE	1

#include <stdio.h>

#ifndef __MINGW32__
#define snprintf _snprintf
#define read _read
#define open _open
#define close _close
#define popen  _popen
#define pclose _pclose
#endif

static const char *
lws_signame_f(int x)
{
  static char	signame[10];
  snprintf(signame, sizeof(signame), "%d", x);
  return(signame);
}
#define lws_signame(x)	lws_signame_f(x)

/* Simple basic version of this struct */
struct passwd {
	char	*pw_name;		/* user name */
	char	*pw_passwd;		/* encrypted password */
	int	pw_uid;			/* user uid */
	int	pw_gid;			/* user gid */
	char	*pw_gecos;		/* Honeywell login info */
	char	*pw_dir;		/* home directory */
	char	*pw_shell;		/* default shell */
};

#define __dead2

