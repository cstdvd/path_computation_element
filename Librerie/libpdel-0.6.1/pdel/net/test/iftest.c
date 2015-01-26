
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>
#include <pdel/util/typed_mem.h>
#include <pdel/net/if_util.h>

int
main(int ac, char **av)
{
	struct in_addr *iplist;
	struct in_addr *nmlist;
	char **ifnames;
	int nif;
	int val;
	int ch;
	int i;

	while ((ch = getopt(ac, av, "")) != -1) {
		switch (ch) {
		default:
			goto usage;
		}
	}
	ac -= optind;
	av += optind;

	if ((nif = if_get_list(&ifnames, TYPED_MEM_TEMP)) == NULL)
		err(1, "if_list");
	for (i = 0; i < nif; i++) {
		printf("interface \"%s\":\n", ifnames[i]);
		printf("     type=");
		if ((val = if_get_type(ifnames[i])) != -1)
			printf("%d\n", val);
		else
			printf("%s\n", strerror(errno));
		printf("    flags=");
		if ((val = if_get_flags(ifnames[i])) != -1)
			printf("0x%x\n", val);
		else
			printf("%s\n", strerror(errno));
		printf("      mtu=");
		if ((val = if_get_mtu(ifnames[i])) != -1)
			printf("%d\n", val);
		else
			printf("%s\n", strerror(errno));
		if ((val = if_get_ip_addrs(ifnames[i],
		    &iplist, &nmlist, TYPED_MEM_TEMP)) == -1)
			printf("  ipaddrs=%s\n", strerror(errno));
		else {
			int j;

			printf("  ipaddrs=[%d]:", val);
			for (j = 0; j < val; j++) {
				printf(" %s/0x%08x", inet_ntoa(iplist[j]),
				    (u_int32_t)ntohl(nmlist[j].s_addr));
			}
			printf("\n");
			FREE(TYPED_MEM_TEMP, iplist);
			FREE(TYPED_MEM_TEMP, nmlist);
		}
	}
	if (ac > 0) {
		int flags;

		if ((flags = if_get_flags(av[0])) == -1)
			err(1, "if_get_flags(%s)", av[0]);
		if (if_set_flags(av[0], flags | IFF_UP) == -1)
			err(1, "if_set_flags(%s)", av[0]);
	}
	while (nif > 0)
		FREE(TYPED_MEM_TEMP, ifnames[--nif]);
	FREE(TYPED_MEM_TEMP, ifnames);
	return (0);

usage:
	fprintf(stderr, "usage: iftest [interface-to-mark-up]\n");
	return (1);
}

