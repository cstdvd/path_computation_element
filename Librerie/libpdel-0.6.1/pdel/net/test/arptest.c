
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <err.h>

#include <pdel/net/if_util.h>

int
main(int ac, char **av)
{
	struct ether_addr *ea;
	const u_char *ether = NULL;
	struct in_addr ip;
	int publish = 0;
	int temp = 0;
	int ch;

	while ((ch = getopt(ac, av, "tp")) != -1) {
		switch (ch) {
		case 't':
			temp = 1;
			break;
		case 'p':
			publish = 1;
			break;
		default:
			goto usage;
		}
	}
	ac -= optind;
	av += optind;

	/* set command */
	if ((ac == 2 || ac == 3) && strcmp(av[0], "set") == 0) {
		if (ac == 3) {
			if ((ea = ether_aton(av[2])) == NULL)
				goto usage;
			ether = ea->octet;
		}
		if (!inet_aton(av[1], &ip))
			goto usage;
		if (if_set_arp(ip, ether, temp, publish) == -1)
			err(1, "if_set_arp");
		return (0);
	}

	/* flush command */
	if (ac == 1 && strcmp(av[0], "flush") == 0) {
		if (if_flush_arp() == -1)
			err(1, "if_flush_arp");
		return (0);
	}

	/* get command */
	if (ac == 2 && strcmp(av[0], "get") == 0) {
		struct ether_addr mac;

		if (!inet_aton(av[1], &ip))
			goto usage;
		if (if_get_arp(ip, (u_char *)&mac) == -1)
			err(1, "%s", inet_ntoa(ip));
		printf("%s is at %s\n", inet_ntoa(ip), ether_ntoa(&mac));
		return (0);
	}

usage:
	/* Unknown usage */
	(void)fprintf(stderr, "Usage: arptest"
	    " < [-tp] set ip [ether] | flush | get ip >\n");
	exit(1);
}

