
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_XMLRPC_H_
#define _PDEL_STRUCTS_XMLRPC_H_

#define XML_RPC_URL		"/RPC2"
#define XML_RPC_REQUEST_TAG	"methodCall"
#define XML_RPC_REPLY_TAG	"methodResponse"
#define XML_RPC_METHOD_NAME_TAG	"methodName"
#define XML_RPC_ARRAY_TYPE	"array"
#define XML_RPC_STRUCT_TYPE	"struct"
#define XML_RPC_ARRAY_DTYPE	"array.data"

/* 
 * XML-RPC structure definitions
 */

DEFINE_STRUCTS_UNION(xmlrpc_value_union, xmlrpc_value);
DEFINE_STRUCTS_UNION(xmlrpc_response_union, xmlrpc_response);
DEFINE_STRUCTS_ARRAY(xmlrpc_value_array, struct xmlrpc_value_union);
DEFINE_STRUCTS_ARRAY(xmlrpc_param_array, struct xmlrpc_param);
DEFINE_STRUCTS_ARRAY(xmlrpc_struct, struct xmlrpc_member);
DEFINE_STRUCTS_ARRAY_T(xmlrpc_namelist, char *);
DEFINE_STRUCTS_CARRAY_T(cxmlrpc_namelist, char *);

/* Member (of a structure) */
struct xmlrpc_member {
	char				*name;
	struct xmlrpc_value_union	value;
};

/* Array */
struct xmlrpc_array {
	struct xmlrpc_value_array	data;
};

/* Parameter (for either a request or a response) */
struct xmlrpc_param {
	struct xmlrpc_value_union	value;
};

/* Value */
union xmlrpc_value {
	char				*string;
	int32_t				i4;
	int32_t				int_;
	u_char				boolean;
	double				double_;
	time_t				dateTime_iso8601;
	struct structs_data		base64;
	struct xmlrpc_value_array	array;
	struct xmlrpc_struct		struct_;
};

/* Fault */
struct xmlrpc_fault {
	struct xmlrpc_value_union	value;		/* see below */
};

/* Fault in compact form */
struct xmlrpc_compact_fault {
	int32_t		faultCode;
	char		*faultString;
};

/* Request */
struct xmlrpc_request {
	char				*methodName;
	struct xmlrpc_param_array	params;
};

/* Response */
union xmlrpc_response {
	struct xmlrpc_param_array	params;		/* params.length == 1 */
	struct xmlrpc_fault		fault;
};

__BEGIN_DECLS

/*
 * Type describing an XML-RPC value in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_value_union'.
 */
extern const	struct structs_type structs_type_xmlrpc_value;

/*
 * Type describing an XML-RPC array in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_array'.
 */
extern const	struct structs_type structs_type_xmlrpc_array;

/*
 * Type describing an XML-RPC member in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_member'.
 */
extern const	struct structs_type structs_type_xmlrpc_member;

/*
 * Type describing an XML-RPC struct in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_struct'.
 */
extern const	struct structs_type structs_type_xmlrpc_struct;

/*
 * Type describing an XML-RPC request in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_request'.
 */
extern const	struct structs_type structs_type_xmlrpc_request;

/*
 * Type describing an XML-RPC response in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_response_union'.
 */
extern const	struct structs_type structs_type_xmlrpc_response;

/*
 * Type describing an XML-RPC fault in XML-RPC exploded form.
 * This type describes a 'struct xmlrpc_fault'.
 */
extern const	struct structs_type structs_type_xmlrpc_fault;

/*
 * Type describing an XML-RPC fault in compact form.
 * This type describes a 'struct xmlrpc_compact_fault'.
 */
extern const	struct structs_type structs_type_xmlrpc_compact_fault;

/*
 * Create an XML-RPC methodCall structure with the given parameters.
 * The returned structure will be allocated with type "mtype"
 * and have type &structs_type_xmlrpc_request.
 *
 * The parameters should be specified as pairs: type, data.
 */
extern struct	xmlrpc_request *structs_xmlrpc_build_request(const char *mtype,
			const char *methodName, u_int nparams,
			const struct structs_type **types, const void **params);

/*
 * Create an XML-RPC methodResponse structure with the given return value.
 *
 * The returned structure will be allocated with type "mtype"
 * and have type &structs_type_xmlrpc_response.
 */
extern struct	xmlrpc_response_union *structs_xmlrpc_build_response(
			const char *mtype, const struct structs_type *type,
			const void *data);

/*
 * Create an XML-RPC methodResponse structure with the given fault.
 *
 * The returned structure will be allocated with type "mtype"
 * and have type &structs_type_xmlrpc_response.
 */
extern struct	xmlrpc_response_union *structs_xmlrpc_build_fault_response(
			const char *mtype,
			const struct xmlrpc_compact_fault *fault);

/*
 * Convert a normal structure into an XML-RPC value.
 */
extern int	structs_struct2xmlrpc(const struct structs_type *type,
			const void *data, const char *sname,
			const struct structs_type *xtype,
			void *xdata, const char *xname);

/*
 * Convert an XML-RPC value into a normal structure.
 */
extern int	structs_xmlrpc2struct(const struct structs_type *xtype,
			const void *xdata, const char *xname,
			const struct structs_type *type, void *data,
			const char *sname, char *ebuf, size_t emax);

/*
 * Extract multiple items (by name) from a master object returning an
 * array of the specified, fully qualified objects.  Useful for implementing
 * an RPC to fetch arbirtray items from a namespace.
 */
extern int	structs_structs2xmlrpcs(const struct structs_type *type,
			const void *data, 
			const struct cxmlrpc_namelist *names,
			const struct structs_type *xtype, 
			void *xdata, 
			u_int32_t flags);


__END_DECLS

#endif	/* _PDEL_STRUCTS_XMLRPC_H_ */

