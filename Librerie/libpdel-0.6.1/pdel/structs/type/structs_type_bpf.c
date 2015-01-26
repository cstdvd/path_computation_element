
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/bpf.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <syslog.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/bpf.h"
#include "structs/type/struct.h"
#include "structs/type/string.h"
#include "structs/type/data.h"
#include "util/gtree.h"
#include "util/typed_mem.h"

/*********************************************************************
			BPF PROGRAM TYPE
*********************************************************************/

#define MEM_TYPE		"structs_type_bpf"

/*
 * Internal functions
 */
static int	structs_bpf_check_init(void);
static struct	structs_bpf *structs_bpf_add(structs_bpf_compile_t *compiler,
			const char *ascii, struct structs_data *prog,
			int linktype, char *ebuf, size_t emax);

static gtree_cmp_t	structs_bpf_compare;

/*
 * Internal variables
 */
static struct		gtree *structs_type_bpf_tree;
static pthread_mutex_t	structs_type_bpf_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Default program: accept everything
 */
static const	struct bpf_insn structs_bpf_all_prog[] = {
	{ BPF_RET, 0, 0, ETHER_MAX_LEN }
};

/*
 * Initialize method
 */
int
structs_bpf_init(const struct structs_type *type, void *data)
{
	structs_bpf_compile_t *const compiler = type->args[1].v;
	const int linktype = type->args[0].i;
	struct structs_data pdata;
	struct structs_bpf *bp;

	if (structs_bpf_check_init() == -1)
		return (-1);
	pdata.data = (void *)&structs_bpf_all_prog;
	pdata.length = sizeof(structs_bpf_all_prog);
	if ((bp = structs_bpf_add(compiler,
	    "", &pdata, linktype, NULL, 0)) == NULL)
		return (-1);
	*((struct structs_bpf **)data) = bp;
	return (0);
}

/*
 * Copy method
 */
int
structs_bpf_copy(const struct structs_type *type,
	const void *from, void *to)
{
	struct structs_bpf *const bp = *((void **)from);
	int r;

	*((void **)to) = bp;
	r = pthread_mutex_lock(&structs_type_bpf_mutex);
	assert(r == 0);
	bp->_refs++;
	r = pthread_mutex_unlock(&structs_type_bpf_mutex);
	assert(r == 0);
	return (0);
}

/*
 * Equality method
 */
int
structs_bpf_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	struct structs_bpf *const bp1 = *((void **)v1);
	struct structs_bpf *const bp2 = *((void **)v2);

	return (bp1 == bp2);				/* nice 'n easy */
}

/*
 * Ascify method
 */
char
*structs_bpf_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	struct structs_bpf *const bp = *((void **)data);

	return (STRDUP(mtype, bp->ascii));
}

/*
 * Binify method
 */
int
structs_bpf_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	structs_bpf_compile_t *const compiler = type->args[1].v;
	const int linktype = type->args[0].i;
	struct structs_bpf *bp;

	if (structs_bpf_check_init() == -1)
		return (-1);
	if ((bp = structs_bpf_add(compiler,
	    ascii, NULL, linktype, ebuf, emax)) == NULL)
		return (-1);
	*((struct structs_bpf **)data) = bp;
	return (0);
}

/*
 * Free method
 */
void
structs_bpf_free(const struct structs_type *type, void *data)
{
	struct structs_bpf **const bpp = data;
	struct structs_bpf *const bp = *bpp;
	int removed;
	int r;

	/* Check for NULL */
	if (bp == NULL)
		return;
	*bpp = NULL;

	/* Lock tree */
	r = pthread_mutex_lock(&structs_type_bpf_mutex);
	assert(r == 0);

	/* Decrement ref count and nuke if zero */
	assert(bp->_refs >= 1);
	if (--bp->_refs == 0) {
		removed = gtree_remove(structs_type_bpf_tree, bp);
		assert(removed);
		FREE(NULL, bp->bpf.bf_insns);
		FREE(MEM_TYPE, (char *)bp->ascii);
		FREE(MEM_TYPE, bp);
	}

	/* Clean up empty tree */
	if (gtree_size(structs_type_bpf_tree) == 0)
		gtree_destroy(&structs_type_bpf_tree);

	/* Done */
	r = pthread_mutex_unlock(&structs_type_bpf_mutex);
	assert(r == 0);
}

/* Use this structure for encoding BPF code */
struct bpf_encoded {
	const char		*ascii;
	struct structs_data	prog;
};

/* Type for struct bpf_encoded */
static const struct structs_field bpf_encoded_fields[] = {
	STRUCTS_STRUCT_FIELD(bpf_encoded, ascii, &structs_type_string),
	STRUCTS_STRUCT_FIELD(bpf_encoded, prog, &structs_type_data),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type bpf_encoded_type
	= STRUCTS_STRUCT_TYPE(bpf_encoded, &bpf_encoded_fields);

/*
 * Encode
 */
int
structs_bpf_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	struct structs_bpf *const bp = *((void **)data);
	struct bpf_encoded be;

	/* Set up structure */
	be.ascii = bp->ascii;
	be.prog.data = (u_char *)bp->bpf.bf_insns;
	be.prog.length = bp->bpf.bf_len * sizeof(*bp->bpf.bf_insns);

	/* Encode it */
	if (structs_get_binary(&bpf_encoded_type, NULL, &be, mtype, code) == -1)
		return (-1);

	/* Done */
	return (0);
}

/*
 * Decode method
 */
int
structs_bpf_decode(const struct structs_type *type, const u_char *code,
	size_t cmax, void *data, char *ebuf, size_t emax)
{
	structs_bpf_compile_t *const compiler = type->args[1].v;
	const int linktype = type->args[0].i;
	struct structs_data cdata;
	struct structs_bpf *bp;
	struct bpf_encoded be;
	int clen;

	/* Initialize encoded struct */
	if (structs_init(&bpf_encoded_type, NULL, &be) == -1)
		return (-1);

	/* Decode structure */
	cdata.data = (u_char *)code;
	cdata.length = cmax;
	if ((clen = structs_set_binary(&bpf_encoded_type,
	    NULL, &cdata, &be, ebuf, emax)) == -1) {
		structs_free(&bpf_encoded_type, NULL, &be);
		return (-1);
	}

	/* Add program */
	if (structs_bpf_check_init() == -1) {
		structs_free(&bpf_encoded_type, NULL, &be);
		return (-1);
	}
	if ((bp = structs_bpf_add(compiler,
	    be.ascii, &be.prog, linktype, ebuf, emax)) == NULL) {
		structs_free(&bpf_encoded_type, NULL, &be);
		return (-1);
	}
	structs_free(&bpf_encoded_type, NULL, &be);
	*((struct structs_bpf **)data) = bp;
	return (clen);
}

/*
 * Ensure tree is initialized
 */
static int
structs_bpf_check_init(void)
{
	int rtn = 0;
	int r;

	/* Create new tree if needed */
	r = pthread_mutex_lock(&structs_type_bpf_mutex);
	assert(r == 0);
	if (structs_type_bpf_tree == NULL
	    && (structs_type_bpf_tree = gtree_create(NULL, MEM_TYPE,
	      structs_bpf_compare, NULL, NULL, NULL)) == NULL)
		rtn = -1;
	r = pthread_mutex_unlock(&structs_type_bpf_mutex);
	assert(r == 0);

	/* Done */
	return (rtn);
}

/*
 * Create a new filter. This might return an existing filter
 * (with a bumped reference count) if such a filter already exists.
 */
static struct structs_bpf *
structs_bpf_add(structs_bpf_compile_t *compiler, const char *ascii,
	struct structs_data *prog, int linktype, char *ebuf, size_t emax)
{
	struct structs_bpf key;
	struct structs_bpf *bp;
	size_t x, y;
	char *abuf;
	int r;

	/* Copy string, compressing all whitespaces into a single space */
	if ((abuf = MALLOC(MEM_TYPE, strlen(ascii) + 1)) == NULL)
		return (NULL);
	for (x = 0; isspace(ascii[x]); x++);	  /* trim leading ws */
	for (y = 0; ascii[x] != '\0'; x++, y++) {
		if (isspace(ascii[x])) {
			while (isspace(ascii[x + 1]))
				x++;		  /* turn ws into ' ' */
			abuf[y] = ' ';
		} else
			abuf[y] = ascii[x];
	}
	while (y > 0 && isspace(abuf[y - 1]))	  /* trim trailing ws */
		y--;
	abuf[y] = '\0';

	/* Lock tree */
	r = pthread_mutex_lock(&structs_type_bpf_mutex);
	assert(r == 0);

	/* See if it already exists; if so, just bump reference count */
	key.ascii = abuf;
	key.linktype = linktype;
	if ((bp = gtree_get(structs_type_bpf_tree, &key)) != NULL) {
		FREE(MEM_TYPE, abuf);
		bp->_refs++;
		goto done;
	}

	/* Create a new 'structs_bpf' structure */
	if ((bp = MALLOC(MEM_TYPE, sizeof(*bp))) == NULL)
		goto fail;
	memset(bp, 0, sizeof(*bp));
	bp->ascii = abuf;
	bp->linktype = linktype;
	bp->_refs = 1;

	/* Compile bpf program, or use pre-compiled program if supplied */
	if (prog == NULL) {

		/* If no compiler supplied, can't compile */
		if (compiler == NULL) {
			snprintf(ebuf, emax,
			    "BPF compilation is not supported by this type");
			errno = EOPNOTSUPP;
			goto fail;
		}

		/* Compile ASCII string into BPF program */
		if ((*compiler)(ascii, &bp->bpf, linktype, ebuf, emax) == -1) {
			errno = EINVAL;
			goto fail;
		}
	} else {

		/* Copy pre-compiled program supplied by caller */
		if ((bp->bpf.bf_insns = MALLOC(NULL, prog->length)) == NULL)
			goto fail;
		memcpy(bp->bpf.bf_insns, prog->data, prog->length);
		bp->bpf.bf_len = prog->length / sizeof(*bp->bpf.bf_insns);
	}

	/* Add new program to the tree */
	if (gtree_put(structs_type_bpf_tree, bp) == -1) {
		FREE(NULL, bp->bpf.bf_insns);
		goto fail;
	}

done:
	/* Done */
	r = pthread_mutex_unlock(&structs_type_bpf_mutex);
	assert(r == 0);
	return (bp);

fail:
	/* Clean up after failure */
	r = pthread_mutex_unlock(&structs_type_bpf_mutex);
	assert(r == 0);
	if (bp != NULL) {
		FREE(MEM_TYPE, (char *)bp->ascii);
		FREE(MEM_TYPE, bp);
	}
	return (NULL);
}

/*
 * Compare two struct structs_bpf's.
 *
 * We compare them by comparing the ASCII strings and linktype. It's possible
 * but unlikely that two different strings compile to the same BPF code.
 * If so, no big deal, we just store them as two separate structures.
 */
static int
structs_bpf_compare(struct gtree *g, const void *item1, const void *item2)
{
	const struct structs_bpf *const bp1 = item1;
	const struct structs_bpf *const bp2 = item2;
	int diff;

	if ((diff = strcmp(bp1->ascii, bp2->ascii)) != 0)
		return (diff);
	return (bp1->linktype - bp2->linktype);
}

