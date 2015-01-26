 
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#ifndef __CYGWIN__
#include <net/ethernet.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/struct.h"
#include "structs/type/union.h"
#include "structs/type/string.h"
#include "structs/type/float.h"
#include "structs/type/boolean.h"
#include "structs/type/data.h"
#include "structs/type/time.h"
#include "structs/type/int.h"
#include "structs/xml.h"
#include "structs/xmlrpc.h"
#include "util/typed_mem.h"

#if 0
#define DEBUG_XMLRPC 1
#endif

#ifdef DEBUG_XMLRPC
int debug_recurse = 1;
#endif

/************************************************************************
				string
************************************************************************/

static const struct structs_type string_type
	= STRUCTS_STRING_TYPE("xmlrpc_string", 0);

/************************************************************************
				base64
************************************************************************/

static const struct structs_type base64_type
	= STRUCTS_DATA_TYPE(NULL, "xmlrpc_base64");

/************************************************************************
				array data
************************************************************************/

static const struct structs_type array_data_type
	= STRUCTS_ARRAY_TYPE(&structs_type_xmlrpc_value,
		"xmlrpc_array", "value");

/************************************************************************
				array
************************************************************************/

static const struct structs_field array_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_array, data, &array_data_type),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type structs_type_xmlrpc_array
	= STRUCTS_STRUCT_TYPE(xmlrpc_array, &array_fields);

/************************************************************************
				member
************************************************************************/

static const struct structs_field member_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_member, name, &string_type),
	STRUCTS_STRUCT_FIELD(xmlrpc_member, value, &structs_type_xmlrpc_value),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type structs_type_xmlrpc_member
	= STRUCTS_STRUCT_TYPE(xmlrpc_member, &member_fields);

/************************************************************************
				struct
************************************************************************/

const struct structs_type structs_type_xmlrpc_struct
	= STRUCTS_ARRAY_TYPE(&structs_type_xmlrpc_member,
		"xmlrpc_struct", "member");

/************************************************************************
				value
************************************************************************/

static const struct structs_ufield value_fields[] = {
	STRUCTS_UNION_FIELD(string, &string_type),
	STRUCTS_UNION_FIELD(i4, &structs_type_int32),
	STRUCTS_UNION_FIELD(int, &structs_type_int32),
	STRUCTS_UNION_FIELD(boolean, &structs_type_boolean_char_01),
	STRUCTS_UNION_FIELD(double, &structs_type_double),
	STRUCTS_UNION_FIELD(dateTime.iso8601, &structs_type_time_iso8601),
	STRUCTS_UNION_FIELD(base64, &base64_type),
	STRUCTS_UNION_FIELD(struct, &structs_type_xmlrpc_struct),
	STRUCTS_UNION_FIELD(array, &structs_type_xmlrpc_array),
	STRUCTS_UNION_FIELD_END
};
const struct structs_type structs_type_xmlrpc_value
	= STRUCTS_UNION_TYPE(xmlrpc_value, &value_fields);

/************************************************************************
				param
************************************************************************/

static const struct structs_field param_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_param, value, &structs_type_xmlrpc_value),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type param_type
	= STRUCTS_STRUCT_TYPE(xmlrpc_param, &param_fields);

/************************************************************************
				params
************************************************************************/

static const struct structs_type params_type
	= STRUCTS_ARRAY_TYPE(&param_type, "xmlrpc_params", "param");

/************************************************************************
				fault
************************************************************************/

static const struct structs_field fault_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_fault, value, &structs_type_xmlrpc_value),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type structs_type_xmlrpc_fault
	= STRUCTS_STRUCT_TYPE(xmlrpc_fault, &fault_fields);

/************************************************************************
				request
************************************************************************/

static const struct structs_field request_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_request, methodName, &string_type),
	STRUCTS_STRUCT_FIELD(xmlrpc_request, params, &params_type),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type structs_type_xmlrpc_request
	= STRUCTS_STRUCT_TYPE(xmlrpc_request, &request_fields);

/************************************************************************
				response
************************************************************************/

static const struct structs_ufield response_fields[] = {
	STRUCTS_UNION_FIELD(params, &params_type),
	STRUCTS_UNION_FIELD(fault, &structs_type_xmlrpc_fault),
	STRUCTS_UNION_FIELD_END
};
const struct structs_type structs_type_xmlrpc_response
	= STRUCTS_UNION_TYPE(xmlrpc_response, &response_fields);

/************************************************************************
			fault (compact form)
************************************************************************/

static const struct structs_type fault_string_type
	= STRUCTS_STRING_TYPE("xmlrpc_fault_string", 0);

static const struct structs_field compact_fault_fields[] = {
	STRUCTS_STRUCT_FIELD(xmlrpc_compact_fault,
	    faultCode, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(xmlrpc_compact_fault,
	    faultString, &fault_string_type),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type structs_type_xmlrpc_compact_fault
	= STRUCTS_STRUCT_TYPE(xmlrpc_compact_fault, &compact_fault_fields);

/************************************************************************
			Local Funcation Prototypes
************************************************************************/

static int
structs_xmlrpc_addnode(const struct structs_type *type,
		       const void *data, 
		       const char *name,
		       const struct structs_type *xtype,
		       struct xmlrpc_value_union **xdata);


/************************************************************************
				FUNCTIONS
************************************************************************/

/*
 * Create an XML-RPC request structure with the given parameters.
 */
struct xmlrpc_request *
structs_xmlrpc_build_request(const char *mtype, const char *methodName,
	u_int nparams, const struct structs_type **types, const void **datas)
{
	const struct structs_type *const xtype = &structs_type_xmlrpc_request;
	struct xmlrpc_request *xreq;
	u_int i;

	/* Create new XML-RPC request structure */
	if ((xreq = MALLOC(mtype, sizeof(*xreq))) == NULL)
		return (NULL);
	if (structs_init(xtype, NULL, xreq) == -1) {
		FREE(mtype, xreq);
		return (NULL);
	}

	/* Set method name */
	if (structs_set_string(xtype,
	    "methodName", methodName, xreq, NULL, 0) == -1)
		goto fail;

	/* Copy parameters into request */
	for (i = 0; i < nparams; i++) {
		const void *const data = datas[i];
		char xname[32];

		/* Add another element to params array */
		if (structs_array_insert(xtype, "params", i, xreq) == -1)
			goto fail;
		snprintf(xname, sizeof(xname), "params.%u.value", i);

		/* Explode parameter unless already exploded */
		if (types != NULL) {
			if (structs_struct2xmlrpc(types[i], data, NULL,
			    xtype, xreq, xname) == -1)
				goto fail;
		} else {
			if (structs_set(&structs_type_xmlrpc_value,
			    data, xname, xreq) == -1)
				goto fail;
		}
	}

	/* Done */
	return (xreq);

fail:
	structs_free(xtype, NULL, xreq);
	FREE(mtype, xreq);
	return (NULL);
}

/*
 * Create an XML-RPC response structure with the given return value.
 */
struct xmlrpc_response_union *
structs_xmlrpc_build_response(const char *mtype,
	const struct structs_type *type, const void *data)
{
	const struct structs_type *const xtype = &structs_type_xmlrpc_response;
	struct xmlrpc_response_union *xrep;

	/* Create new XML-RPC response structure */
	if ((xrep = MALLOC(mtype, sizeof(*xrep))) == NULL)
		return (NULL);
	if (structs_init(xtype, NULL, xrep) == -1) {
		FREE(mtype, xrep);
		return (NULL);
	}

	/* Add a single parameter */
	if (structs_array_insert(xtype, "params", 0, xrep) == -1)
		goto fail;

	/* Copy parameter, optionally compacting it */
	if (type != NULL) {
		if (structs_struct2xmlrpc(type, data,
		    NULL, xtype, xrep, "params.0.value") == -1)
			goto fail;
	} else {
		if (structs_set(&structs_type_xmlrpc_value,
		    data, "params.0.value", xrep) == -1)
			goto fail;
	}

	/* Done */
	return (xrep);

fail:
	structs_free(xtype, NULL, xrep);
	FREE(mtype, xrep);
	return (NULL);
}

/*
 * Create an XML-RPC fault response structure with the given fault.
 */
struct xmlrpc_response_union *
structs_xmlrpc_build_fault_response(const char *mtype,
	const struct xmlrpc_compact_fault *fault)
{
	const struct structs_type *const xtype = &structs_type_xmlrpc_response;
	struct xmlrpc_response_union *xrep;

	/* Create new XML-RPC response structure */
	if ((xrep = MALLOC(mtype, sizeof(*xrep))) == NULL)
		return (NULL);
	if (structs_init(xtype, NULL, xrep) == -1) {
		FREE(mtype, xrep);
		return (NULL);
	}

	/* Set fault */
	if (structs_struct2xmlrpc(&structs_type_xmlrpc_compact_fault,
	    fault, NULL, xtype, xrep, "fault.value") == -1)
		goto fail;

	/* Done */
	return (xrep);

fail:
	structs_free(xtype, NULL, xrep);
	FREE(mtype, xrep);
	return (NULL);
}

/*
 * Copy the contents of a normal structure into an XML-RPC "value" structure.
 */
int
structs_struct2xmlrpc(const struct structs_type *type,
	const void *data, const char *sname,
	const struct structs_type *xtype, void *xdata, const char *xname)
{
	/* Find source and dest data */
	if ((type = structs_find(type, sname, (const void **) &data, 0)) == NULL) {
#ifdef DEBUG_XMLRPC
		if (debug_recurse) {
			fprintf(stderr, "%s: structs_find(type, %s) FAILED\n",
				__FUNCTION__, PDSAFESTR(sname));
		}
#endif
		return (-1);
	}
	if ((xtype = structs_find(xtype, xname, (const void **) &xdata, 1)) == NULL) {
#ifdef DEBUG_XMLRPC
		if (debug_recurse) {
			fprintf(stderr, "%s: structs_find(xtype, %s) FAILED\n",
				__FUNCTION__, PDSAFESTR(xname));
		}
#endif
		return (-1);
	}

	/* Destination type must always be an XML-RPC value */
	if (xtype != &structs_type_xmlrpc_value) {
		errno = EINVAL;
		return (-1);
	}

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

#ifdef DEBUG_XMLRPC
	if (debug_recurse) {
	   fprintf(stderr, 
		   "%s: tname=%s, xtname=%s, sname=%s, xname=%s, "
		   "data=%p, xdata=%p\n",
		   __FUNCTION__, 
		   PDSAFESTR(type->name), PDSAFESTR(xtype->name),
		   PDSAFESTR(sname), PDSAFESTR(xname),
		   PDSAFESTR(data), PDSAFESTR(xdata));
	}
#endif

	switch (type->tclass) {
	case STRUCTS_TYPE_PRIMITIVE:
	    {
		const char *xprim;
		char *s;

		/* 
		 * Get corresponding XML-RPC primitive type.
		 *
		 * We use int as the fallback for an ordinal numeric type.
		 *
		 * We forcibly coerce any type that is an int and smaller
		 * than or equal to the size of int32 to an i4.
		 * This is slightly sub-optimal for a 32 bit unsigned, but
		 * the results will be more likely correct and useful than
		 * a base64 set of bytes.
		 */

		if (strcmp(type->name, "float") == 0
		    || strcmp(type->name, "double") == 0)
			xprim = "double";
		else if (strcmp(type->name, "boolean") == 0)
			xprim = "boolean";
		else if (((0 == strcmp(type->name, "int")) ||
			  (0 == strcmp(type->name, "uint")) ||
			  (0 == strcmp(type->name, "hint"))) &&
			 type->size <= 4)
			xprim = "i4";
		else if (type == &structs_type_time_iso8601)
			xprim = "dateTime.iso8601";
		else if (strcmp(type->name, "data") == 0
		    && type->args[0].v == NULL)		/* XXX same charmap */
			xprim = "base64";
		else
			xprim = "string";

		/* Get primitive value as a string */
		if ((s = structs_get_string(type,
		    NULL, data, TYPED_MEM_TEMP)) == NULL)
			return (-1);

		/* Set primitive value as a string */
		if (structs_set_string(xtype,
		    xprim, s, xdata, NULL, 0) == -1) {
			FREE(TYPED_MEM_TEMP, s);
			return (-1);
		}
		FREE(TYPED_MEM_TEMP, s);
		break;
	    }
	case STRUCTS_TYPE_ARRAY:
	    {
		const struct structs_type *const etype = type->args[0].v;
		const struct structs_array *const ary = data;
		u_int i;

		/* Reset destination value to be an empty array */
		if (structs_array_reset(xtype, XML_RPC_ARRAY_DTYPE, 
					xdata) == -1)
			return (-1);

		/* Copy over each element in the array */
		for (i = 0; i < ary->length; i++) {
			char buf[32];
#ifdef DEBUG_XMLRPC
			if (debug_recurse) {
				fprintf(stderr, "\t\tARRAY [%d]\n", i);
			}
#endif
			/* Add a new element to the dest array */
			if (structs_array_insert(xtype,
			    XML_RPC_ARRAY_DTYPE, i, xdata) == -1)
				return (-1);

			/* Set its value */
			snprintf(buf, sizeof(buf), "array.data.%u", i);
			if (structs_struct2xmlrpc(etype,
			    (char *)ary->elems + (i * etype->size), NULL,
			    xtype, xdata, buf) == -1)
				return (-1);
		}
		break;
	    }
	case STRUCTS_TYPE_FIXEDARRAY:
	    {
		const struct structs_type *const etype = type->args[0].v;
		const u_int length = type->args[2].i;
		u_int i;

		/* Reset destination value to be an empty array */
		if (structs_array_reset(xtype, XML_RPC_ARRAY_DTYPE, 
					xdata) == -1)
			return (-1);

		/* Copy over each element in the array */
		for (i = 0; i < length; i++) {
			char buf[32];

#ifdef DEBUG_XMLRPC
			if (debug_recurse) {
				fprintf(stderr, "\t\tFIXEDARRAY [%d]\n", i);
			}
#endif
			/* Add a new element to the dest array */
			if (structs_array_insert(xtype,
			    XML_RPC_ARRAY_DTYPE, i, xdata) == -1)
				return (-1);

			/* Set its value */
			snprintf(buf, sizeof(buf), "array.data.%u", i);
			if (structs_struct2xmlrpc(etype,
			    (char *)data + (i * etype->size), NULL,
			    xtype, xdata, buf) == -1)
				return (-1);
		}
		break;
	    }
	case STRUCTS_TYPE_STRUCTURE:
	    {
		const struct structs_field *field = type->args[0].v;
		u_int i;

		/* Reset destination value to be an empty struct,
		   which really means just an empty array of members. */
		if (structs_array_reset(xtype, XML_RPC_STRUCT_TYPE,
					xdata) == -1)
			return (-1);

		/* Copy over each field in the structure to the member array */
		for (i = 0; field->name != NULL; i++, field++) {
			char buf[32];

#ifdef DEBUG_XMLRPC
			if (debug_recurse) {
				fprintf(stderr, "\t\tSTRUCTURE ->%s\n", 
					field->name);
			}
#endif
			/* Add a new element to the member array */
			if (structs_array_insert(xtype,
			    XML_RPC_STRUCT_TYPE, i, xdata) == -1)
				return (-1);

			/* Set the new member's name */
			snprintf(buf, sizeof(buf), "struct.%u.name", i);
			if (structs_set_string(xtype,
			    buf, field->name, xdata, NULL, 0) == -1)
				return (-1);

			/* Set the new member's value */
			snprintf(buf, sizeof(buf), "struct.%u.value", i);
			if (structs_struct2xmlrpc(field->type,
			    (char *)data + field->offset, NULL,
			    xtype, xdata, buf) == -1)
				return (-1);
		}
		break;
	    }
	case STRUCTS_TYPE_UNION:
	    {
		const struct structs_ufield *const fields = type->args[0].v;
		const struct structs_union *const un = data;
		const struct structs_ufield *field;

		/* Reset destination value to be an empty struct,
		   which really means just an empty array of members. */
		if (structs_array_reset(xtype, XML_RPC_STRUCT_TYPE, 
					xdata) == -1)
			return (-1);

		/* Find field */
		for (field = fields; field->name != NULL
		    && strcmp(un->field_name, field->name) != 0; field++);
		if (field->name == NULL) {
			assert(0);
			errno = EINVAL;
			return (-1);
		}

		/* Add a new element to the member array */
		if (structs_array_insert(xtype, XML_RPC_STRUCT_TYPE, 0, 
					 xdata) == -1)
			return (-1);

		/* Set the new member's name */
		if (structs_set_string(xtype, "struct.0.name",
		    field->name, xdata, NULL, 0) == -1)
			return (-1);

		/* Set the new member's value */
		if (structs_struct2xmlrpc(field->type,
		    un->un, NULL, xtype, xdata, "struct.0.value") == -1)
			return (-1);
		break;
	    }
	default:
		assert(0);
		return (-1);
	}
	return (0);
}

/*
 * Copy the contents of an XML-RPC "value" structure into a normal structure.
 */
int
structs_xmlrpc2struct(const struct structs_type *xtype, const void *xdata,
	const char *xname, const struct structs_type *type,
	void *data, const char *sname, char *ebuf, size_t emax)
{
	const struct structs_union *xun;

	/* Safety */
	snprintf(ebuf, emax, "Unknown error");

	/* Find source and dest data */
	if ((xtype = structs_find(xtype, xname, (const void **) &xdata, 0)) == NULL)
		goto fail_errno;
	if ((type = structs_find(type, sname, (const void **) &data, 1)) == NULL)
		goto fail_errno;

	/* Source type must always be an XML-RPC value */
	if (xtype != &structs_type_xmlrpc_value) {
		snprintf(ebuf, emax, "xtype != &structs_type_xmlrpc_value");
		errno = EINVAL;
		return (-1);
	}
	assert (xtype->tclass == STRUCTS_TYPE_UNION);

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Get union info */
	xun = xdata;

	/* Check which union field is in use and transfer accordingly */
	if (strcmp(xun->field_name, "i4") == 0
	    || strcmp(xun->field_name, "int") == 0
	    || strcmp(xun->field_name, "boolean") == 0
	    || strcmp(xun->field_name, "double") == 0
	    || strcmp(xun->field_name, "string") == 0
	    || strcmp(xun->field_name, "dateTime.iso8601") == 0
	    || strcmp(xun->field_name, "base64") == 0) {
		const char *xfname = xun->field_name;
		char *s;

		if ((s = structs_get_string(xtype,
		    xfname, xdata, TYPED_MEM_TEMP)) == NULL)
			goto fail_errno;
		if (structs_set_string(type, NULL, s, data, ebuf, emax) == -1) {
			FREE(TYPED_MEM_TEMP, s);
			return (-1);
		}
		FREE(TYPED_MEM_TEMP, s);
	} else if (strcmp(xun->field_name, XML_RPC_STRUCT_TYPE) == 0) {
		int is_struct;
		u_int len;
		u_int i;

		/* Check destination type */
		if (type->tclass != STRUCTS_TYPE_STRUCTURE
		    && type->tclass != STRUCTS_TYPE_UNION) {
			snprintf(ebuf, emax, "can't convert XML-RPC struct");
			errno = EINVAL;
			return (-1);
		}
		is_struct = (type->tclass == STRUCTS_TYPE_STRUCTURE);

		/* Copy structure members */
		if ((len = structs_array_length(xtype, XML_RPC_STRUCT_TYPE, 
						xdata)) == -1)
			goto fail_errno;
		for (i = 0; i < len; i++) {
			const struct structs_type *ftype = type;
			void *fdata = data;
			char buf[32];
			char *mname;

			/* Get member field name */
			snprintf(buf, sizeof(buf), "struct.%u.name", i);
			if ((mname = structs_get_string(xtype,
			    buf, xdata, TYPED_MEM_TEMP)) == NULL)
				goto fail_errno;

			/* Find field in struct or union */
			if ((ftype = structs_find(type,
			    mname, (const void **) &fdata, 1)) == NULL) {
				if (errno != ENOENT) {
					FREE(TYPED_MEM_TEMP, mname);
					goto fail_errno;
				}
				snprintf(ebuf, emax, "unknown %s field \"%s\"",
				    is_struct ? "struct" : "union", mname);
				FREE(TYPED_MEM_TEMP, mname);
				errno = ENOENT;
				return (-1);
			}
			FREE(TYPED_MEM_TEMP, mname);

			/* Copy over value */
			snprintf(buf, sizeof(buf), "struct.%u.value", i);
			if (structs_xmlrpc2struct(xtype, xdata, buf,
			    ftype, fdata, NULL, ebuf, emax) == -1)
				return (-1);
		}
	} else if (strcmp(xun->field_name, XML_RPC_ARRAY_TYPE) == 0) {
		const struct structs_type *const etype = type->args[0].v;
		struct structs_array *const ary = data;
		const u_int length = type->args[2].i;	/* if fixedarray */
		const struct structs_type *xatype;
		const struct structs_array *xary;
		int fixed;
		u_int i;

		/* Check destination type */
		if (type->tclass != STRUCTS_TYPE_ARRAY
		    && type->tclass != STRUCTS_TYPE_FIXEDARRAY) {
			snprintf(ebuf, emax, "can't convert XML-RPC array");
			errno = EINVAL;
			return (-1);
		}
		fixed = (type->tclass == STRUCTS_TYPE_FIXEDARRAY);

		/* Dig for the actual source array */
		xary = xdata;
		if ((xatype = structs_find(xtype,
		    XML_RPC_ARRAY_DTYPE, (const void **)&xary, 0)) == NULL)
			goto fail_errno;

		/* Reset destination array */
		if (structs_reset(type, NULL, data) == -1)
			goto fail_errno;

		/* Check length for fixed arrays */
		if (fixed && xary->length > length) {
			snprintf(ebuf, emax, "XML-RPC array is too long");
			errno = EDOM;
			return (-1);
		}

		/* Copy over each element in the array */
		for (i = 0; i < xary->length; i++) {
			char buf[32];
			void *elem;

			/* Add/set new element in the dest array */
			if (fixed)
				elem = (char *)data + (i * etype->size);
			else {
				if (structs_array_insert(type,
				    NULL, i, data) == -1)
					goto fail_errno;
				elem = (char *)ary->elems + (i * etype->size);
			}

			/* Set its value */
			snprintf(buf, sizeof(buf), "%u", i);
			if (structs_xmlrpc2struct(xatype, xary, buf, etype,
			    elem, NULL, ebuf, emax) == -1)
				return (-1);
		}
	} else {
		snprintf(ebuf, emax, "unsupported XML-RPC type \"%s\"",
		    xun->field_name);
		errno = EINVAL;
		return (-1);
	}

	/* Done */
	return (0);

fail_errno:
	/* Return with error */
	snprintf(ebuf, emax, "%s", strerror(errno));
	return (-1);
}

/*
 * Extract multiple items (by name) from a master object returning an
 * array of the specified, fully qualified objects.  Useful for implementing
 * an RPC to fetch arbirtray items from a namespace.
 */
int
structs_structs2xmlrpcs(const struct structs_type *type,
			const void *data, 
			const struct cxmlrpc_namelist *names,
			const struct structs_type *xtype, 
			void *xdata, 
			u_int32_t flags)
{
	int	sidx;
	int	didx;
	struct cxmlrpc_namelist dname;
	const char *dnames[2];
	struct xmlrpc_value_union	*ddata = 
			(struct xmlrpc_value_union *) xdata;
	struct xmlrpc_value_union	*tmpdata;

	/* Shorthand - if names is NULL or 0 length map to array of 1 item of 
	   everything */

	if (names == NULL || names->length == 0) {
	   dname.length = 1;
		dnames[0] = "";
		dnames[1] = NULL;
		dname.elems = dnames;
		names = &dname;
	}

	/* Destination type must always be an XML-RPC value, NULL means 
	   do just that */
	if (xtype == NULL) {
		xtype = &structs_type_xmlrpc_value;
	} else {
		if (xtype != &structs_type_xmlrpc_value) {
			errno = EINVAL;
			return (-1);
		}
	}

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Make the value an array */
	if (-1 == structs_union_set(&structs_type_xmlrpc_value, NULL, ddata, 
				    XML_RPC_ARRAY_TYPE)) {
		return(-1);
	}

	/* Prebuild the array */
	if (-1 == structs_array_setsize(&structs_type_xmlrpc_value, 
					XML_RPC_ARRAY_DTYPE, 
					names->length, ddata, 1)) {
		return(-1);
	}

	/* Loop setting the individual fields */
	for (sidx = 0, didx = 0; sidx < names->length; sidx++) {
		char			buf[32];/* Scratch name	*/
		const structs_type	*etype;	/* Source structs_type	*/
		const void		*fdata = data;	/* "" data	*/

		/* We don't use etype but check for valid name */
		if (NULL == 
		    (etype = structs_find(type, names->elems[sidx], 
					  (const void **) &fdata, 0))) {
			if (flags & STRUCTS_XML_LOOSE) {
			   continue;
			}
			return(-1);
		}
		snprintf(buf, sizeof(buf), XML_RPC_ARRAY_DTYPE ".%u", didx);

		tmpdata = &ddata->un->array.elems[didx];
		if (-1 == structs_xmlrpc_addnode(type, data,
						 names->elems[sidx], 
						 xtype, &tmpdata)) {
		   return(-1);
		}
		/* Base name now becomes 0 as we used addnode */
		buf[0] = '\0';

		if (-1 == structs_struct2xmlrpc(type, data,
						names->elems[sidx], 
						xtype, tmpdata, buf)) {
			return(-1);
		}
		didx++;
	}
	return(0);
}

/*
 * Add the next intermediate child(ren) of a name as an XMLRPC node.  This
 * is used to encode a fully qualified namespace in the "exploded" XMLRPC
 * model.
 */
static int
structs_xmlrpc_addnode(const struct structs_type *type,
		       const void *data, 
		       const char *name,
		       const struct structs_type *xtype,
		       struct xmlrpc_value_union **xdata)
{
	const void	*fdata = data;
	struct xmlrpc_value_union	*tdata = 
			 *xdata;
	char		*walk;
	char		buf[32];
	char		obuf[255];
	int		recurse = 0;
	const structs_type	*ntype;
	const structs_type	*nxtype;

	/* If name is NULL or "" we are done */
	if (name == NULL || *name == '\0') {
		return(0);
	}

	/* Massage source name, if no more "." this is our last pass */
	strlcpy(obuf, name, sizeof(obuf));
	walk = strchr(obuf, '.');
	if (walk) {
		*xdata = tdata;
		recurse = 1;
    
		/* Terminate base name and advance walk to rest of name */
		*walk = '\0';
		walk++;
	}

	/* Find source and dest data */
	if ((ntype = structs_find(type, obuf, (const void **)&fdata, 0)) == NULL) {
#ifdef DEBUG_XMLRPC
		if (debug_recurse) {
			fprintf(stderr, "%s: structs_find(type, %s) FAILED\n",
				__FUNCTION__, PDSAFESTR(obuf));
		}
#endif
		return (-1);
	}

	if ((nxtype = structs_find(xtype, NULL, (const void **) &tdata, 1)) == NULL) {
#ifdef DEBUG_XMLRPC
		if (debug_recurse) {
			fprintf(stderr, "%s: structs_find(xtype, NULL) FAILED\n",
				__FUNCTION__);
		}
#endif
		return (-1);
	}

	/* Destination type must always be an XML-RPC value */
	if (nxtype != &structs_type_xmlrpc_value) {
		errno = EINVAL;
		return (-1);
	}

#if 1
	/* Dereference through pointer(s) */
	while (ntype->tclass == STRUCTS_TYPE_POINTER) {
		ntype = ntype->args[0].v;
		fdata = *((void **)fdata);
	}
#endif

	/* We select based on the "current" structs type but pass the 
	   "next" type down for recursion */

	switch (type->tclass) {
	case STRUCTS_TYPE_PRIMITIVE:
	    {   
	       break;
	    }
	case STRUCTS_TYPE_FIXEDARRAY:
	case STRUCTS_TYPE_ARRAY:
	    {
		/* Reset destination value to be an empty array */
		if (structs_array_reset(xtype, XML_RPC_ARRAY_DTYPE, 
					tdata) == -1)
			return (-1);

		/* Add a new element to the dest array */
		if (structs_array_insert(xtype, XML_RPC_ARRAY_DTYPE,
					 0, tdata) == -1)
			return (-1);

		/* Move tdata/xdata to the new node */
		tdata = tdata->un->array.elems;

		/* And recurse for rest of name */
		if (-1 == structs_xmlrpc_addnode(ntype, fdata, walk, 
						 nxtype, &tdata))
			return(-1);
		break;
	    }
	case STRUCTS_TYPE_STRUCTURE:
	case STRUCTS_TYPE_UNION:
	    {
		/* Reset destination value to be an empty struct,
		   which really means just an empty array of members. */
		if (structs_array_reset(xtype, XML_RPC_STRUCT_TYPE, 
					tdata) == -1)
			return (-1);

		/* Add a new element to the dest array */
		if (structs_array_insert(xtype, XML_RPC_STRUCT_TYPE,
					 0, tdata) == -1)
			return (-1);

		/* Set the new member's name */
		snprintf(buf, sizeof(buf), "struct.%u.name", 0);
		if (structs_set_string(xtype, buf, obuf,
				       tdata, NULL, 0) == -1)
			return (-1);

		/* Move tdata/xdata to the new node */
		tdata = &tdata->un->struct_.elems->value;

		/* And recurse for rest of name */
		if (-1 == structs_xmlrpc_addnode(ntype, fdata, walk, 
						 nxtype, &tdata)) {
			return(-1);
		}
		break;
	    }
	}
	// Success

	*xdata = tdata;
	return(0);
}
