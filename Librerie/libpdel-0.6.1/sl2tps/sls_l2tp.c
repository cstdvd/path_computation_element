
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"

/*
 * L2TP server callbacks.
 */
static ppp_l2tp_server_admit_t		sls_l2tp_admit;
static ppp_l2tp_server_destroy_t	sls_l2tp_destroy;

int
sls_l2tp_start(struct ppp_engine *engine)
{
	struct ppp_l2tp_server_info l2tp_info;
	int port;

	/* Set port if not default */
	port = (sls_curconf->bind_port == 0) ?
	    L2TP_PORT : sls_curconf->bind_port;

	/* Start L2TP server */
	memset(&l2tp_info, 0, sizeof(l2tp_info));
	l2tp_info.arg = engine;
	l2tp_info.vendor = "sl2tp2";
	l2tp_info.admit = sls_l2tp_admit;
	l2tp_info.natmap = NULL;
	l2tp_info.destroy = sls_l2tp_destroy;
	if (ppp_l2tp_server_start(engine,
	    &l2tp_info, sls_curconf->bind_ip, port, 0) == -1) {
		alog(LOG_ERR, "can't start L2TP server");
		return -1;
	}

	/* Done */
	return 0;
}

void
sls_l2tp_stop(struct ppp_engine *engine)
{
	ppp_l2tp_server_stop(engine);
}

static void *
sls_l2tp_admit(void *arg, struct ppp_l2tp_peer *peer, struct in_addr ip,
	u_int16_t port, struct ppp_auth_config *auth, char *name, size_t nsize)
{
	*auth = sls_auth_config;
	snprintf(name, nsize, "[%s:%u]", inet_ntoa(ip), port);
	return ((void *)1);
}

static void
sls_l2tp_destroy(void *arg, void *carg)
{
}

