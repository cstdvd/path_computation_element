
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <fetch.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>
#include <pthread.h>
#include <err.h>
#include <netgraph.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_ksocket.h>
#include <netgraph/ng_iface.h>

#include <openssl/ssl.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>
#include <pdel/net/if_util.h>
#include <pdel/util/typed_mem.h>
#include <pdel/sys/alog.h>

#include <pdel/ppp/ppp_lib.h>
#include <pdel/ppp/ppp_log.h>
#include <pdel/ppp/ppp_auth.h>
#include <pdel/ppp/ppp_auth_chap.h>
#include <pdel/ppp/ppp_link.h>
#include <pdel/ppp/ppp_bundle.h>
#include <pdel/ppp/ppp_msoft.h>
#include <pdel/ppp/ppp_engine.h>
#include <pdel/ppp/ppp_manager.h>
#include <pdel/ppp/ppp_pptp_server.h>
#include <pdel/ppp/ppp_l2tp_server.h>

#define VENDOR_NAME	"PDEL library"

/*
 * PPP manager definition.
 *
 * The PPP manager is the "application". The library defers to the
 * manager for all "policy" decisions. The PPP engine is the thing
 * that the application gets from the library which does all the
 * PPP stuff.
 */
static ppp_manager_bundle_config_t	demo_manager_bundle_config;
static ppp_manager_bundle_plumb_t	demo_manager_bundle_plumb;
static ppp_manager_bundle_unplumb_t	demo_manager_bundle_unplumb;
static ppp_manager_release_ip_t		demo_manager_release_ip;

static struct	ppp_manager_meth demo_manager_methods = {
	demo_manager_bundle_config,
	demo_manager_bundle_plumb,
	demo_manager_bundle_unplumb,
	demo_manager_release_ip,
};

static struct	ppp_manager demo_manager = {
	&demo_manager_methods
};

/*
 * PPP authorization callbacks and configuration.
 */
static ppp_auth_acquire_t		demo_auth_acquire;
static ppp_auth_check_t			demo_auth_check;

static struct	ppp_auth_meth demo_auth_meth = {
	demo_auth_acquire,
	demo_auth_check,
};

static const	struct ppp_auth_config demo_auth_config = {
	&demo_auth_meth,
	{
		0
#if 0
		| (1 << PPP_AUTH_PAP)
#endif
		| (1 << PPP_AUTH_CHAP_MSV1)
		| (1 << PPP_AUTH_CHAP_MSV2)
		| (1 << PPP_AUTH_CHAP_MD5)
		,
		(1 << PPP_AUTH_NONE)
	}
};

/*
 * PPTP server callbacks. Used for 'pptp_info' application info structure.
 */
static ppp_pptp_server_admit_t		demo_pptp_admit;
static ppp_pptp_server_plumb_t		demo_pptp_plumb;
static ppp_pptp_server_destroy_t	demo_pptp_destroy;

/*
 * L2TP server callbacks. Used for 'l2tp_info' application info structure.
 */
static ppp_l2tp_server_admit_t		demo_l2tp_admit;
static ppp_l2tp_server_destroy_t	demo_l2tp_destroy;

/*
 * Logging callback, for when the PPP library needs to log something.
 */
static ppp_log_vput_t			demo_log_vput;

/*
 * Internal variables
 */
static struct	in_addr bind_ip;
static struct	in_addr ppp_ip[2];
static struct	in_addr dns_server;
static int	pptp_port = PPTP_PORT;
static int	l2tp_port = L2TP_PORT;

static u_char	mppe_40;			/* want 40 bit mppe */
static u_char	mppe_56;			/* want 56 bit mppe */
static u_char	mppe_128;			/* want 128 bit mppe */
static u_char	mppe_stateless;			/* mppe stateless mode */
static const	char *user = "foo";
static const	char *password = "bar";
static struct	ppp_log *log;			/* where ppp stuff logs to */
static struct	ppp_engine *engine;		/* library provided "engine" */

static const	struct in_addr fullmask = { 0xffffffff };

/*
 * Internal functions
 */
static void	usage(void);

/*
 * Demo for PPP code. This is a PPTP and L2TP server.
 */
int
main(int argc, char **argv)
{
	struct ppp_pptp_server_info pptp_info;
	struct ppp_l2tp_server_info l2tp_info;
	struct alog_config ac;
	struct ppp_log *elog;
	sigset_t sigs;
	int rtn = 1;
	int sig;
	int ch;

	/* Parse command line arguments */
	while ((ch = getopt(argc, argv, "a:dD:e:s:p:P:SU:t:u:")) != -1) {
		switch (ch) {
		case 'a':
			if (!inet_aton(optarg, &bind_ip)) {
				fprintf(stderr,
				    "invalid bind IP address \"%s\"\n",
				    optarg);
				usage();
			}
			break;
		case 'd':
			NgSetDebug(NgSetDebug(-1) + 1);
			break;
		case 'D':
			if (!inet_aton(optarg, &dns_server)) {
				fprintf(stderr,
				    "invalid DNS server IP address \"%s\"\n",
				    optarg);
				usage();
			}
			break;
		case 'e':
			switch (atoi(optarg)) {
			case 40:
				mppe_40 = 1;
				break;
			case 56:
				mppe_56 = 1;
				break;
			case 128:
				mppe_128 = 1;
				break;
			default:
				fprintf(stderr,
				    "invalid MPPE bits \"%s\"\n", optarg);
				usage();
			}
			break;
		case 'S':
			mppe_stateless = 1;
			break;
		case 's':
		case 'p':
			if (!inet_aton(optarg,
			    &ppp_ip[ch == 's' ? PPP_SELF : PPP_PEER])) {
				fprintf(stderr,
				    "invalid %s IP address \"%s\"\n",
				    ch == 's' ? "self" : "peer", optarg);
				usage();
			}
			break;
		case 't':
			pptp_port = atoi(optarg);
			break;
		case 'u':
			l2tp_port = atoi(optarg);
			break;
		case 'U':
			user = optarg;
			break;
		case 'P':
			password = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	switch (argc) {
	default:
		usage();
		break;
	case 0:
		break;
	}

	/* Enable typed memory */
	if (typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	/* Block SIGPIPE */
	(void)signal(SIGPIPE, SIG_IGN);

	/* Initialize logging */
	memset(&ac, 0, sizeof(ac));
	ac.min_severity = LOG_DEBUG;
	if (alog_configure(0, &ac) == -1) {
		warn("alog_configure");
		goto done;
	}

	/* Create PPP log */
	if ((log = ppp_log_create(NULL, demo_log_vput, NULL)) == NULL) {
		warn("ppp_log_create");
		goto done;
	}

	/* Create new PPP engine */
	elog = ppp_log_dup(log);
	if ((engine = ppp_engine_create(&demo_manager, NULL, elog)) == NULL) {
		warn("ppp_engine_create");
		ppp_log_close(&elog);
		ppp_log_close(&log);
		goto done;
	}

	/* Start PPTP server */
	memset(&pptp_info, 0, sizeof(pptp_info));
	pptp_info.arg = engine;
	pptp_info.vendor = VENDOR_NAME;
	pptp_info.admit = demo_pptp_admit;
	pptp_info.plumb = demo_pptp_plumb;
	pptp_info.destroy = demo_pptp_destroy;
	if (ppp_pptp_server_start(engine,
	    &pptp_info, bind_ip, pptp_port, 0) == -1) {
		warn("ppp_pptp_server_start");
		ppp_engine_destroy(&engine, 1);
		goto done;
	}

	/* Start L2TP server */
	memset(&l2tp_info, 0, sizeof(l2tp_info));
	l2tp_info.arg = engine;
	l2tp_info.vendor = VENDOR_NAME;
	l2tp_info.admit = demo_l2tp_admit;
	l2tp_info.natmap = NULL;
	l2tp_info.destroy = demo_l2tp_destroy;
	if (ppp_l2tp_server_start(engine,
	    &l2tp_info, bind_ip, l2tp_port, 0) == -1) {
		warn("ppp_l2tp_server_start");
		ppp_engine_destroy(&engine, 1);
		goto done;
	}

	/* Wait for interrupt */
	alog(LOG_INFO, "waiting for connections...");
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1) {
		warn("sigprocmask");
		goto done;
	}
	if (sigwait(&sigs, &sig) == -1) {
		warn("sigwait");
		goto done;
	}

	/* Shut down server */
	printf("\nRec'd signal %s, shutting down...\n", sys_signame[sig]);

done:
	ppp_engine_destroy(&engine, 1);
	ppp_log_close(&log);
	typed_mem_dump(stdout);
	return (rtn);
}

/*
 * Exit after printing usage string
 */
static void
usage(void)
{
	(void)fprintf(stderr, "Usage: ppp_demo [options...]\n");
	(void)fprintf(stderr, "\t-d\t\t\tIncrease netgraph debugging level\n");
	(void)fprintf(stderr, "\t-a ipaddr\t\tIP address to listen on\n");
	(void)fprintf(stderr, "\t-D ipaddr\t\tPeer's DNS server IP address\n");
	(void)fprintf(stderr, "\t-e < 40 | 56 | 128 >\tEnable MPPE\n");
	(void)fprintf(stderr, "\t-S\t\t\tEnable MPPE stateless mode\n");
	(void)fprintf(stderr, "\t-s ipaddr\t\tSpecify self's inside IP\n");
	(void)fprintf(stderr, "\t-p ipaddr\t\tSpecify peer's inside IP\n");
	(void)fprintf(stderr, "\t-U username\t\tSpecify username\n");
	(void)fprintf(stderr, "\t-P password\t\tSpecify password\n");
	(void)fprintf(stderr, "\t-t port\t\t\tSpecify PPTP listen port\n");
	(void)fprintf(stderr, "\t-u port\t\t\tSpecify L2TP listen port\n");
	exit(1);
}

/***********************************************************************
			MANAGER METHODS
***********************************************************************/

static int	ip_acquired;

static void *
demo_manager_bundle_config(struct ppp_manager *manager,
	struct ppp_link *link, struct ppp_bundle_config *conf)
{
	printf("[MANAGER] new link %p, peer=\"%s\"\n",
	    link, ppp_link_get_authname(link, PPP_PEER));
	if (ip_acquired) {
		errno = EALREADY;
		return (NULL);
	}
	memset(conf, 0, sizeof(*conf));
	conf->ip[PPP_SELF] = ppp_ip[PPP_SELF];
	conf->ip[PPP_PEER] = ppp_ip[PPP_PEER];
	ip_acquired = 1;
	conf->dns_servers[0] = dns_server;
	conf->vjc = 1;
	conf->mppe_40 = mppe_40;
	conf->mppe_56 = mppe_56;
	conf->mppe_128 = mppe_128;
	return ((void *)1);
}

static void *
demo_manager_bundle_plumb(struct ppp_manager *manager,
	struct ppp_bundle *bundle, const char *path, const char *hook,
	struct in_addr *ips, struct in_addr *dns, struct in_addr *nbns,
	u_int mtu)
{
	union {
	    u_char buf[sizeof(struct ng_mesg)
	      + sizeof(struct ng_iface_ifname)];
	    struct ng_mesg reply;
	} repbuf;
	struct ng_mesg *const reply = &repbuf.reply;
	struct ng_iface_ifname *const ifname
	    = (struct ng_iface_ifname *)reply->data;
	char ifpath[64];
	char ipbuf[16];
	int csock = -1;
	char *rtn;
	int esave;

	/* Debug */
	strlcpy(ipbuf, inet_ntoa(ips[PPP_SELF]), sizeof(ipbuf));
	printf("[MANAGER] plumbing top side of bundle %p %s -> %s MTU=%u\n",
	    bundle, ipbuf, inet_ntoa(ips[PPP_PEER]), mtu);

	/* Get temporary socket node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		warn("NgMkSockNode");
		goto fail;
	}
	snprintf(ifpath, sizeof(ifpath), "%s%s", path, hook);

	/* Attach iface node */
	if (NgSendAsciiMsg(csock, path, "mkpeer { type=\"%s\""
	    " ourhook=\"%s\" peerhook=\"%s\" }", NG_IFACE_NODE_TYPE,
	    hook, NG_IFACE_HOOK_INET) == -1) {
		warn("mkpeer");
		goto fail;
	}

	/* Get node name */
	if (NgSendMsg(csock, ifpath, NGM_IFACE_COOKIE, NGM_IFACE_GET_IFNAME,
	    NULL, 0) == -1) {
		warn("NgSendMsg");
		goto fail;
	}
	if (NgRecvMsg(csock, reply, sizeof(repbuf), NULL) == -1) {
		warn("NgRecvMsg");
		goto fail;
	}

	/* Configure iface node */
	if (if_add_ip_addr(ifname->ngif_name, ips[PPP_SELF],
	    fullmask, ips[PPP_PEER]) == -1) {
		warn("if_add_ip(%s)", inet_ntoa(ips[PPP_SELF]));
		goto fail;
	}
	if (if_set_mtu(ifname->ngif_name, mtu) == -1) {
		warn("if_setmtu(%d)", mtu);
		goto fail;
	}

	/* Get return value */
	ASPRINTF("ppp_demo.ifname", &rtn, "%s:", ifname->ngif_name);
	if (rtn == NULL) {
		warn("asprintf");
		goto fail;
	}

	/* Done */
	(void)close(csock);
	printf("[MANAGER] plumbing top side of bundle %p OK\n", bundle);
	return (rtn);

fail:
	/* Clean up after failure */
	esave = errno;
	printf("[MANAGER] plumbing top side of bundle %p failed: %s\n",
	    bundle, strerror(esave));
	if (csock != -1) {
		(void)NgSendMsg(csock, ifpath, NGM_GENERIC_COOKIE,
		    NGM_SHUTDOWN, NULL, 0);
		(void)close(csock);
	}
	errno = esave;
	return (NULL);
}

static void
demo_manager_release_ip(struct ppp_manager *manager,
	struct ppp_bundle *bundle, struct in_addr ip)
{
	printf("[MANAGER] releasing IP address for bundle %p\n", bundle);
	ip_acquired = 0;
}

static void
demo_manager_bundle_unplumb(struct ppp_manager *manager, void *arg,
	struct ppp_bundle *bundle)
{
	char *const ifpath = arg;
	int csock;

	printf("[MANAGER] unplumb bundle %p (%s)\n", bundle, ifpath);

	/* Get temporary socket node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		warn("NgMkSockNode");
		return;
	}

	/* Kill iface node */
	if (NgSendMsg(csock, ifpath,
	    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0) == -1)
		warn("shutdown(%s)", ifpath);

	/* Free node name */
	FREE("ppp_demo.ifname", ifpath);
	(void)close(csock);
}

/***********************************************************************
			PPTP SERVER CALLBACKS
***********************************************************************/

static struct		in_addr peer_ip;
static u_int16_t	peer_port;

static void *
demo_pptp_admit(void *arg, struct ppp_pptp_peer *peer,
	struct in_addr ip, u_int16_t port,
	struct ppp_auth_config *auth, char *name, size_t nsize)
{
	if (peer_ip.s_addr != 0)
		return (NULL);
	peer_ip = ip;
	peer_port = port;
	*auth = demo_auth_config;
	printf("[DEMO] PPTP connection from %s:%u\n", inet_ntoa(ip), port);
	snprintf(name, nsize, "[%s:%u]", inet_ntoa(ip), port);
	return ((void *)1);
}

static int
demo_pptp_plumb(void *arg, void *carg, const char *path,
	const char *hook, const struct in_addr *ips)
{
	struct sockaddr_in laddr;
	struct sockaddr_in paddr;
	char kpath[64];
	int csock = -1;
	int esave;

	strlcpy(kpath, inet_ntoa(ips[PPP_SELF]), sizeof(kpath));
	printf("[DEMO] plumbing GRE %s -> %s\n",
	    kpath, inet_ntoa(ips[PPP_PEER]));

	/* Get temporary socket node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		warn("NgMkSockNode");
		goto fail;
	}
	snprintf(kpath, sizeof(kpath), "%s%s", path, hook);

	/* Attach ksocket node to pptpgre node */
	if (NgSendAsciiMsg(csock, path, "mkpeer { type=\"%s\" ourhook=\"%s\""
	    " peerhook=\"%s\" }", NG_KSOCKET_NODE_TYPE, hook,
	    "inet/raw/gre") == -1) {
		warn("mkpeer");
		goto fail;
	}

	/* Bind(2) ksocket node */
	memset(&laddr, 0, sizeof(laddr));
	laddr.sin_len = sizeof(laddr);
	laddr.sin_family = AF_INET;
	laddr.sin_addr = bind_ip;
	if (NgSendMsg(csock, kpath, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_BIND, &laddr, sizeof(laddr)) == -1) {
		warn("bind");
		goto fail;
	}

	/* Connect(2) ksocket node to peer */
	memset(&paddr, 0, sizeof(paddr));
	paddr.sin_len = sizeof(paddr);
	paddr.sin_family = AF_INET;
	paddr.sin_addr = peer_ip;
	if (NgSendMsg(csock, kpath, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_CONNECT, &paddr, sizeof(paddr)) == -1) {
		warn("connect");
		goto fail;
	}

	/* Done */
	(void)close(csock);
	return (0);

fail:
	/* Clean up after failure */
	esave = errno;
	if (csock != -1) {
		(void)NgSendMsg(csock, kpath, NGM_GENERIC_COOKIE,
		    NGM_SHUTDOWN, NULL, 0);
		(void)close(csock);
	}
	errno = esave;
	return (-1);
}

static void
demo_pptp_destroy(void *arg, void *carg, const char *path)
{
	printf("[DEMO] closing GRE\n");
	peer_ip.s_addr = 0;
	peer_port = 0;
}

/***********************************************************************
			L2TP SERVER CALLBACKS
***********************************************************************/

static void *
demo_l2tp_admit(void *arg, struct ppp_l2tp_peer *peer, struct in_addr ip,
	u_int16_t port, struct ppp_auth_config *auth, char *name, size_t nsize)
{
	if (peer_ip.s_addr != 0)
		return (NULL);
	peer_ip = ip;
	peer_port = port;
	*auth = demo_auth_config;
	printf("[DEMO] L2TP connection from %s:%u\n", inet_ntoa(ip), port);
	snprintf(name, nsize, "[%s:%u]", inet_ntoa(ip), port);
	return ((void *)1);
}

static void
demo_l2tp_destroy(void *arg, void *carg)
{
	printf("[DEMO] closing L2TP\n");
	peer_ip.s_addr = 0;
	peer_port = 0;
}

/***********************************************************************
		    AUTHORIZATION CALLBACKS
***********************************************************************/

static int
demo_auth_acquire(struct ppp_link *link,
	struct ppp_auth_cred *creds, struct ppp_auth_resp *resp)
{
	printf("[DEMO] auth acquire, sleeping 1 second...\n");
	sleep(1);
	switch (creds->type) {
	default:
		errno = EPROTONOSUPPORT;
		return (-1);
	}
}

static int
demo_auth_check(struct ppp_link *link,
	const struct ppp_auth_cred *creds, struct ppp_auth_resp *resp)
{
	int i;

	printf("[DEMO] auth check, sleeping 1 second...\n");
	sleep(1);
	switch (creds->type) {
	case PPP_AUTH_PAP:
	    {
		const struct ppp_auth_cred_pap *const pap = &creds->u.pap;

		printf("[DEMO] PAP auth check user=\"%s\"\n", pap->name);
		if (strcmp(pap->name, user) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "wrong username");
			return (-1);
		}
		if (strcmp(pap->password, password) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "wrong password");
			return (-1);
		}
		return (0);
	    }
	case PPP_AUTH_CHAP_MSV1:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		const struct ppp_auth_cred_chap_msv1 *const rsp = &chap->u.msv1;
		u_char buf[PPP_MSOFT_NT_HASH_LEN];

		printf("[DEMO] MSv1 auth check user=\"%s\"\n", chap->name);

		/* Check response */
		if (strcmp(chap->name, user) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "wrong username");
			return (-1);
		}
		if (!rsp->use_nt) {		/* disallow lan-man hash */
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "LAN-MAN hash unacceptable");
			return (-1);
		}
		ppp_msoft_nt_challenge_response(chap->chal_data, password, buf);
		if (memcmp(rsp->nt_hash, buf, sizeof(buf)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "MSCHAPv1 hash is invalid");
			return (-1);
		}

		/* Derive MPPE keys */
		ppp_msoft_init_key_v1(0, password,
		    chap->chal_data, resp->mppe.msv1.key_64);
		ppp_msoft_init_key_v1(1, password,
		    chap->chal_data, resp->mppe.msv1.key_128);
		printf("[DEMO] MSv1 MPPE 64 BIT KEY:\n");
		ppp_log_dump(log, LOG_DEBUG,
		    resp->mppe.msv1.key_64, sizeof(resp->mppe.msv1.key_64));
		printf("[DEMO] MSv1 MPPE 128 BIT KEY:\n");
		ppp_log_dump(log, LOG_DEBUG,
		    resp->mppe.msv1.key_128, sizeof(resp->mppe.msv1.key_128));

		/* Done */
		return (0);
	    }
	case PPP_AUTH_CHAP_MSV2:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		const struct ppp_auth_cred_chap_msv2 *const rsp = &chap->u.msv2;
		u_char buf[PPP_MSOFT_NT_HASH_LEN];

		printf("[DEMO] MSv2 auth check user=\"%s\"\n", chap->name);

		/* Check response */
		if (strcmp(chap->name, user) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "wrong username");
			return (-1);
		}
		ppp_msoft_generate_nt_response(chap->chal_data,
		    rsp->peer_chal, chap->name, password, buf);
		if (memcmp(rsp->nt_response, buf, sizeof(buf)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "MSCHAPv2 hash is invalid");
			return (-1);
		}

		/* Generate expected authenticator response for reply */
		ppp_msoft_generate_authenticator_response(password,
		    rsp->nt_response, rsp->peer_chal, chap->chal_data,
		    chap->name, resp->authresp);

		/* Derive MPPE keys */
		for (i = 0; i < 2; i++) {
			ppp_msoft_init_key_v2(i, password,
			    rsp->nt_response, resp->mppe.msv2.keys[i]);
			printf("[DEMO] MSv2 MPPE SERVER %s KEY:\n",
			    i == 0 ? "XMIT" : "RECV");
			ppp_log_dump(log, LOG_DEBUG, resp->mppe.msv2.keys[i],
			    sizeof(resp->mppe.msv2.keys[i]));
		}

		/* Done */
		return (0);
	    }
	case PPP_AUTH_CHAP_MD5:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		struct ppp_auth_cred_chap temp;

		printf("[DEMO] CHAP-MD5 auth check user=\"%s\"\n", chap->name);
		strlcpy(temp.name, chap->name, sizeof(temp.name));
		temp.chal_len = chap->chal_len;
		memcpy(temp.chal_data, chap->chal_data, chap->chal_len);
		temp.u.md5.id = chap->u.md5.id;
		(*ppp_auth_chap_md5.hash)(&temp, password, strlen(password));
		if (memcmp(temp.u.md5.hash,
		    chap->u.md5.hash, sizeof(temp.u.md5.hash)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid MD5 hash");
			return (-1);
		}
		return (0);
	    }
	default:
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "unsupported auth check");
		errno = EPROTONOSUPPORT;
		return (-1);
	}
}

/***********************************************************************
			LOG METHODS
***********************************************************************/

static void
demo_log_vput(void *arg, int sev, const char *fmt, va_list args)
{
	valog(sev, fmt, args);
}

