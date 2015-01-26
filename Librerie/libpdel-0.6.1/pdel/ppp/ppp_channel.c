
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_channel.h"

void
ppp_channel_open(struct ppp_channel *chan)
{
	if (chan->meth->open != NULL)
		(*chan->meth->open)(chan);
}

void
ppp_channel_close(struct ppp_channel *chan)
{
	if (chan->meth->close != NULL)
		(*chan->meth->close)(chan);
}

void
ppp_channel_destroy(struct ppp_channel **chanp)
{
	struct ppp_channel *chan = *chanp;

	if (chan == NULL)
		return;
	*chanp = NULL;
	(*chan->meth->destroy)(&chan);
}

struct mesg_port *
ppp_channel_get_outport(struct ppp_channel *chan)
{
	return (chan->outport);
}

void
ppp_channel_free_output(struct ppp_channel *chan,
	struct ppp_channel_output *output)
{
	(*chan->meth->free_output)(chan, output);
}

void
ppp_channel_set_link_info(struct ppp_channel *chan, u_int32_t accm)
{
	if (chan->meth->set_link_info != NULL)
		(*chan->meth->set_link_info)(chan, accm);
}

int
ppp_channel_get_origination(struct ppp_channel *chan)
{
	return ((*chan->meth->get_origination)(chan));
}

const char *
ppp_channel_get_node(struct ppp_channel *chan)
{
	if (chan->meth->get_node == NULL)
		return (NULL);
	return ((*chan->meth->get_node)(chan));
}

const char *
ppp_channel_get_hook(struct ppp_channel *chan)
{
	if (chan->meth->get_hook == NULL)
		return (NULL);
	return ((*chan->meth->get_hook)(chan));
}

int
ppp_channel_is_async(struct ppp_channel *chan)
{
	if (chan->meth->is_async == NULL)
		return (0);
	return ((*chan->meth->is_async)(chan));
}

u_int
ppp_channel_get_mtu(struct ppp_channel *chan)
{
	if (chan->meth->get_mtu == NULL)
		return (0);
	return ((*chan->meth->get_mtu)(chan));
}

int
ppp_channel_get_acfcomp(struct ppp_channel *chan)
{
	if (chan->meth->get_acfcomp == NULL)
		return (0);
	return ((*chan->meth->get_acfcomp)(chan));
}

int
ppp_channel_get_pfcomp(struct ppp_channel *chan)
{
	if (chan->meth->get_pfcomp == NULL)
		return (0);
	return ((*chan->meth->get_pfcomp)(chan));
}


