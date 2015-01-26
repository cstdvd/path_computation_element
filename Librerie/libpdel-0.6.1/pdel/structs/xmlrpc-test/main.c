
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

#define XMLRPC_TEST_VERSION	"libpdel/" PDEL_VERSION_STRING

/* List of XML-RPC methods */
static const struct http_servlet_xmlrpc_method *method_list[] = {
	&arrayOfStructsTest_method,
	&countTheEntities_method,
	&easyStructTest_method,
	&echoStructTest_method,
	&manyTypesTest_method,
	&moderateSizeArrayCheck_method,
	&nestedStructTest_method,
	&simpleStructReturnTest_method,
	&faultTest_method,
};
#define NUM_METHODS	(sizeof(method_list) / sizeof(*method_list))

/* Global variables */
int		debug_level;

/* Internal functions */
static void		usage(void);

/*
 * Implement the XML-RPC validation test suite.
 */
int
main(int argc, char **argv)
{
	struct http_servlet_xmlrpc_method xmlrpc_methods[NUM_METHODS + 1];
	struct http_servlet_xmlrpc_info xmlrpc_info;
	static const struct in_addr zero_ip;
	struct http_servlet *servlet;
	struct http_server *server;
	struct pevent_ctx *ctx;
	char versbuf[64];
	sigset_t sigs;
	int port = 0;
	int sig;
	int ch;
	int i;

	/* Parse command line arguments */
	while ((ch = getopt(argc, argv, "dp:")) != -1) {
		switch (ch) {
		case 'd':
			debug_level++;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	switch (argc) {
	case 0:
		break;
	default:
		usage();
		break;
	}
	if (port == 0)
		port = 80;

	/* Enable typed memory */
	if (debug_level > 0 && typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	/* Get event context */
	if ((ctx = pevent_ctx_create("xmlrpc_test_server.ctx", NULL)) == NULL)
		err(1, "pevent_ctx_create");

	/* Start HTTP server */
	snprintf(versbuf, sizeof(versbuf), "%s (%s/%s)",
	    XMLRPC_TEST_VERSION, host_os, host_arch);
	if ((server = http_server_start(ctx,
	    zero_ip, port, NULL, versbuf, alog)) == NULL)
		err(1, "http_server_start");
	fprintf(stderr, "started XML-RPC server on port %d\n", port);

	/* Create XML-RPC servlet */
	for (i = 0; i < NUM_METHODS; i++)
		xmlrpc_methods[i] = *method_list[i];
	memset(&xmlrpc_methods[i], 0, sizeof(xmlrpc_methods[i]));
	memset(&xmlrpc_info, 0, sizeof(xmlrpc_info));
	xmlrpc_info.methods = xmlrpc_methods;
	xmlrpc_info.logger = alog;
	if ((servlet = http_servlet_xmlrpc_create(&xmlrpc_info,
	    NULL, NULL)) == NULL)
		err(1, "http_servlet_xmlrpc_create");

	/* Register XML-RPC servlet */
	if (http_server_register_servlet(server,
	    servlet, NULL, "^" XML_RPC_URL "$", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Wait for interrupt */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1)
		err(1, "sigprocmask");
	if (sigwait(&sigs, &sig) == -1)
		err(1, "sigwait");

	/* Shut down server */
	if (debug_level > 0)
		fprintf(stderr, "\nrec'd signal %d, shutting down...\n", sig);
	http_server_stop(&server);

	/* Destroy event context */
	pevent_ctx_destroy(&ctx);
	usleep(100000);

	/* Done */
	if (debug_level > 0) {
		fprintf(stderr, "displaying unfreed memory...\n");
		typed_mem_dump(stdout);
	}
	return (0);
}

/*
 * Exit after printing usage string
 */
static void
usage(void)
{
	fprintf(stderr, "Usage: xmlrpc_test [-d] [-p port]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-d\tIncrease debugging level\n");
	fprintf(stderr, "\t-p\tSpecify HTTP server port (default 80)\n");
	exit(1);
}

