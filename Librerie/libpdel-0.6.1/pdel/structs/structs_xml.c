
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <net/ethernet.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <expat.h>

#include "structs/structs.h"
#include "structs/xml.h"
#include "structs/type/array.h"
#include "structs/type/struct.h"
#include "structs/type/union.h"
#include "util/typed_mem.h"
#include "sys/alog.h"

/* Standard XML header */
#define XML_HEADER		"<?xml version=\"1.0\" standalone=\"yes\"?>\n"

/* Max parse depth */
#define MAX_XML_STACK		128

#define INPUT_MEM_TYPE		"structs_xml_input"
#define INFO_MEM_TYPE		"structs_xml_input.info"
#define CHARDATA_MEM_TYPE	"structs_xml_input.chardata"
#define OUTPUT_MEM_TYPE		"structs_xml_output"

/* Parse private context */
struct xmlinput_stackframe {
	const struct structs_type	*type;		/* type we're parsing */
	void				*data;		/* data pointer */
	char				*s;		/* character data */
	u_int				s_len;		/* strlen(s) */
	u_int				index;		/* fixed array index */
	u_char				combined;	/* was a combined tag */
};

struct xml_input_info {
	XML_Parser			p;
	int				error;
	int				depth;
	int				flags;
	u_int				skip;
	const char			*elem_tag;
	char				**attrp;
	const char			*attr_mtype;
	int				attr_len;
	struct xmlinput_stackframe	stack[MAX_XML_STACK];
	structs_xmllog_t		*logger;
};

/*
 * Internal functions
 */
static void	*structs_xml_malloc(size_t size);
static void	*structs_xml_realloc(void *ptr, size_t size);
static void	structs_xml_free(void *ptr);

static void	structs_xml_output_cleanup(void *arg);
static void	structs_xml_output_prefix(FILE *fp, int depth);
static void	structs_xml_encode(FILE *fp, const char *s);

static structs_xmllog_t	structs_xml_null_logger;
static structs_xmllog_t	structs_xml_stderr_logger;
static structs_xmllog_t	structs_xml_alog_logger;

/*
 * Internal variables
 */
static const XML_Memory_Handling_Suite memsuite = {
	structs_xml_malloc,
	structs_xml_realloc,
	structs_xml_free
};

static const	char separator_string[] = { STRUCTS_SEPARATOR, '\0' };
static const	char double_separator_string[] = {
	STRUCTS_SEPARATOR, STRUCTS_SEPARATOR, '\0'
};

/*********************************************************************
			XML INPUT ROUTINES
*********************************************************************/

/*
 * Internal functions
 */
static void	structs_xml_input_cleanup(void *arg);
static void	structs_xml_input_start(void *userData,
			const XML_Char *name, const XML_Char **atts);
static void	structs_xml_input_end(void *userData, const XML_Char *name);
static void	structs_xml_input_chardata(void *userData,
			const XML_Char *s, int len);
static void	structs_xml_input_nest(struct xml_input_info *info,
			const char *name, const struct structs_type **typep,
			void **datap);
static void	structs_xml_input_prep(struct xml_input_info *info,
			const struct structs_type *type, void *data,
			int combined);
static void	structs_xml_unnest(struct xml_input_info *info,
			const XML_Char *name);
static void	structs_xml_pop(struct xml_input_info *info);

/* Context for one XML parse run */
struct structs_xml_input_ctx {
	struct xml_input_info		*info;
	XML_Parser			p;
	int				data_init;
	int				rtn;
	const struct structs_type	*type;
	void				*data;
	char				**attrp;
	const char			*attr_mtype;
};

/*
 * Input a type from XML.
 *
 * Note: it is safe for the calling thread to be canceled.
 */
int
structs_xml_input(const struct structs_type *type,
	const char *elem_tag, char **attrp, const char *attr_mtype,
	FILE *fp, void *data, int flags, structs_xmllog_t *logger)
{
	struct structs_xml_input_ctx *ctx;
	int esave;
	int r;

	/* Special cases for logger */
	if (logger == STRUCTS_LOGGER_NONE)
		logger = structs_xml_null_logger;
	else if (logger == STRUCTS_LOGGER_STDERR)
		logger = structs_xml_stderr_logger;
	else if (logger == STRUCTS_LOGGER_ALOG)
		logger = structs_xml_alog_logger;

	/* Create context */
	if ((ctx = MALLOC(INPUT_MEM_TYPE, sizeof(*ctx))) == NULL)
		return (-1);
	memset(ctx, 0, sizeof(*ctx));
	ctx->attrp = attrp;
	ctx->attr_mtype = attr_mtype;
	ctx->type = type;
	ctx->data = data;
	ctx->rtn = -1;
	pthread_cleanup_push(structs_xml_input_cleanup, ctx);

	/* Initialize attributes */
	if (ctx->attrp != NULL
	    && (*ctx->attrp = STRDUP(ctx->attr_mtype, "")) == NULL)
		goto done;

	/* Initialize data object if desired */
	if (ctx->type != NULL && (flags & STRUCTS_XML_UNINIT) != 0) {
		if ((*type->init)(ctx->type, ctx->data) == -1) {
			esave = errno;
			(*logger)(LOG_ERR, "error initializing data: %s",
			    strerror(errno));
			errno = esave;
			goto done;
		}
		ctx->data_init = 1;
	}

	/* Allocate info structure */
	if ((ctx->info = MALLOC(INFO_MEM_TYPE, sizeof(*ctx->info))) == NULL) {
		esave = errno;
		(*logger)(LOG_ERR, "%s: %s", "malloc", strerror(errno));
		errno = esave;
		goto done;
	}
	memset(ctx->info, 0, sizeof(*ctx->info));
	ctx->info->logger = logger;
	ctx->info->attrp = attrp;
	ctx->info->attr_len = 0;
	ctx->info->attr_mtype = attr_mtype;
	ctx->info->elem_tag = elem_tag;
	ctx->info->stack[0].type = type;
	ctx->info->stack[0].data = data;
	ctx->info->flags = flags;

	/* Create a new parser */
	if ((ctx->p = XML_ParserCreate_MM(NULL, &memsuite, NULL)) == NULL) {
		esave = errno;
		(*logger)(LOG_ERR,
		    "error creating XML parser: %s", strerror(errno));
		errno = esave;
		goto done;
	}
	ctx->info->p = ctx->p;
	XML_SetUserData(ctx->p, ctx->info);
	XML_SetElementHandler(ctx->p,
	    structs_xml_input_start, structs_xml_input_end);
	XML_SetCharacterDataHandler(ctx->p, structs_xml_input_chardata);

	/* Parse it */
	while (1) {
		const int bufsize = 1024;
		size_t len;
		void *buf;

		/* Get buffer */
		if ((buf = XML_GetBuffer(ctx->p, bufsize)) == NULL) {
			esave = errno;
			(*logger)(LOG_ERR,
			    "error from XML_GetBuffer: %s", strerror(errno));
			errno = esave;
			break;
		}

		/* Read more bytes. Note: we could get canceled here. */
		len = fread(buf, 1, bufsize, fp);

		/* Check for error */
		if (ferror(fp)) {
			esave = errno;
			(*logger)(LOG_ERR, "read error: %s", strerror(errno));
			errno = esave;
			break;
		}

		/* Process them */
		if (len > 0 && !XML_ParseBuffer(ctx->p, len, feof(fp))) {
			(*logger)(LOG_ERR, "line %d:%d: %s",
			    XML_GetCurrentLineNumber(ctx->p),
			    XML_GetCurrentColumnNumber(ctx->p),
			    XML_ErrorString(XML_GetErrorCode(ctx->p)));
			errno = EINVAL;
			break;
		}

		/* Done? */
		if (feof(fp)) {
			if (ctx->info->error != 0)
				errno = ctx->info->error;
			else
				ctx->rtn = 0;
			break;
		}
	}

done:
	/* Clean up and exit */
	r = ctx->rtn;
	pthread_cleanup_pop(1);
	return (r);
}

/*
 * Cleanup for structs_xml_input()
 */
static void
structs_xml_input_cleanup(void *arg)
{
	struct structs_xml_input_ctx *const ctx = arg;
	const int esave = errno;

	/* Free private parse info */
	if (ctx->info != NULL) {
		while (ctx->info->depth >= 0)
			structs_xml_pop(ctx->info);
		FREE(INFO_MEM_TYPE, ctx->info);
	}

	/* Free parser */
	if (ctx->p != NULL)
		XML_ParserFree(ctx->p);

	/* If error, free returned attributes and initialized data */
	if (ctx->rtn != 0) {
		if (ctx->attrp != NULL && *ctx->attrp != NULL) {
			FREE(ctx->attr_mtype, *ctx->attrp);
			*ctx->attrp = NULL;
		}
		if (ctx->data_init)
			(*ctx->type->uninit)(ctx->type, ctx->data);
	}

	/* Free context */
	FREE(INPUT_MEM_TYPE, ctx);
	errno = esave;
}

/*
 * Start tag handler
 */
static void
structs_xml_input_start(void *userData,
	const XML_Char *name, const XML_Char **attrs)
{
	struct xml_input_info *const info = userData;
	struct xmlinput_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type = frame->type;
	void *data = frame->data;
	char *namebuf;
	char *ctx;
	int first;
	char *s;
	int sev;

	/* Skip if any errors */
	if (info->error)
		return;
	if (info->skip) {
		info->skip++;
		return;
	}

	/* Handle the top level tag specially */
	if (info->depth == 0) {
		int i;

		/* The top level tag must match what we expect */
		if (strcmp(name, info->elem_tag) != 0) {
			(*info->logger)(LOG_ERR,
			    "line %d:%d: expecting element \"%s\" here",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    info->elem_tag);
			info->error = EINVAL;
			return;
		}

		/* Extract attributes */
		for (i = 0; info->attrp != NULL && attrs[i] != NULL; i += 2) {
			const char *const name = attrs[i];
			const char *const value = attrs[i + 1];
			void *mem;

			if ((mem = REALLOC(info->attr_mtype,
			    *info->attrp, info->attr_len + strlen(name) + 1
			      + strlen(value) + 1 + 1)) == NULL) {
				info->error = errno;
				(*info->logger)(LOG_ERR, "line %d:%d: %s: %s",
				    XML_GetCurrentLineNumber(info->p),
				    XML_GetCurrentColumnNumber(info->p),
				    "malloc", strerror(errno));
				return;
			}
			*info->attrp = mem;
			strcpy(*info->attrp + info->attr_len, name);
			strcpy(*info->attrp + info->attr_len
			    + strlen(name) + 1, value);
			(*info->attrp)[info->attr_len + strlen(name) + 1
			    + strlen(value) + 1] = '\0';
			info->attr_len += strlen(name) + 1 + strlen(value) + 1;
		}

		/* Are we only scanning? */
		if ((info->flags & STRUCTS_XML_SCAN) != 0) {
			info->skip++;
			return;
		}

		/* Prep the top level data structure */
		structs_xml_input_prep(info, type, data, 0);
		return;
	}

	/* We don't allow attributes with non top level elements */
	if (attrs[0] != NULL) {
		sev = (info->flags & STRUCTS_XML_LOOSE) == 0 ?
		    LOG_ERR : LOG_WARNING;
		(*info->logger)(sev, "line %d:%d: element \"%s\""
		    " contains attributes (not allowed)",
		    XML_GetCurrentLineNumber(info->p),
		    XML_GetCurrentColumnNumber(info->p), name);
		if (sev == LOG_ERR) {
			info->error = EINVAL;
			return;
		}
	}

	/*
	 * Check the case of a structure or union field name
	 * containing the separator character. Such fields take
	 * precedence over combined XML tags.
	 */
	if (strchr(name, STRUCTS_SEPARATOR) != NULL) {
		switch (type->tclass) {
		case STRUCTS_TYPE_STRUCTURE:
		    {
			const struct structs_field *field;

			for (field = frame->type->args[0].v;
			    field->name != NULL; field++) {
				if (strcmp(field->name, name) == 0)
					goto not_combined;
			}
		    }
		case STRUCTS_TYPE_UNION:
		    {
			const struct structs_ufield *field;

			for (field = frame->type->args[0].v;
			    field->name != NULL; field++) {
				if (strcmp(field->name, name) == 0)
					goto not_combined;
			}
		    }
		default:
			break;
		}
	}

	/* Check whether we need to consider this as a combined tag */
	if ((info->flags & STRUCTS_XML_COMB_TAGS) == 0
	    || strchr(name, STRUCTS_SEPARATOR) == NULL)
		goto not_combined;

	/* Check that the combined XML tag is well-formed */
	if (name[0] == STRUCTS_SEPARATOR
	    || (name[0] != '\0' && name[strlen(name) - 1] == STRUCTS_SEPARATOR)
	    || strstr(name, double_separator_string) != NULL) {
		(*info->logger)(LOG_ERR, "line %d:%d: invalid combined"
		    " element tag \"%s\"", XML_GetCurrentLineNumber(info->p),
		    XML_GetCurrentColumnNumber(info->p), name);
		info->error = EINVAL;
		return;
	}

	/* Copy XML tag so we can parse it */
	if ((namebuf = STRDUP(TYPED_MEM_TEMP, name)) == NULL) {
		info->error = errno;
		return;
	}

	/* Parse combined XML tag into individual tag names */
	for (first = 1, s = strtok_r(namebuf, separator_string, &ctx);
	    s != NULL; first = 0, s = strtok_r(NULL, separator_string, &ctx)) {
		struct xmlinput_stackframe *const frame
		    = &info->stack[info->depth];

		type = frame->type;
		data = frame->data;
		structs_xml_input_nest(info, s, &type, &data);
		if (info->error != 0) {
			FREE(TYPED_MEM_TEMP, namebuf);
			return;
		}
		structs_xml_input_prep(info, type, data, !first);
		if (info->error != 0) {
			FREE(TYPED_MEM_TEMP, namebuf);
			return;
		}
	}
	FREE(TYPED_MEM_TEMP, namebuf);
	return;

not_combined:
	/* Handle a non-combined tag */
	structs_xml_input_nest(info, name, &type, &data);
	if (info->error != 0)
		return;
	structs_xml_input_prep(info, type, data, 0);
	if (info->error != 0)
		return;
}

/*
 * Nest one level deeper into the data structure we're inputting
 * using the sub-field "name".
 */
static void
structs_xml_input_nest(struct xml_input_info *info, const char *name,
	const struct structs_type **typep, void **datap)
{
	struct xmlinput_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type;
	void *data;
	int sev;

	/* Check type type */
	switch (frame->type->tclass) {
	case STRUCTS_TYPE_STRUCTURE:
	case STRUCTS_TYPE_UNION:
	    {
		/* Find field; for unions, adjust the field type if necessary */
		type = frame->type;
		data = frame->data;
		if ((type = structs_find(type, name, (const void **) &data, 1)) == NULL) {
			if (errno == ENOENT) {
				sev = (info->flags & STRUCTS_XML_LOOSE) == 0 ?
				    LOG_ERR : LOG_WARNING;
				(*info->logger)(sev,
				    "line %d:%d: element \"%s\" is not"
				    " expected here",
				    XML_GetCurrentLineNumber(info->p),
				    XML_GetCurrentColumnNumber(info->p), name);
				if (sev == LOG_ERR) {
					info->error = EINVAL;
					return;
				}
				info->skip++;
				return;
			}
			(*info->logger)(LOG_ERR, "line %d:%d: error"
			    " initializing union field \"%s\": %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    name, strerror(errno));
			info->error = errno;
			return;
		}
		break;
	    }

	case STRUCTS_TYPE_ARRAY:
	    {
		const struct structs_type *const etype = frame->type->args[0].v;
		const char *mtype = frame->type->args[1].s;
		const char *elem_name = frame->type->args[2].s;
		struct structs_array *const ary = frame->data;
		void *mem;

		/* Check tag name */
		if (strcmp(name, elem_name) != 0) {
			(*info->logger)(LOG_ERR, "line %d:%d: expected element"
			    " \"%s\" instead of \"%s\"",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    elem_name, name);
			info->error = EINVAL;
			return;
		}

		/* Expand the array by one */
		if ((mem = REALLOC(mtype,
		    ary->elems, (ary->length + 1) * etype->size)) == NULL) {
			info->error = errno;
			(*info->logger)(LOG_ERR, "line %d:%d: %s: %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    "realloc", strerror(errno));
			return;
		}
		ary->elems = mem;

		/* Initialize the new element */
		memset((char *)ary->elems + (ary->length * etype->size),
		    0, etype->size);
		if ((*etype->init)(etype,
		    (char *)ary->elems + (ary->length * etype->size)) == -1) {
			info->error = errno;
			(*info->logger)(LOG_ERR, "line %d:%d: %s: %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    "error initializing new array element",
			    strerror(errno));
			return;
		}

		/* Parse the element next */
		type = etype;
		data = (char *)ary->elems + (ary->length * etype->size);
		ary->length++;
		break;
	    }

	case STRUCTS_TYPE_FIXEDARRAY:
	    {
		const struct structs_type *const etype = frame->type->args[0].v;
		const char *elem_name = frame->type->args[1].s;
		const u_int length = frame->type->args[2].i;

		/* Check tag name */
		if (strcmp(name, elem_name) != 0) {
			(*info->logger)(LOG_ERR, "line %d:%d: expected element"
			    " \"%s\" instead of \"%s\"",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    elem_name, name);
			info->error = EINVAL;
			return;
		}

		/* Check index vs. array length */
		if (frame->index >= length) {
			sev = (info->flags & STRUCTS_XML_LOOSE) == 0 ?
			    LOG_ERR : LOG_WARNING;
			(*info->logger)(sev, "line %d:%d: too many"
			    " elements in fixed array (length %u)",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p), length);
			if (sev == LOG_ERR) {
				info->error = EINVAL;
				return;
			}
			info->skip++;
			return;
		}

		/* Parse the element next */
		type = etype;
		data = (char *)frame->data + (frame->index * etype->size);
		frame->index++;
		break;
	    }

	case STRUCTS_TYPE_PRIMITIVE:
		sev = (info->flags & STRUCTS_XML_LOOSE) == 0 ?
		    LOG_ERR : LOG_WARNING;
		(*info->logger)(sev,
		    "line %d:%d: element \"%s\" is not expected here",
		    XML_GetCurrentLineNumber(info->p),
		    XML_GetCurrentColumnNumber(info->p), name);
		if (sev == LOG_ERR) {
			info->error = EINVAL;
			return;
		}
		info->skip++;
		return;

	default:
		assert(0);
		return;
	}

	/* Done */
	*typep = type;
	*datap = data;
}

/*
 * Prepare the next level of nesting for parsing.
 */
static void
structs_xml_input_prep(struct xml_input_info *info,
	const struct structs_type *type, void *data, int combined)
{
	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* If next item is an array, re-initialize it */
	switch (type->tclass) {
	case STRUCTS_TYPE_ARRAY:
		(*type->uninit)(type, data);
		memset(data, 0, type->size);
		break;
	case STRUCTS_TYPE_FIXEDARRAY:
	    {
		void *mem;

		/* Get temporary region for newly initialized array */
		if ((mem = MALLOC(TYPED_MEM_TEMP, type->size)) == NULL) {
			info->error = errno;
			(*info->logger)(LOG_ERR, "line %d:%d: %s: %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    "error initializing new array", strerror(errno));
			return;
		}

		/* Initialize new array */
		if ((*type->init)(type, mem) == -1) {
			info->error = errno;
			(*info->logger)(LOG_ERR, "line %d:%d: %s: %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    "error initializing new array", strerror(errno));
			FREE(TYPED_MEM_TEMP, mem);
			return;
		}

		/* Replace existing array with fresh one */
		(*type->uninit)(type, data);
		memcpy(data, mem, type->size);
		FREE(TYPED_MEM_TEMP, mem);

		/* Remember that we're on the first element */
		info->stack[info->depth + 1].index = 0;
		break;
	    }
	default:
		break;
	}

	/* Check stack overflow */
	if (info->depth == MAX_XML_STACK - 1) {
		(*info->logger)(LOG_ERR,
		    "line %d:%d: maximum parse stack depth (%d) exceeded",
		    XML_GetCurrentLineNumber(info->p),
		    XML_GetCurrentColumnNumber(info->p), MAX_XML_STACK);
		info->error = EMLINK;
		return;
	}

	/* Continue in a new stack frame */
	info->depth++;
	info->stack[info->depth].type = type;
	info->stack[info->depth].data = data;
	info->stack[info->depth].combined = combined;
}

/*
 * Character data handler
 */
static void
structs_xml_input_chardata(void *userData, const XML_Char *s, int len)
{
	struct xml_input_info *const info = userData;
	struct xmlinput_stackframe *const frame = &info->stack[info->depth];
	void *mem;

	/* Skip if any errors */
	if (info->error || info->skip)
		return;

	/* Expand buffer and append character data */
	if ((mem = REALLOC(CHARDATA_MEM_TYPE,
	    frame->s, frame->s_len + len + 1)) == NULL) {
		info->error = errno;
		(*info->logger)(LOG_ERR, "%s: %s",
		    "realloc", strerror(errno));
		return;
	}
	frame->s = mem;
	memcpy(frame->s + frame->s_len, (char *)s, len);
	frame->s[frame->s_len + len] = '\0';
	frame->s_len += len;
}

/*
 * End tag handler
 */
static void
structs_xml_input_end(void *userData, const XML_Char *name)
{
	struct xml_input_info *const info = userData;
	struct xmlinput_stackframe *frame;
	int was_combined;

	/* Un-nest once for each structs tag */
	do {
		frame = &info->stack[info->depth];
		was_combined = frame->combined;
		structs_xml_unnest(info, name);
	} while (was_combined);
}

/*
 * Unnest one level
 */
static void
structs_xml_unnest(struct xml_input_info *info, const XML_Char *name)
{
	struct xmlinput_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type;
	const char *s;
	char ebuf[64];
	void *data;

	/* Skip if any errors */
	if (info->error)
		return;
	if (info->skip) {
		info->skip--;
		return;
	}

	/* Get current type and data */
	data = frame->data;
	type = frame->type;

	/*
	 * Convert from ASCII if possible, otherwise check only whitespace.
	 * For unions, we allow the field name tag to be omitted if you
	 * want to use the default field, which must have primitive type.
	 */
	switch (type->tclass) {
	case STRUCTS_TYPE_UNION:
	    {
		const struct structs_ufield *const field = type->args[0].v;

		/* Check to see if there's any non-whitespace text */
		if (frame->s == NULL)
			goto done;
		for (s = frame->s; *s != '\0' && isspace(*s); s++);
		if (*s == '\0')
			goto done;

		/* Default field must have primitive type */
		if (field->type->tclass != STRUCTS_TYPE_PRIMITIVE)
			break;

		/* Switch the union to the default field */
		if (structs_union_set(type, NULL, data, field->name) == -1) {
			info->error = errno;
			(*info->logger)(LOG_ERR,
			    "%s: %s", "structs_union_set", strerror(errno));
			return;
		}

		/* Point at the field instead of the union */
		type = field->type;
		data = ((const struct structs_union *)data)->un;

		/* FALLTHROUGH */
	    }
	case STRUCTS_TYPE_PRIMITIVE:
		if (structs_set_string(type, NULL,
		    frame->s, data, ebuf, sizeof(ebuf)) == -1) {
			info->error = errno;
			(*info->logger)(LOG_ERR,
			    "line %d:%d: error in \"%s\" element data"
			    " \"%s\": %s",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    name, frame->s == NULL ? "" : frame->s, ebuf);
			return;
		}
		goto done;
	default:
		break;
	}

	/* There shouldn't be any non-whitespace text here */
	if (frame->s != NULL) {
		for (s = frame->s; *s != '\0' && isspace(*s); s++);
		if (*s != '\0') {
			(*info->logger)(LOG_ERR, "line %d:%d:"
			    " extra garbage within \"%s\" element",
			    XML_GetCurrentLineNumber(info->p),
			    XML_GetCurrentColumnNumber(info->p),
			    name);
			info->error = EINVAL;
			return;
		}
	}

done:
	/* Pop stack frame */
	structs_xml_pop(info);
}

/*
 * Pop the XML parse stack
 */
static void
structs_xml_pop(struct xml_input_info *info)
{
	struct xmlinput_stackframe *const frame = &info->stack[info->depth];

	assert(info->depth >= 0);
	if (frame->s != NULL)
		FREE(CHARDATA_MEM_TYPE, frame->s);
	memset(frame, 0, sizeof(*frame));
	info->depth--;
}

/*********************************************************************
			XML OUTPUT ROUTINES
*********************************************************************/

#define STRUCTS_XML_SHOWONE	0x0100	/* show elem for next level only */
#define STRUCTS_XML_SHOWALL	0x0200	/* show elem and all sub elems */

/*
 * Internal functions
 */
static int	structs_xml_output_sub(const struct structs_type *type,
			const void *data, const char *tag, const char *attrs,
			FILE *fp, const char **elems, const char *posn,
			int flags, int depth);
static void	structs_xml_output_openelem(FILE *fp,
			int depth, const char *tag, const char *attrs);

/*
 * Output a structure in XML
 *
 * Note: it is safe for the calling thread to be canceled.
 */
int
structs_xml_output(const struct structs_type *type, const char *elem_tag,
	const char *attrs, const void *data, FILE *fp, const char **elems,
	int flags)
{
	static const char *all[] = { "", NULL };

	/* Output standard XML header */
	fputs(XML_HEADER, fp);

	/* NULL elems list means "show everything" */
	if (elems == NULL)
		elems = all;

	/* Output structure, and always show the opening and closing tags */
	return (structs_xml_output_sub(type, data, elem_tag, attrs, fp,
	    elems, "", flags | STRUCTS_XML_SHOWONE, 0));
}

/*
 * Output a sub-structure in XML
 */
static int
structs_xml_output_sub(const struct structs_type *type, const void *data,
	const char *tag, const char *attrs, FILE *fp, const char **elems,
	const char *posn, int flags, int depth)
{
	int r = 0;
	int i;

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Determine whether to show this element */
	if ((flags & STRUCTS_XML_SHOWALL) == 0) {
		const size_t plen = strlen(posn);

		for (i = 0; elems[i] != NULL; i++) {
			if (strncmp(elems[i], posn, plen) == 0
			    && (elems[i][plen] == '\0'
			      || elems[i][plen] == STRUCTS_SEPARATOR)) {
				if (elems[i][plen] == '\0')
					flags |= STRUCTS_XML_SHOWALL;
				break;
			}
		}
		if (elems[i] == NULL && depth > 0
		    && (flags & STRUCTS_XML_SHOWONE) == 0)
			return (0);		/* not matched, skip element */
	}

	/* If doing abbreviated version, compare with default value */
	if (depth > 0
	    && (flags & (STRUCTS_XML_FULL|STRUCTS_XML_SHOWONE)) == 0) {
		void *init_value;
		int equal;

		if ((init_value = MALLOC(TYPED_MEM_TEMP, type->size)) == NULL)
			return (-1);
		if ((*type->init)(type, init_value) == -1) {
			FREE(TYPED_MEM_TEMP, init_value);
			return (-1);
		}
		equal = (*type->equal)(type, data, init_value);
		(*type->uninit)(type, init_value);
		FREE(TYPED_MEM_TEMP, init_value);
		if (equal)
			return (0);
	}

	/* The STRUCTS_XML_SHOWONE flag only applies to the next level down */
	flags &= ~STRUCTS_XML_SHOWONE;

	/* Output element */
	switch (type->tclass) {
	case STRUCTS_TYPE_UNION:
	    {
		const struct structs_union *const un = data;
		const struct structs_ufield *const fields = type->args[0].v;
		const struct structs_ufield *field;
		char *sposn;

		/* Find field */
		for (field = fields; field->name != NULL
		    && strcmp(un->field_name, field->name) != 0; field++);
		if (field->name == NULL)
			assert(0);

		/* Generate new position tag */
		ASPRINTF(OUTPUT_MEM_TYPE, &sposn, "%s%s%s", posn _
		    *posn != '\0' ? separator_string : "" _ field->name);
		if (sposn == NULL)
			return (-1);
		pthread_cleanup_push(structs_xml_output_cleanup, sposn);

		/* Opening tag */
		structs_xml_output_openelem(fp, depth, tag, attrs);
		fprintf(fp, "\n");

		/*
		 * If the union field is not the default choice for this union,
		 * then it must always be shown so the recipient knows that.
		 */
		if (strcmp(un->field_name, fields[0].name) != 0)
			flags |= STRUCTS_XML_SHOWONE;

		/* Output chosen union field */
		r = structs_xml_output_sub(field->type, un->un, field->name,
		    NULL, fp, elems, sposn, flags, depth + 1);

		/* Free position tag */
		pthread_cleanup_pop(1);

		/* Bail out if there was an error */
		if (r == -1)
			break;

		/* Closing tag */
		structs_xml_output_prefix(fp, depth);
		fprintf(fp, "</%s>\n", tag);
		break;
	    }

	case STRUCTS_TYPE_STRUCTURE:
	    {
		const struct structs_field *field;

		/* Opening tag */
		structs_xml_output_openelem(fp, depth, tag, attrs);
		fprintf(fp, "\n");

		/* Do each structure field */
		for (field = type->args[0].v; field->name != NULL; field++) {
			char *sposn;

			/* Generate new position tag */
			ASPRINTF(OUTPUT_MEM_TYPE, &sposn, "%s%s%s", posn _
			    *posn != '\0' ? separator_string : "" _ field->name);
			if (sposn == NULL)
				return (-1);
			pthread_cleanup_push(structs_xml_output_cleanup, sposn);

			/* Do structure field */
			r = structs_xml_output_sub(field->type,
			    (char *)data + field->offset, field->name, NULL,
			    fp, elems, sposn, flags, depth + 1);

			/* Free position tag */
			pthread_cleanup_pop(1);

			/* Bail out if there was an error */
			if (r == -1)
				break;
		}

		/* Closing tag */
		structs_xml_output_prefix(fp, depth);
		fprintf(fp, "</%s>\n", tag);
		break;
	    }

	case STRUCTS_TYPE_ARRAY:
	    {
		const struct structs_type *const etype = type->args[0].v;
		const char *elem_name = type->args[2].s;
		const struct structs_array *const ary = data;
		int i;

		/* Opening tag */
		structs_xml_output_openelem(fp, depth, tag, attrs);
		fprintf(fp, "\n");

		/* All array elements must be shown to keep proper ordering */
		flags |= STRUCTS_XML_SHOWONE;

		/* Do elements in order */
		for (i = 0; i < ary->length; i++) {
			char *sposn;

			/* Generate new position tag */
			ASPRINTF(OUTPUT_MEM_TYPE, &sposn, "%s%s%u",
			    posn _ *posn != '\0' ? separator_string : "" _ i);
			if (sposn == NULL)
				return (-1);
			pthread_cleanup_push(structs_xml_output_cleanup, sposn);

			/* Output array element */
			r = structs_xml_output_sub(etype, (char *)ary->elems
			    + (i * etype->size), elem_name, NULL, fp, elems,
			    sposn, flags, depth + 1);

			/* Free position tag */
			pthread_cleanup_pop(1);

			/* Bail out if there was an error */
			if (r == -1)
				break;
		}

		/* Closing tag */
		structs_xml_output_prefix(fp, depth);
		fprintf(fp, "</%s>\n", tag);
		break;
	    }

	case STRUCTS_TYPE_FIXEDARRAY:
	    {
		const struct structs_type *const etype = type->args[0].v;
		const char *elem_name = type->args[1].s;
		const u_int length = type->args[2].i;
		u_int i;

		/* Opening tag */
		structs_xml_output_openelem(fp, depth, tag, attrs);
		fprintf(fp, "\n");

		/* All array elements must be shown to keep proper ordering */
		flags |= STRUCTS_XML_SHOWONE;

		/* Do elements in order */
		for (i = 0; i < length; i++) {
			char *sposn;

			/* Generate new position tag */
			ASPRINTF(OUTPUT_MEM_TYPE, &sposn, "%s%s%u",
			    posn _ *posn != '\0' ? separator_string : "" _ i);
			if (sposn == NULL)
				return (-1);
			pthread_cleanup_push(structs_xml_output_cleanup, sposn);

			/* Output array element */
			r = structs_xml_output_sub(etype, (char *)data
			    + (i * etype->size), elem_name, NULL, fp, elems,
			    sposn, flags, depth + 1);

			/* Free position tag */
			pthread_cleanup_pop(1);

			/* Bail out if there was an error */
			if (r == -1)
				break;
		}

		/* Closing tag */
		structs_xml_output_prefix(fp, depth);
		fprintf(fp, "</%s>\n", tag);
		break;
	    }

	case STRUCTS_TYPE_PRIMITIVE:
	    {
		char *ascii;

		/* Get ascii string */
		if ((ascii = (*type->ascify)(type,
		    OUTPUT_MEM_TYPE, data)) == NULL)
			return (-1);

		/* Push cleanup hook to handle cancellation */
		pthread_cleanup_push(structs_xml_output_cleanup, ascii);

		/* Output element */
		structs_xml_output_openelem(fp, depth, tag, attrs);
		structs_xml_encode(fp, ascii);
		fprintf(fp, "</%s>\n", tag);

		/* Free ascii string */
		pthread_cleanup_pop(1);
		break;
	    }

	default:
		assert(0);
	}
	return (r);
}

/*
 * Cleanup for structs_xml_output_sub()
 */
static void
structs_xml_output_cleanup(void *arg)
{
	FREE(OUTPUT_MEM_TYPE, arg);
}

/*
 * Output opening element tag with optional attributes
 */
static void
structs_xml_output_openelem(FILE *fp,
	int depth, const char *tag, const char *attrs)
{
	structs_xml_output_prefix(fp, depth);
	fprintf(fp, "<%s", tag);
	if (attrs != NULL) {
		while (*attrs != '\0') {
			fprintf(fp, " ");
			structs_xml_encode(fp, attrs);
			attrs += strlen(attrs) + 1;
			fprintf(fp, "=\"");
			structs_xml_encode(fp, attrs);
			attrs += strlen(attrs) + 1;
			fprintf(fp, "\"");
		}
	}
	fprintf(fp, ">");
}

/*********************************************************************
			UTILITY STUFF
*********************************************************************/

/*
 * Output whitespace prefix
 */
static void
structs_xml_output_prefix(FILE *fp, int depth)
{
	while (depth >= 2) {
		putc('\t', fp);
		depth -= 2;
	}
	if (depth > 0) {
		fputs("    ", fp);
	}
}

/*
 * Output text XML-encoded
 */
static void
structs_xml_encode(FILE *fp, const char *s)
{
	for ( ; *s != '\0'; s++) {
		switch (*s) {
		case '<':
			fprintf(fp, "&lt;");
			break;
		case '>':
			fprintf(fp, "&gt;");
			break;
		case '"':
			fprintf(fp, "&quot;");
			break;
		case '&':
			fprintf(fp, "&amp;");
			break;
			break;
		default:
			if (!isprint(*s)) {
				fprintf(fp, "&#%d;", (u_char)*s);
				break;
			}
			/* fall through */
		case '\n':
		case '\t':
			putc(*s, fp);
			break;
		}
	}
}

/*********************************************************************
			BUILT-IN LOGGERS
*********************************************************************/

static void
structs_xml_null_logger(int sev, const char *fmt, ...)
{
}

static void
structs_xml_stderr_logger(int sev, const char *fmt, ...)
{
	static const char *const sevs[] = {
		"emerg", "alert", "crit", "err",
		"warning", "notice", "info", "debug"
	};
	static const int num_sevs = sizeof(sevs) / sizeof(*sevs);
	va_list args;

	va_start(args, fmt);
	if (sev < 0)
		sev = 0;
	if (sev >= num_sevs)
		sev = num_sevs - 1;
	fprintf(stderr, "%s: ", sevs[sev]);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

static void
structs_xml_alog_logger(int sev, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	valog(sev, fmt, args);
	va_end(args);
}

/*********************************************************************
			MEMORY WRAPPERS
*********************************************************************/

#define EXPAT_MEM_TYPE			"structs_xml_input.expat"

static void *
structs_xml_malloc(size_t size)
{
	return (MALLOC(EXPAT_MEM_TYPE, size));
}

static void *
structs_xml_realloc(void *ptr, size_t size)
{
	return (REALLOC(EXPAT_MEM_TYPE, ptr, size));
}

static void
structs_xml_free(void *ptr)
{
	FREE(EXPAT_MEM_TYPE, ptr);
}

