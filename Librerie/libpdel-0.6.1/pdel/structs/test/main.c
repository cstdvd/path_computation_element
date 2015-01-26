
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */


#define USE_IPV6 1

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef NO_BPF
#include <net/bpf.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifndef NO_BPF
#include <pcap.h>
#endif
#include <err.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/xml.h>
#include <pdel/structs/types.h>
#include <pdel/structs/xmlrpc.h>
#include <pdel/structs/type/ip6.h>
#include <pdel/sys/alog.h>
#include <pdel/util/typed_mem.h>

#define ELEM_TAG		"structs_test"
#define XMLRPC_TAG		"value"
#define ATTR_MEM_TYPE		"attr_mem_type"

/* Our structures, etc */
struct array_elem {
	u_int16_t		elem_value;
	char			*elem_string;
	u_char			elem_ether[6];
	u_int32_t		elem_color;
	struct in_addr		elem_ip;
	float			elem_float;
	double			elem_double;
	u_char			elem_bytes[13];
	char			elem_bstring[13];
	struct structs_dnsname	elem_dnsname;
};

union ip_address {
	struct in_addr	ip4;
	struct in6_addr	ip6;
};

/* fields for union ip_address */
static const struct structs_ufield ip_address_fields[] = {
	STRUCTS_UNION_FIELD(ip4, &structs_type_ip4),
	STRUCTS_UNION_FIELD(ip6, &structs_type_ip6),
	STRUCTS_UNION_FIELD_END
};

/* structs type for union ip_address */
static const struct structs_type ip_address_type
	= STRUCTS_UNION_TYPE(ip_address, &ip_address_fields);

/* c type for union ip_address */
DEFINE_STRUCTS_UNION(ip_address_struct, ip_address);

struct prefix_ip {
	struct ip_address_struct ip_addr;
	u_int masklen;
};

static struct structs_field prefix_ip_fields[] = {
	STRUCTS_STRUCT_FIELD(prefix_ip, ip_addr, &ip_address_type),
	STRUCTS_STRUCT_FIELD(prefix_ip, masklen, &structs_type_uint),
	STRUCTS_STRUCT_FIELD_END
};

static const struct structs_type prefix_ip_type
	= STRUCTS_STRUCT_TYPE(prefix_ip, prefix_ip_fields);

union my_union {
	char 		*u_string;
	int64_t		u_int64;
	u_char		u_bool;
};

DEFINE_STRUCTS_UNION(ustruct, my_union);

#define INNER_STRARY_LEN	4

struct inner_struct {
	char			*inner_string;
	struct structs_data	inner_data;
	struct structs_data	inner_data2;
	struct structs_array	inner_array;
	struct array_elem	inner_struct;
	struct structs_array	inner_struct_ptr_array;
	u_char			*inner_ether;
	struct alog_config	inner_alog;
#ifndef NO_BPF
	struct structs_bpf	*inner_bpf;
#endif
	time_t			inner_time_gmt;
	time_t			inner_time_local;
	time_t			inner_time_iso8601;
	time_t			inner_time_rel;
	struct ustruct		inner_union;
	struct ustruct		inner_union2;
	struct ip_address_struct	ip6_mapped_from_ip4;
	struct ip_address_struct	ip6;
	struct ip_address_struct	ip4;
	struct prefix_ip	pfx_ip6;
	struct prefix_ip	pfx_ip4;
	struct structs_regex	inner_regex;
	char			*inner_strary[INNER_STRARY_LEN];
};

/* Type for field 'elem_color' */
static const struct structs_id color_list[] = {
	{ "Red", 1 }, { "Green", 2 }, { "Blue", 3 }, { NULL, 0 }
};
static const struct structs_type color_type
	= STRUCTS_ID_TYPE(color_list, sizeof(u_int32_t));

/* Type for field 'elem_bytes' */
static const struct structs_type elem_bytes_type
	= STRUCTS_FIXEDDATA_TYPE(sizeof(((struct array_elem *)0)->elem_bytes));

/* Type for field 'elem_bstring' */
static const struct structs_type elem_bstring_type
	= STRUCTS_FIXEDSTRING_TYPE(
	  sizeof(((struct array_elem *)0)->elem_bstring));

/* Type for struct array_elem */
static const struct structs_field array_elem_fields[] = {
	STRUCTS_STRUCT_FIELD(array_elem, elem_value, &structs_type_uint16),
	STRUCTS_STRUCT_FIELD(array_elem, elem_string, &structs_type_string),
	STRUCTS_STRUCT_FIELD(array_elem, elem_ether, &structs_type_ether),
	STRUCTS_STRUCT_FIELD(array_elem, elem_bstring, &elem_bstring_type),
	STRUCTS_STRUCT_FIELD(array_elem, elem_dnsname, &structs_type_dnsname),
	STRUCTS_STRUCT_FIELD(array_elem, elem_color, &color_type),
	STRUCTS_STRUCT_FIELD(array_elem, elem_ip, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(array_elem, elem_float, &structs_type_float),
	STRUCTS_STRUCT_FIELD(array_elem, elem_double, &structs_type_double),
	STRUCTS_STRUCT_FIELD(array_elem, elem_bytes, &elem_bytes_type),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type array_elem_type
	= STRUCTS_STRUCT_TYPE(array_elem, &array_elem_fields);
static const struct structs_type array_elem_ptr_type
	= STRUCTS_POINTER_TYPE(&array_elem_type, "array_elem_ptr");
/* Type for array of pointers to array_elems type */
static const struct structs_type array_elem_ptr_array_type
	= STRUCTS_ARRAY_TYPE(&array_elem_ptr_type,
	    "inner_struct_ptr", "inner_struct_ptr");

/* Type for array of array_elems type */
static const struct structs_type inner_array_type
	= STRUCTS_ARRAY_TYPE(&array_elem_type, "array_elem", "array_elem");

/* Type for pointer to Ethernet address */
static const struct structs_type ether_ptr_type
	= STRUCTS_POINTER_TYPE(&structs_type_ether_nocolon, "ether_ptr");

/* Type for union my_union */
static const struct structs_ufield my_union_fields[] = {
	STRUCTS_UNION_FIELD(u_string, &structs_type_string),
	STRUCTS_UNION_FIELD(u_int64, &structs_type_int64),
	STRUCTS_UNION_FIELD(u_bool, &structs_type_boolean_char),
	STRUCTS_UNION_FIELD_END
};
static const struct structs_type my_union_type
	= STRUCTS_UNION_TYPE(my_union, &my_union_fields);

/* Type for field "inner_strary" in struct inner_struct */
static const struct structs_type structs_type_strary
	= STRUCTS_FIXEDARRAY_TYPE(&structs_type_string,
	    sizeof(char *), INNER_STRARY_LEN, "strary_elem");

#ifndef NO_BPF
/* Type for field "inner_bpf" in struct inner_struct */
static structs_bpf_compile_t	bpf_compiler;

static const struct structs_type inner_bpf_type
	= BPF_STRUCTS_TYPE(DLT_EN10MB, bpf_compiler);
#endif

/* Type for struct inner_struct */
static const struct structs_field inner_struct_fields[] = {
	STRUCTS_STRUCT_FIELD(inner_struct, inner_string, &structs_type_string),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_data, &structs_type_data),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_data2, &structs_type_hexdata),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_array, &inner_array_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_struct, &array_elem_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_struct_ptr_array,
		&array_elem_ptr_array_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_ether, &ether_ptr_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_alog, &alog_config_type),
#ifndef NO_BPF
	STRUCTS_STRUCT_FIELD(inner_struct, inner_bpf, &inner_bpf_type),
#endif
	STRUCTS_STRUCT_FIELD(inner_struct, inner_time_gmt,
		&structs_type_time_gmt),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_time_local,
		&structs_type_time_local),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_time_iso8601,
		&structs_type_time_iso8601),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_time_rel,
		&structs_type_time_rel),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_union, &my_union_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_union2, &my_union_type),
	STRUCTS_STRUCT_FIELD(inner_struct, ip6_mapped_from_ip4, &ip_address_type),
	STRUCTS_STRUCT_FIELD(inner_struct, ip6, &ip_address_type),
	STRUCTS_STRUCT_FIELD(inner_struct, ip4, &ip_address_type),
	STRUCTS_STRUCT_FIELD(inner_struct, pfx_ip6, &prefix_ip_type),
	STRUCTS_STRUCT_FIELD(inner_struct, pfx_ip4, &prefix_ip_type),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_regex, &structs_type_regex),
	STRUCTS_STRUCT_FIELD(inner_struct, inner_strary, &structs_type_strary),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type inner_struct_type
	= STRUCTS_STRUCT_TYPE(inner_struct, &inner_struct_fields);

static int	structs_encode(const char *fname);
static int	structs_decode(const char *fname);

static struct	in_addr random_ip = { 0x04030201 };

static int	flags;

int
main(int argc, char *argv[])
{
	const struct structs_type *type = &inner_struct_type;
	struct inner_struct s, scopy;
	const char *name = "inner_array.1.elem_string";
	const char *value = "new value";
	const char *fname = NULL;
	char ebuf[128] = { '\0' };
	int xml2xmlrpc = 0;
	int normalize = 0;
	int nostats = 0;
	int coding = 0;
	void *xmlrpc;
	char *attrs;
	char *t;
	int ch;

	/* Parse command line arguments */
	while ((ch = getopt(argc, argv, "cdef:hnxlTt")) != -1) {
		switch (ch) {
		case 'c':
			flags |= STRUCTS_XML_COMB_TAGS;
			break;
		case 'd':
			coding = -1;
			break;
		case 'e':
			coding = 1;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'l':
			flags |= STRUCTS_XML_LOOSE;
			break;
		case 'n':
			normalize = 1;
			break;
		case 'x':
			nostats = 1;
			break;
		case 't':
			xml2xmlrpc = 1;
			break;
		case 'T':
			xml2xmlrpc = -1;
			break;
		case 'h':
		default:
usage:			errx(1, "usage:\n"
		"\tstructs_test [-xlc] < test.xml\n"
		"\tstructs_test -n [-xlc] < test.xml > test.xml\n"
		"\tstructs_test -t [-xlc] < test.xml > test.xmlrpc\n"
		"\tstructs_test -T [-x] < test.xmlrpc > test.xml\n"
		"\tstructs_test -e [-f field] [-xlc] < test.xml > test.bin\n"
		"\tstructs_test -d [-f field] [-xlc] < test.bin > test.xml");
			exit(3);
		}
	}
	argc -= optind;
	argv += optind;
	switch (argc) {
	case 0:
		break;
	default:
		goto usage;
	}

	if (typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	if (coding && normalize)
		goto usage;

	/* Binary encoding/decoding? */
	if (coding == 1) {
		structs_encode(fname);
		goto done;
	}
	if (coding == -1) {
		structs_decode(fname);
		goto done;
	}

	/* Convert between XML and XML-RPC? */
	if (xml2xmlrpc == 1) {
		const struct structs_type *const xtype
		    = &structs_type_xmlrpc_value;
		struct xmlrpc_value_union xvalue;

		if (structs_xml_input(type, ELEM_TAG, NULL,
		    NULL, stdin, &s, STRUCTS_XML_UNINIT | flags,
		    STRUCTS_LOGGER_STDERR) == -1)
			err(1, "structs_xml_input");
		if (structs_init(xtype, NULL, &xvalue) == -1)
			err(1, "structs_init");
		if (structs_struct2xmlrpc(type, &s, NULL,
		    xtype, &xvalue, NULL) == -1)
			err(1, "structs_struct2xmlrpc");
		structs_free(type, NULL, &s);
		if (structs_xml_output(xtype, XMLRPC_TAG, NULL,
		    &xvalue, stdout, NULL, 0) == -1)
			err(1, "structs_xml_output");
		structs_free(xtype, NULL, &xvalue);
		goto done;
	} else if (xml2xmlrpc == -1) {
		const struct structs_type *const xtype
		    = &structs_type_xmlrpc_value;
		struct xmlrpc_value_union xvalue;

		if (structs_xml_input(xtype, XMLRPC_TAG, NULL,
		    NULL, stdin, &xvalue, STRUCTS_XML_UNINIT,
		    STRUCTS_LOGGER_STDERR) == -1)
			err(1, "structs_xml_input");
		if (structs_init(type, NULL, &s) == -1)
			err(1, "structs_init");
		if (structs_xmlrpc2struct(xtype, &xvalue, NULL,
		    type, &s, NULL, ebuf, sizeof(ebuf)) == -1)
			errx(1, "structs_struct2xmlrpc: %s", ebuf);
		structs_free(xtype, NULL, &xvalue);
		if (structs_xml_output(type, ELEM_TAG, NULL,
		    &s, stdout, NULL, 0) == -1)
			err(1, "structs_xml_output");
		structs_free(type, NULL, &s);
		goto done;
	}

	/* Normalize XML? */
	if (normalize) {
		if (structs_xml_input(type, ELEM_TAG, NULL,
		    NULL, stdin, &s, STRUCTS_XML_UNINIT | flags,
		    STRUCTS_LOGGER_STDERR) == -1)
			err(1, "structs_xml_input");
		if (structs_xml_output(type, ELEM_TAG, NULL,
		    &s, stdout, NULL, 0) == -1)
			err(1, "structs_xml_output");
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Initializing struct...\n");
	if (structs_init(type, NULL, &s) == -1) {
		warn("structs_init");
		goto done;
	}

	printf(">>>>>>>> TEST: Dumping initialized struct...\n");
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &s, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Free'ing struct...\n");
	structs_free(type, NULL, &s);

	printf(">>>>>>>> TEST: Initializing struct...\n");
	if (structs_init(type, NULL, &s) == -1) {
		warn("structs_init");
		goto done;
	}

	printf(">>>>>>>> TEST: Setting %s to \"%s\"...\n",
	    "inner_union.u_int64", "0");
	if (structs_set_string(type, "inner_union.u_int64",
	    "0", &s, ebuf, sizeof(ebuf)) == -1) {
		warnx("structs_set_string: %s", ebuf);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Dumping struct, should show the union...\n");
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &s, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		structs_free(type, NULL, &s);
		goto done;
	}

    {
	const char *elems[] = { "inner_union", NULL };

	printf(">>>>>>>> TEST: Dumping only \"%s\", but full...\n", elems[0]);
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &s, stdout, elems, STRUCTS_XML_FULL) == -1) {
		warn("structs_xml_output");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &s);
		goto done;
	}
    }

	printf(">>>>>>>> TEST: Copying struct...\n");
	if (structs_get(type, NULL, &s, &scopy) == -1) {
		warn("structs_get");
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Free'ing both structs...\n");
	structs_free(type, NULL, &s);
	structs_free(type, NULL, &scopy);

	printf(">>>>>>>> TEST: Reading input...\n");
	if (structs_xml_input(type, ELEM_TAG, &attrs,
	    ATTR_MEM_TYPE, stdin, &s, STRUCTS_XML_UNINIT | flags,
	    STRUCTS_LOGGER_STDERR) == -1) {
		warn("structs_xml_input");
		goto done;
	}

	printf(">>>>>>>> TEST: Displaying attributes...\n");
	for (t = attrs; *t != '\0'; ) {
		printf("%30s =", t);
		t += strlen(t) + 1;
		printf(" %s\n", t);
		t += strlen(t) + 1;
	}

	printf(">>>>>>>> TEST: Dumping output...\n");
	if (structs_xml_output(type,
	    ELEM_TAG, attrs, &s, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &s);
		goto done;
	}

    {
	const char *elems[] = { "inner_struct", NULL };

	printf(">>>>>>>> TEST: Dumping only \"%s\" ...\n", elems[0]);
	if (structs_xml_output(type,
	    ELEM_TAG, attrs, &s, stdout, elems, 0) == -1) {
		warn("structs_xml_output");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &s);
		goto done;
	}
    }

    {
	const char *elems[] = {
	    "inner_struct_ptr_array.2.elem_bytes", NULL };

	printf(">>>>>>>> TEST: Dumping only \"%s\", but full...\n", elems[0]);
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &s, stdout, elems, STRUCTS_XML_FULL) == -1) {
		warn("structs_xml_output");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &s);
		goto done;
	}
    }

	printf(">>>>>>>> TEST: Copying struct...\n");
	if (structs_get(type, NULL, &s, &scopy) == -1) {
		warn("structs_get");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Dumping copy...\n");
	if (structs_xml_output(type,
	    ELEM_TAG, attrs, &scopy, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		FREE(ATTR_MEM_TYPE, attrs);
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}
	FREE(ATTR_MEM_TYPE, attrs);

	printf(">>>>>>>> TEST: Comparing original and copy...\n");
	printf("equal = %s\n",
	    structs_equal(type, NULL, &s, &scopy) ? "TRUE" : "FALSE");

	printf(">>>>>>>> TEST: Setting copy's %s to \"%s\"...\n", name, value);
	if (structs_set_string(type, name, value,
	    &scopy, ebuf, sizeof(ebuf)) == -1) {
		warnx("structs_set_string: %s", ebuf);
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Setting copy's %s to \"%s\"...\n",
	    "inner_union.u_int64", "1234567890");
	if (structs_set_string(type, "inner_union.u_int64",
	    "1234567890", &scopy, ebuf, sizeof(ebuf)) == -1) {
		warnx("structs_set_string: %s", ebuf);
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Setting copy's %s to \"%s\"...\n",
	    "inner_struct.elem_ip", inet_ntoa(random_ip));
	if (structs_set(type, &random_ip,
	    "inner_struct.elem_ip", &scopy) == -1) {
		warn("structs_set");
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Comparing original and copy...\n");
	printf("equal = %s\n",
	    structs_equal(type, NULL, &s, &scopy) ? "TRUE" : "FALSE");

	printf(">>>>>>>> TEST: Dumping copy...\n");
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &scopy, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}

	printf(">>>>>>>> TEST: Dumping copy in an XML-RPC request...\n");
    {
	const struct structs_type *types[1] = { type };
	const void *datas[1] = { &scopy };

	if ((xmlrpc = structs_xmlrpc_build_request(TYPED_MEM_TEMP,
	    "someMethodName", 1, types, datas)) == NULL) {
		warn("structs_xmlrpc_build_request");
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}
	if (structs_xml_output(&structs_type_xmlrpc_request,
	    "methodCall", NULL, xmlrpc, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		structs_free(&structs_type_xmlrpc_request, NULL, xmlrpc);
		FREE(TYPED_MEM_TEMP, xmlrpc);
		structs_free(type, NULL, &scopy);
		structs_free(type, NULL, &s);
		goto done;
	}
	structs_free(&structs_type_xmlrpc_request, NULL, xmlrpc);
	FREE(TYPED_MEM_TEMP, xmlrpc);
    }

	printf(">>>>>>>> TEST: dumping copy's element list...\n");
    {
	char **list;
	int len;
	int i;

	if ((len = structs_traverse(type, &scopy, &list, TYPED_MEM_TEMP)) == -1)
		err(1, "structs_traverse");
	for (i = 0; i < len; i++) {
		char *val;

		if ((val = structs_get_string(type,
		    list[i], &scopy, TYPED_MEM_TEMP)) == NULL)
			err(1, "structs_get_string");
		printf("\t%s=%s\n", list[i], val);
		FREE(TYPED_MEM_TEMP, val);
	}
	while (len > 0)
		FREE(TYPED_MEM_TEMP, list[--len]);
	FREE(TYPED_MEM_TEMP, list);
    }

	printf(">>>>>>>> TEST: Free'ing original and copy...\n");
	structs_free(type, NULL, &s);
	structs_free(type, NULL, &scopy);

done:
	if (!nostats) {
		FILE *const fp = coding ? stderr : stdout;

		fprintf(fp, ">>>>>>>> TEST: Displaying unfree'd memory...\n");
		typed_mem_dump(fp);
	}

	/* Done */
	return (0);
}

static int
structs_encode(const char *fname)
{
	const struct structs_type *type = &inner_struct_type;
	struct structs_data code;
	struct inner_struct s;

	if (structs_xml_input(type, ELEM_TAG, NULL, NULL, stdin,
	    &s, STRUCTS_XML_UNINIT | flags, STRUCTS_LOGGER_STDERR) == -1) {
		warn("structs_xml_input");
		return (-1);
	}
	if (structs_get_binary(type, fname, &s, TYPED_MEM_TEMP, &code) == -1) {
		warn("structs_get_binary");
		structs_free(type, NULL, &s);
		return (-1);
	}
	if (fwrite(code.data, 1, code.length, stdout) != code.length) {
		warn("fwrite");
		FREE(TYPED_MEM_TEMP, code.data);
		structs_free(type, NULL, &s);
		return (-1);
	}
	FREE(TYPED_MEM_TEMP, code.data);
	structs_free(type, NULL, &s);
	return (0);
}

static int
structs_decode(const char *fname)
{
	const struct structs_type *type = &inner_struct_type;
	struct structs_data code;
	struct inner_struct s;
	u_char buf[0x10000];
	char ebuf[128];
	int blen;
	int clen;

	if (structs_init(type, NULL, &s) == -1) {
		warn("initializing inner_struct");
		return (-1);
	}
	if ((blen = fread(buf, 1, sizeof(buf), stdin)) == 0) {
		warn("reading input");
		structs_free(type, NULL, &s);
		return (-1);
	}
	code.data = buf;
	code.length = blen;
	if ((clen = structs_set_binary(type,
	    fname, &code, &s, ebuf, sizeof(ebuf))) == -1) {
		warnx("structs_set_binary: %s", ebuf);
		structs_free(type, NULL, &s);
		return (-1);
	}
	if (clen < blen) {
		fprintf(stderr, "WARNING: ignoring %d extra bytes\n",
		    blen - clen);
	}
	if (structs_xml_output(type,
	    ELEM_TAG, NULL, &s, stdout, NULL, 0) == -1) {
		warn("structs_xml_output");
		structs_free(type, NULL, &s);
		return (-1);
	}
	structs_free(type, NULL, &s);
	return (0);
}

#ifndef NO_BPF
static int
bpf_compiler(const char *string, struct bpf_program *bpf,
	int linktype, char *ebuf, size_t emax)
{
	pcap_t *pcap;

	memset(bpf, 0, sizeof(*bpf));
	if ((pcap = pcap_open_dead(linktype, 2048)) == NULL)
		return (-1);
	if (pcap_compile(pcap, bpf, (char *)string, 1, ~0) != 0) {
		strlcpy(ebuf, pcap_geterr(pcap), emax);
		pcap_close(pcap);
		errno = EINVAL;
		return (-1);
	}
	pcap_close(pcap);
	return (0);
}
#endif

