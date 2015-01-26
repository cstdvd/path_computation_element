
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_XML_H_
#define _PDEL_HTTP_XML_H_

struct http_client;
struct structs_type;
struct xmlrpc_compact_fault;

__BEGIN_DECLS

/*
 * Send structured message, wait for a reply, and return the reply.
 * If there was an error, NULL is returned and errno is set.
 *
 * Parameters:
 *
 *	client		HTTP client
 *	ip, port	Remote HTTP server
 *	https		Non-zero for SSL
 *	urlpath		Remote URL path (must begin with '/')
 *	username	HTTP auth username (or NULL)
 *	password	HTTP auth password (or NULL)
 *	ptag		XML document element tag for payload
 *	pattrs		Attributes for ptag tag, or NULL for none
 *	ptype		Payload type
 *	payload		Payload (or NULL to do a GET instead of a POST)
 *	pflags		Flags to structs_xml_output()
 *	rtag		XML document element tag for reply
 *	rattrsp		Pointer to string to hold reply attributes (if != NULL)
 *	rattrs_mtype	Allocation type for *rattrsp.
 *	rtype		Reply type
 *	reply		Reply data
 *	rflags		Flags to structs_xml_input()
 *	rlogger		XML logger
 *
 * This function returns zero on success, -1 on error.
 */
extern int	http_xml_send(struct http_client *client, struct in_addr ip,
			u_int16_t port, int https, const char *urlpath,
			const char *username, const char *password,
			const char *ptag, const char *pattrs,
			const struct structs_type *ptype, const void *payload,
			int pflags, const char *rtag, char **rattrsp,
			const char *rattrs_mtype,
			const struct structs_type *rtype, void *reply,
			int rflags, structs_xmllog_t *rlogger);

/*
 * Send XML-RPC message, wait for a reply, and return the reply.
 * If there was an error, NULL is returned and errno is set.
 *
 * Parameters:
 *
 *	client		HTTP client
 *	ip, port	Remote HTTP server
 *	https		Non-zero for SSL
 *	username	HTTP auth username (or NULL)
 *	password	HTTP auth password (or NULL)
 *	methodName	Name of XML-RPC method
 *	nparams		Number of parameters
 *	ptypes		Array of pointers to parameter types
 *	pdatas		Array of pointers to parameter structures
 *	rtype		Reply type (or NULL)
 *	reply		Uninitialized reply (or NULL)
 *	faultp		Set to non-NULL to indicate a fault
 *	rlogger		XML logger
 *
 * This function returns zero on success, -1 on a system error, or -2
 * if an XML-RPC fault was returned (in which case *fault, if not NULL, will
 * be initialized and filled in (so *fault should never be pre-initialized
 * but it should be freed via structs_free() if -2 is returned)).
 */
extern int	http_xml_send_xmlrpc(struct http_client *client,
			struct in_addr ip, u_int16_t port, int https,
			const char *username, const char *password,
			const char *methodName, u_int nparams,
			const struct structs_type **ptypes, const void **pdatas,
			const struct structs_type *rtype, void *reply,
			struct xmlrpc_compact_fault *faultp,
			structs_xmllog_t *rlogger);

/*
 * Same as http_xml_send_xmlrpc() but with user-defined URL path.
 */
extern int	http_xml_send_xmlrpc2(struct http_client *client,
			struct in_addr ip, u_int16_t port, int https,
			const char *username, const char *password,
			const char *urlpat, const char *methodName,
			u_int nparams, const struct structs_type **ptypes,
			const void **pdatas, const struct structs_type *rtype,
			void *reply, struct xmlrpc_compact_fault *faultp,
			structs_xmllog_t *rlogger);

__END_DECLS

#endif	/* _PDEL_HTTP_XML_H_ */
