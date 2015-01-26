
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_manager.h"

void *
ppp_manager_bundle_config(struct ppp_manager *manager,
	struct ppp_link *link, struct ppp_bundle_config *conf)
{
	return ((*manager->meth->bundle_config)(manager, link, conf));
}

void *
ppp_manager_bundle_plumb(struct ppp_manager *manager,
	struct ppp_bundle *bundle, const char *path, const char *hook,
	struct in_addr *ips, struct in_addr *dns, struct in_addr *nbns,
	u_int mtu)
{
	return ((*manager->meth->bundle_plumb)(manager, bundle,
	    path, hook, ips, dns, nbns, mtu));
}

void
ppp_manager_bundle_unplumb(struct ppp_manager *manager, void *arg,
	struct ppp_bundle *bundle)
{
	if (manager->meth->bundle_unplumb != NULL)
		(*manager->meth->bundle_unplumb)(manager, arg, bundle);
}

void
ppp_manager_release_ip(struct ppp_manager *manager,
	struct ppp_bundle *bundle, struct in_addr ip)
{
	if (manager->meth->release_ip != NULL)
		(*manager->meth->release_ip)(manager, bundle, ip);
}


