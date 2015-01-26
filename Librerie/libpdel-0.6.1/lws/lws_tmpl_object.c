
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_tmpl.h"
#include "lws_tmpl_object.h"
#include "lws_tmpl_passwd.h"
#include "lws_tmpl_config.h"
#include "lws_tmpl_logs.h"
#include "lws_tmpl_memstats.h"
#ifndef WIN32
#include <pwd.h>
#endif

/* Per-thread object descriptor */
struct tmpl_object {
	const char	*name;
	struct tinfo	*tinfo;
};

/* Per-thread object list */
static const struct tmpl_object tmpl_objects[] = {
	{ "config",		&lws_tmpl_config_tinfo },
	{ "logs",		&lws_tmpl_logs_tinfo },
	{ "memstats",		&lws_tmpl_memstats_tinfo },
	{ "tmpl_funclist",	&lws_tmpl_funclist_tinfo },
	{ NULL }
};

/***********************************************************************
			OBJECT FUNCTIONS
***********************************************************************/

#define PASSWD_PREFIX		"passwd:"

static tmpl_handler_t	lws_tf_object_array_delete;
static tmpl_handler_t	lws_tf_object_array_insert;
static tmpl_handler_t	lws_tf_object_union_set;
static tmpl_handler_t	lws_tf_object_get;
static tmpl_handler_t	lws_tf_object_reset;
static tmpl_handler_t	lws_tf_object_set;

static struct	tinfo *lws_tmpl_object_find(const char *name,
			char **errmsgp, const char *mtype);

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_object_functions[] = {
    LWS_TMPL_FUNC(object_get, 2, 2, "object:field",
	"Returns the contents of field $2 in the per-thread object named $1."
"\n"	"The available objects are:"
"\n"	"<ul><li><b>config</b> - Current LWS configuration</li>"
"\n"	"<li><b>logs</b> - Logs (see"
"\n"		"<a href=\"#logs_load\">@logs_load()</a>)</li>"
"\n"	"<li><b>memstats</b> - Heap memory usage (when running with -D)</li>"
"\n"	"<li><b>tmpl_funclist</b> - This list of user-defined functions</li>"
"\n"	"<li><b>passwd:<em>user</em></b> - The <code>struct passwd</code>"
"\n"		"corresponding to <em>user</em>. Only one user's information"
"\n"		"can be loaded at a time</li>"
"\n"	"</ul>"),
    LWS_TMPL_FUNC(object_set, 3, 3, "object:field:value",
	"Sets field $2 in the per-thread object named $1 to $3. Returns the"
"\n"	"empty string if sucessful, otherwise an error string."),
    LWS_TMPL_FUNC(object_reset, 1, 2, "object:field",
	"Resets the object named $1 to its intial, default value. An optional"
"\n"	"subfield $2 may be supplied to only reset the subfield."),
    LWS_TMPL_FUNC(object_array_insert, 3, 3, "object:field:index",
	"Insert a new element into the array field $2 of the object named"
"\n"	"$1 at index $3."),
    LWS_TMPL_FUNC(object_array_delete, 3, 3, "object:field:index",
	"Delete the element at index $3 in the array field $2 of the object"
"\n"	"named $1."),
    LWS_TMPL_FUNC(object_union_set, 3, 3, "object:union:field",
	"Change the current field of union $2 in object $1 to $3."),
    { { NULL } }
};

/*
 * Set a field from a per-thread named object.
 *
 * Usage: @object_get(object, field)
 */
static char *
lws_tf_object_get(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const void *object;
	struct tinfo *t;
	char *rtn;

	/* Find object */
	if ((t = lws_tmpl_object_find(av[1], errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Get desired field from object */
	if ((rtn = structs_get_string(t->type, av[2], object, mtype)) == NULL) {
		if (errno == ENOENT) {
			ASPRINTF(mtype, errmsgp, "no such field \"%s\" in"
			    " object \"%s\"", av[2] _ av[1]);
		}
	}

	/* Done */
	return (rtn);
}

/*
 * Set a field in a per-thread named object.
 *
 * Usage: @object_set(object, field, value)
 */
static char *
lws_tf_object_set(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	struct tinfo *t;
	char ebuf[128];
	void *object;

	/* Find object */
	if ((t = lws_tmpl_object_find(av[1], errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Set desired field in object */
	if (structs_set_string(t->type, av[2],
	    av[3], object, ebuf, sizeof(ebuf)) == -1)
		return (STRDUP(mtype, ebuf));

	/* Done */
	return (STRDUP(mtype, ""));
}

/*
 * Reset a (sub-field of a) per-thread named object to its intitial state.
 *
 * Usage: @object_reset(object [, sub-field])
 */
static char *
lws_tf_object_reset(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *const subfield = (ac >= 3) ? av[2] : NULL;
	struct tinfo *t;
	void *object;

	/* Find object */
	if ((t = lws_tmpl_object_find(av[1], errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Reset subfield of object, or entire object if no subfield given */
	if (subfield == NULL)
		tinfo_set(t, NULL);
	else if (structs_reset(t->type, subfield, object) == -1) {
		ASPRINTF(mtype, errmsgp, "error resetting subfield \"%s\""
		    " of object \"%s\": %s", subfield _ av[1] _ strerror(errno));
		return (NULL);
	}

	/* Done */
	return (STRDUP(mtype, ""));
}

/*
 * Insert an element into a per-thread named object array.
 *
 * Usage: @object_array_insert(object, arrayfield, index)
 */
static char *
lws_tf_object_array_insert(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *const objectname = av[1];
	const char *const subfield = av[2];
	const u_int index = strtol(av[3], NULL, 10);
	struct tinfo *t;
	void *object;

	/* Find object */
	if ((t = lws_tmpl_object_find(objectname, errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Insert array item */
	if (structs_array_insert(t->type, subfield, index, object) == -1) {
		ASPRINTF(mtype, errmsgp, "error %s array element"
		    " \"%s.%s.%u\": %s", "inserting" _ objectname _
		    subfield _ index _ strerror(errno));
		return (NULL);
	}

	/* Done */
	return (STRDUP(mtype, ""));
}

/*
 * Delete an element from a per-thread named object array.
 *
 * Usage: @object_array_delete(object, arrayfield, index)
 */
static char *
lws_tf_object_array_delete(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *const objectname = av[1];
	const char *const subfield = av[2];
	const u_int index = strtol(av[3], NULL, 10);
	struct tinfo *t;
	void *object;

	/* Find object */
	if ((t = lws_tmpl_object_find(objectname, errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Delete array item */
	if (structs_array_delete(t->type, subfield, index, object) == -1) {
		ASPRINTF(mtype, errmsgp, "error %s array element"
		    " \"%s.%s.%u\": %s", "deleting" _ objectname _
		    subfield _ index _ strerror(errno));
		return (NULL);
	}

	/* Done */
	return (STRDUP(mtype, ""));
}

/*
 * Set union field.
 *
 * Usage: @object_union_set(object, union, field)
 */
static char *
lws_tf_object_union_set(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *const objectname = av[1];
	const char *const union_name = av[2];
	const char *const field_name = av[3];
	struct tinfo *t;
	void *object;

	/* Find object */
	if ((t = lws_tmpl_object_find(objectname, errmsgp, mtype)) == NULL)
		return (NULL);

	/* Get object */
	if ((object = tinfo_get(t)) == NULL)
		return (NULL);

	/* Set union field */
	if (structs_union_set(t->type, union_name, object, field_name) == -1) {
		ASPRINTF(mtype, errmsgp, "can't set union \"%s.%s\" field"
		    " to \"%s\": %s", objectname _ union_name _ field_name _
		    strerror(errno));
		return (NULL);
	}

	/* Done */
	return (STRDUP(mtype, ""));
}


/*
 * Find a per-thread named object by name.
 */
static struct tinfo *
lws_tmpl_object_find(const char *name, char **errmsgp, const char *mtype)
{
	int i;

	/* Search fixed-name list */
	for (i = 0; tmpl_objects[i].name != NULL; i++) {
		if (strcmp(name, tmpl_objects[i].name) == 0)
			return (tmpl_objects[i].tinfo);
	}

	/* Try 'passwd' object */
	if (strncmp(name, PASSWD_PREFIX, sizeof(PASSWD_PREFIX) - 1) == 0) {
		static pthread_mutex_t passwd_mutex = PTHREAD_MUTEX_INITIALIZER;
		struct passwd *pw;

#ifdef WIN32
		static struct passwd pws;
		pw = &pws;
#endif
		if ((errno = pthread_mutex_lock(&passwd_mutex)) != 0)
			goto not_passwd;
#ifndef WIN32
		if ((pw = getpwnam(name + sizeof(PASSWD_PREFIX) - 1)) == NULL) {
			pthread_mutex_unlock(&passwd_mutex);
			goto not_passwd;
		}
#endif
		if (tinfo_set(&lws_tmpl_passwd_tinfo, pw) == -1) {
			pthread_mutex_unlock(&passwd_mutex);
			goto not_passwd;
		}
		pthread_mutex_unlock(&passwd_mutex);
		return (&lws_tmpl_passwd_tinfo);
	}
not_passwd:

	/* Not found */
	ASPRINTF(mtype, errmsgp, "unknown object \"%s\"", name);
	return (NULL);
}

