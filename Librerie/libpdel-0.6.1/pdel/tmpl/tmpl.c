
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"

/* Internal functions */
static struct	tmpl *tmpl_create_internal(FILE *input, pd_mmap mh,
			int *num_errors, const char *mtype);

/*
 * Create a new template object.
 */
struct tmpl *
tmpl_create(FILE *input, int *num_errors, const char *mtype)
{
	return (tmpl_create_internal(input, NULL, num_errors, mtype));
}

/*
 * Create a new template object using pd_mmap().
 */
struct tmpl *
tmpl_create_mmap(const char *path, int *num_errors, const char *mtype)
{
	struct tmpl *tmpl;
	pd_mmap mh;
	int esave;
	FILE *fp;
	int fd;

	/* Open and memory map file */
	if ((fd = open(path, O_RDONLY)) == -1)
		return (NULL);
	if ((mh = pd_mmap_fd(fd, 0, 0, 
			     PD_PROT_READ, NULL)) == NULL) {
		close(fd);
		return (NULL);
	}
	close(fd);

	/* Parse file */
	if ((fp = fopen(path, "r")) == NULL)
		return (NULL);
	if ((tmpl = tmpl_create_internal(fp, mh,
			 num_errors, mtype)) == NULL) {
		esave = errno;
		fclose(fp);
		errno = esave;
		return (NULL);
	}

	/* Done */
	return (tmpl);
}

/*
 * Create a new template object.
 */
static struct tmpl *
tmpl_create_internal(FILE *input, pd_mmap mh,
	int *num_errors, const char *mtype)
{
	struct tmpl *tmpl;
	int dummy;
	int r;

	/* Sanity check */
	if (input == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	if (num_errors == NULL)
		num_errors = &dummy;

	/* Initialize template object */
	if ((tmpl = MALLOC(mtype, sizeof(*tmpl))) == NULL)
		return (NULL);
	memset(tmpl, 0, sizeof(*tmpl));
	tmpl->mh = mh;
	tmpl->mmap_addr = pd_mmap_getaddr(mh);
	tmpl->mmap_len = pd_mmap_getsize(mh);
	if (mtype != NULL) {
		strlcpy(tmpl->mtype_buf, mtype, sizeof(tmpl->mtype_buf));
		tmpl->mtype = tmpl->mtype_buf;
	}

	/* Parse template; cleanup if thread is canceled */
	pthread_cleanup_push((void (*)(void *))tmpl_destroy, &tmpl);
	r = _tmpl_parse(tmpl, input, num_errors);
	pthread_cleanup_pop(0);

	/* Check for error */
	if (r == -1) {
		tmpl_destroy(&tmpl);
		return (NULL);
	}

	/* Done */
	return (tmpl);
}

/*
 * Destroy a template object.
 */
void
tmpl_destroy(struct tmpl **tmplp)
{
	struct tmpl *const tmpl = *tmplp;

	if (tmpl == NULL)
		return;
	*tmplp = NULL;
	_tmpl_free_elems(tmpl->mtype,
	    tmpl->eblock, tmpl->elems, tmpl->num_elems);
	if (tmpl->mh != NULL)
		pd_mmap_cleanup(&tmpl->mh, 0);
	FREE(tmpl->mtype, tmpl);
}

