
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_fsm_option.h"

#define FSM_OPTION_MTYPE	"ppp_fsm_option"

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Create new options array.
 */
struct ppp_fsm_options *
ppp_fsm_option_create(void)
{
	struct ppp_fsm_options *opt;

	if ((opt = MALLOC(FSM_OPTION_MTYPE, sizeof(*opt))) == NULL)
		return (NULL);
	memset(opt, 0, sizeof(*opt));
	return (opt);
}

/*
 * Destroy options array.
 */
void
ppp_fsm_option_destroy(struct ppp_fsm_options **optsp)
{
	struct ppp_fsm_options *const opts = *optsp;
	int i;

	if (opts == NULL)
		return;
	*optsp = NULL;
	for (i = 0; i < opts->num; i++)
		FREE(FSM_OPTION_MTYPE, opts->opts[i].data);
	FREE(FSM_OPTION_MTYPE, opts->opts);
	FREE(FSM_OPTION_MTYPE, opts);
}

/*
 * Add an option to an options array.
 */
int
ppp_fsm_option_add(struct ppp_fsm_options *opts,
	u_char type, u_char len, const void *data)
{
	struct ppp_fsm_option *opt;
	void *buf = NULL;
	void *mem;

	/* Copy option data into buffer */
	if (len > 0) {
		if ((buf = MALLOC(FSM_OPTION_MTYPE, len)) == NULL)
			return (-1);
		memcpy(buf, data, len);
	}

	/* Extend options array by one */
	if ((mem = REALLOC(FSM_OPTION_MTYPE, opts->opts,
	    (opts->num + 1) * sizeof(*opts->opts))) == NULL) {
		FREE(FSM_OPTION_MTYPE, buf);
		return (-1);
	}
	opts->opts = mem;
	opt = &opts->opts[opts->num++];

	/* Fill in option info */
	opt->type = type;
	opt->len = len;
	opt->data = buf;

	/* Done */
	return (0);
}

/*
 * Remove an option from an options array.
 */
int
ppp_fsm_option_del(struct ppp_fsm_options *opts, u_int index)
{
	struct ppp_fsm_option *const opt = opts->opts + index;

	if (index >= opts->num) {
		errno = EDOM;
		return (-1);
	}
	FREE(FSM_OPTION_MTYPE, opt->data);
	memmove(opts->opts + index, opts->opts + index + 1,
	    (--opts->num - index) * sizeof(*opts->opts));
	return (0);
}

/*
 * Reset an options array to empty.
 */
void
ppp_fsm_option_zero(struct ppp_fsm_options *opts)
{
	while (opts->num > 0)
		ppp_fsm_option_del(opts, 0);
	FREE(FSM_OPTION_MTYPE, opts->opts);
	opts->opts = NULL;
}

/*
 * Copy an options array.
 */
struct ppp_fsm_options *
ppp_fsm_option_copy(struct ppp_fsm_options *opts)
{
	struct ppp_fsm_options *copy;
	int i;

	if ((copy = ppp_fsm_option_create()) == NULL)
		return (NULL);
	for (i = 0; i < opts->num; i++) {
		const struct ppp_fsm_option *const opt = &opts->opts[i];

		if (ppp_fsm_option_add(copy,
		    opt->type, opt->len, opt->data) == -1) {
			ppp_fsm_option_destroy(&copy);
			return (NULL);
		}
	}
	return (copy);
}

/*
 * Compare two options arrays for equality.
 *
 * If "i1" or "i2" is equal to -1 then we compare all options.
 * Otherwise we just compare option "i1" of "o1" to option "i2" of "o2".
 */
int
ppp_fsm_option_equal(const struct ppp_fsm_options *o1,
	int i1, const struct ppp_fsm_options *o2, int i2)
{
	int i;

	/* Compare all options? */
	if (i1 == -1 && i2 == -1) {
		if (o1->num != o2->num)
			return (0);
		for (i = 0; i < o1->num; i++) {
			if (!ppp_fsm_option_equal(o1, i, o2, i))
				return (0);
		}
		return (1);
	}

	/* Sanity check indicies */
	if (i1 < 0 || i2 < 0 || i1 >= o1->num || i2 >= o2->num) {
		errno = EINVAL;
		return (-1);
	}

	/* Compare two options */
	if (o1->opts[i1].type != o2->opts[i2].type)
		return (0);
	if (o1->opts[i1].len != o2->opts[i2].len)
		return (0);
	if (memcmp(o1->opts[i1].data, o2->opts[i2].data, o1->opts[i1].len) != 0)
		return (0);
	return (1);
}

/*
 * Print out options into the log.
 */
void
ppp_fsm_options_decode(const struct ppp_fsm_optdesc *optlist,
	const u_char *data, u_int len, char *buf, size_t bmax)
{
	struct ppp_fsm_options *opts;
	int i;

	/* Decode options */
	if ((opts = ppp_fsm_option_unpack(data, len)) == NULL)
		return;

	/* Special case for empty */
	if (opts->num == 0) {
		strlcpy(buf, "(no options)", bmax);
		goto done;
	}

	/* Print options into buffer */
	strlcpy(buf, "", bmax);
	for (i = 0; i < opts->num; i++) {
		const struct ppp_fsm_option *const opt = &opts->opts[i];
		const struct ppp_fsm_optdesc *const desc
		    = ppp_fsm_option_desc(optlist, opt);

		if (i > 0)
			strlcat(buf, " ", bmax);	/* separator */
		strlcat(buf, "[", bmax);
		if (desc == NULL) {
			snprintf(buf + strlen(buf), bmax - strlen(buf),
			    "?%u (len=%u)", opt->type, opt->len);
		} else {
			strlcat(buf, desc->name, bmax);
			if (desc->print != NULL) {
				strlcat(buf, " ", bmax);
				(*desc->print)(desc, opt,
				    buf + strlen(buf), bmax - strlen(buf));
			}
		}
		strlcat(buf, "]", bmax);
	}

done:
	/* Clean up */
	ppp_fsm_option_destroy(&opts);
}

/*
 * Find option descriptor in a table.
 */
const struct ppp_fsm_optdesc *
ppp_fsm_option_desc(const struct ppp_fsm_optdesc *optlist,
	const struct ppp_fsm_option *opt)
{
	const struct ppp_fsm_optdesc *desc;

	for (desc = optlist; desc->name != NULL; desc++) {
		if (opt->type == desc->type)
			return (desc);
	}
	return (NULL);
}

/***********************************************************************
		    PACKING/UNPACKING OPTIONS
***********************************************************************/

/*
 * Extract encoded options, stopping at the first malformed option.
 *
 * Returns NULL if there was a system error.
 */
struct ppp_fsm_options *
ppp_fsm_option_unpack(const u_char *data, u_int len)
{
	struct ppp_fsm_options *opts;

	if ((opts = ppp_fsm_option_create()) == NULL)
		return (NULL);
	while (len >= 2) {
		const u_char type = data[0];
		const u_char olen = data[1];

		if (olen < 2 || olen > len)
			break;
		if (ppp_fsm_option_add(opts, type, olen - 2, data + 2) == -1) {
			ppp_fsm_option_destroy(&opts);
			return (NULL);
		}
		data += olen;
		len -= olen;
	}
	return (opts);
}

/*
 * Compute length of packed options.
 */
u_int
ppp_fsm_option_packlen(struct ppp_fsm_options *opts)
{
	u_int len;
	int i;

	for (len = i = 0; i < opts->num; i++)
		len += 2 + opts->opts[i].len;
	return (len);
}

/*
 * Pack options into buffer.
 */
void
ppp_fsm_option_pack(struct ppp_fsm_options *opts, u_char *buf)
{
	int i;

	for (i = 0; i < opts->num; i++) {
		struct ppp_fsm_option *const opt = &opts->opts[i];

		*buf++ = opt->type;
		*buf++ = 2 + opt->len;
		memcpy(buf, opt->data, opt->len);
		buf += opt->len;
	}
}

/***********************************************************************
		BUILT-IN OPTIONS PRINTER FUNCTIONS
***********************************************************************/

#define MAX_BINARY	16

/*
 * Print option as binary data.
 */
void
ppp_fsm_pr_binary(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	int i;

	for (i = 0; i < opt->len; i++) {
		if (i >= MAX_BINARY) {
			snprintf(buf + strlen(buf), bmax - strlen(buf),
			    "...");
			break;
		}
		if (i == 0)
			snprintf(buf, bmax, "%02x", opt->data[0]);
		else
			snprintf(buf + strlen(buf), bmax - strlen(buf),
			    " %02x", opt->data[i]);
	}
}

/*
 * Print option as 32 bit hex value.
 */
void
ppp_fsm_pr_hex32(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	u_int32_t val;

	if (opt->len < 4) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	memcpy(&val, opt->data, 4);
	val = ntohl(val);
	snprintf(buf, bmax, "0x%08x", val);
}

/*
 * Print option as a 16 bit hex value.
 */
void
ppp_fsm_pr_int16(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	u_int16_t val;

	if (opt->len < 2) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	memcpy(&val, opt->data, 2);
	val = ntohs(val);
	snprintf(buf, bmax, "%u", val);
}

