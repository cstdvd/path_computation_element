
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_BPF_H_
#define _PDEL_STRUCTS_TYPE_BPF_H_

/*********************************************************************
			BPF PROGRAM TYPE
*********************************************************************/

/*
 * This type handles BPF programs. Multiple instances of the same
 * BPF program will re-use the same structure, saving space.
 *
 * The data is a *pointer* to a 'struct structs_bpf': multiple instances
 * may point to the same BPF program if they have the same tcpdump string
 * and link type. This is an optimization.
 *
 * The ASCII representation of this type is a tcpdump string.
 *
 * Newly created instances are initialized with the empty string,
 * which generates a BPF program that matches all packets.
 */

/*
 * Function type for supplying a BPF compiler routine.
 *
 * This function should compile 'string' and fill out the 'bpf' structure.
 * The bpf->bf_insns array should be allocated with NULL memory type
 * (to be consistent with libpcap).
 *
 * Should return 0 on success; Otherwise, -1 should be returned and a
 * message printed into 'ebuf' which has size 'emax' (see snprintf(3)).
 */
typedef int	structs_bpf_compile_t(const char *string,
			struct bpf_program *bpf, int linktype,
			char *ebuf, size_t emax);

/* A compiled BPF program structure */
struct structs_bpf {
	const char		*ascii;		/* ascii form of program */
	struct bpf_program	bpf;		/* compiled bpf program */
	int			linktype;	/* bpf link type */
	int			_refs;		/* ref count: don't touch! */
};

/*
 * Macro arguments:
 *	[int]				BPF link type, e.g., DLT_EN10MB
 *	[structs_bpf_compile_t *]	BPF compiler, or NULL for unsupported
 */
#define BPF_STRUCTS_TYPE(linktype, comp) {			\
	sizeof(struct structs_bpf *),				\
	"bpf",							\
	STRUCTS_TYPE_PRIMITIVE,					\
	structs_bpf_init,					\
	structs_bpf_copy,					\
	structs_bpf_equal,					\
	structs_bpf_ascify,					\
	structs_bpf_binify,					\
	structs_bpf_encode,					\
	structs_bpf_decode,					\
	structs_bpf_free,					\
	{ { (void *)(linktype) }, { (void *)(comp) }, { NULL } }\
}

__BEGIN_DECLS

/* Structs methods */
extern structs_init_t		structs_bpf_init;
extern structs_copy_t		structs_bpf_copy;
extern structs_equal_t		structs_bpf_equal;
extern structs_ascify_t		structs_bpf_ascify;
extern structs_binify_t		structs_bpf_binify;
extern structs_encode_t		structs_bpf_encode;
extern structs_decode_t		structs_bpf_decode;
extern structs_uninit_t		structs_bpf_free;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_BPF_H_ */

