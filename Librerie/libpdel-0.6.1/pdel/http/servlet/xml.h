
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_XML_H_
#define _PDEL_HTTP_SERVLET_XML_H_

struct http_request;
struct http_servlet_xml_info;

/*
 * User-supplied handler for http_servlet_xml. Should return the
 * reply structure (having type info->rtype), or NULL (and set errno)
 * if there was an error.
 *
 * Parameters
 *
 *	arg		User-supplied cookie
 *	payload		Incoming request structure (do not free)
 *	pattrs		Incoming request XML document element attributes
 *	rattrs		Reply XML document element attributes (if any)
 *	mtype		Memory type with which to allocate reply and *rattrsp
 */
typedef void	*http_servlet_xml_handler_t(void *arg,
			struct http_request *req, const void *payload,
			const char *pattrs, char **rattrsp, const char *mtype);

/*
 * Information required by http_servlet_xml_create().
 *
 *	handler		User-supplied handler function
 *	ptag		Request XML document element tag expected
 *	ptype		Payload type expected
 *	rtag		Reply XML document element tag
 *	rtype		Reply type returned by handler
 *	allow_post	Allow POST queries
 *	allow_get	Allow GET queries (payload and pattrs will be NULL)
 *	logger		Logging function
 *	flags		Flags for structs_xml_output()
 */
struct http_servlet_xml_info {
	http_servlet_xml_handler_t	*handler;	/* user handler */
	const char			*ptag;		/* payload doc elem */
	const struct structs_type	*ptype;		/* payload type */
	const char			*rtag;		/* reply doc elem */
	const struct structs_type	*rtype;		/* reply type */
	u_char				allow_post;	/* allow POST */
	u_char				allow_get;	/* allow GET */
	http_logger_t			*logger;	/* loggging function */
	int				flags;		/* output flags */
};

__BEGIN_DECLS

/*
 * Create a new XML servlet.
 */
extern struct	http_servlet *http_servlet_xml_create(
			const struct http_servlet_xml_info *info,
			void *arg, void (*destroy)(void *));

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_XML_H_ */

