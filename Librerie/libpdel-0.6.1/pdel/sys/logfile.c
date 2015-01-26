
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

#include "pdel/pd_mem.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "sys/logfile.h"
#include "sys/alog.h"
#include "util/typed_mem.h"

#define LOGFILE_MEM_TYPE	"logfile"
#define LOGFILE_MAGIC		0x476ea198

#define MAX_ENTRIES		(1 << 20)
#define MAX_DATA		(1 << 24)

/* Structure passed back to client */
struct logfile {
	pd_mmap		mh;
	struct loghead	*head;			/* mmap'd file region */
	pthread_mutex_t	mutex;			/* mutex lock */
	u_int32_t	headlen;		/* length of head + entries */
	int		fd;			/* file descriptor for file */
};

struct logent {
	u_int32_t	offset;
	u_int32_t	length;
};

/* Initial part of a log file; all fields in host order */
struct loghead {
	u_int32_t	magic;			/* magic number */
	u_int32_t	maxent;			/* max # entries */
	u_int32_t	maxdata;		/* max data area length */
	u_int32_t	num;			/* number of valid entries */
	u_int32_t	next;			/* next entry index */
	struct logent	ents[0];		/* maxent entries */
};

/*
 * Open/create a new logfile.
 */
struct logfile *
logfile_open(const char *path, int flags, u_int32_t maxent, u_int32_t maxdata)
{
	struct logfile *lf;
	struct loghead head;
	int initialize;
	int esave;

	/* Get and sanity check flags */
	switch (flags) {
	case 0:
#ifdef O_SHLOCK
	case O_SHLOCK:
	case O_EXLOCK:
#endif
		break;
	default:
		errno = EINVAL;
		return (NULL);
	}
#ifdef O_SHLOCK
	if ((flags & (O_SHLOCK|O_EXLOCK)) != 0)
		flags |= O_NONBLOCK;
#endif

	/* Create object and open file */
	if ((lf = MALLOC(LOGFILE_MEM_TYPE, sizeof(*lf))) == NULL)
		return (NULL);
	memset(lf, 0, sizeof(*lf));
	if (path != NULL) {
#ifdef O_NOINHERIT
		flags |= O_NOINHERIT;
#endif
		if ((lf->fd = open(path, O_CREAT|O_RDWR|flags, 0644)) == -1)
			goto fail;
#ifndef WIN32
		(void)fcntl(lf->fd, F_SETFD, 1);
#endif
	} else
		lf->fd = -1;

	/* See if file already existed */
	if (lf->fd != -1) {
		struct stat sb;

		if (fstat(lf->fd, &sb) == -1)
			goto fail;
		if (!(initialize = (sb.st_size == 0))) {
			int r;

			if ((r = read(lf->fd,
			    &head, sizeof(head))) != sizeof(head)) {
				if (r != -1)
					errno = EINVAL;
				goto fail;
			}
			if (head.magic != LOGFILE_MAGIC) {
				errno = EINVAL;
				goto fail;
			}
			maxdata = head.maxdata;
			maxent = head.maxent;
		}
	} else
		initialize = 1;

	/* Sanity check parameters */
	if (maxent == 0 || maxdata == 0
	    || maxent > MAX_ENTRIES || maxdata > MAX_DATA) {
		errno = EINVAL;
		goto fail;
	}

	/* Compute size of header */
	lf->headlen = sizeof(*lf->head) + (maxent * sizeof(*lf->head->ents));

	/* Memory map file */
	if ((lf->mh = pd_mmap_fd(lf->fd, lf->headlen + maxdata, 0,
				 PD_PROT_READ|PD_PROT_WRITE, NULL)) == NULL)
		goto fail;

	lf->head = (struct loghead *) pd_mmap_getaddr(lf->mh);

	/* For new file, write header and initialize entries */
	if (initialize) {
		lf->head->magic = LOGFILE_MAGIC;
		lf->head->maxdata = maxdata;
		lf->head->maxent = maxent;
		lf->head->num = 0;
		lf->head->next = 0;
		memset(lf->head->ents, 0, maxent * sizeof(*lf->head->ents));
		pd_mmap_sync(lf->mh, 0, 0, 1);
	}

	/* Sanitize header fields */
	if (lf->head->num > lf->head->maxent)
		lf->head->num = lf->head->maxent;
	lf->head->next %= lf->head->maxent;

	/* Initialize mutex */
	if ((errno = pthread_mutex_init(&lf->mutex, NULL)) != 0)
		goto fail;

	/* Done */
	return (lf);

fail:
	esave = errno;
	if (lf->mh != NULL)
		pd_mmap_cleanup(&lf->mh, 0);
	if (lf->fd > -1)
		close(lf->fd);
	FREE(LOGFILE_MEM_TYPE, lf);
	errno = esave;
	return (NULL);
}

/*
 * Close a logfile.
 */
void
logfile_close(struct logfile **lfp)
{
	struct logfile *const lf = *lfp;

	/* Check for NULL */
	if (lf == NULL)
		return;
	*lfp = NULL;

	/* Close up shop */
	pd_mmap_sync(lf->mh, 0, 0, 1);
	pd_mmap_cleanup(&lf->mh, 0);
	if (lf->fd != -1)
		(void)close(lf->fd);
	pthread_mutex_destroy(&lf->mutex);
	FREE(LOGFILE_MEM_TYPE, lf);
}

/*
 * Get the number of valid entries in a logfile.
 */
u_int32_t
logfile_num_entries(struct logfile *lf)
{
	u_int32_t num;
	int r;

	r = pthread_mutex_lock(&lf->mutex);
	assert(r == 0);
	num = lf->head->num;
	r = pthread_mutex_unlock(&lf->mutex);
	assert(r == 0);
	return (num);
}

/*
 * Trim the number of stored entries.
 */
void
logfile_trim(struct logfile *lf, int num)
{
	int r;

	r = pthread_mutex_lock(&lf->mutex);
	assert(r == 0);
	if (lf->head->num > num)
		lf->head->num = num;
	r = pthread_mutex_unlock(&lf->mutex);
	assert(r == 0);
}

/*
 * Retrieve an entry.
 */
const void *
logfile_get(struct logfile *lf, int which, int *lenp)
{
	struct loghead *const head = lf->head;
	struct logent *ent;
	const void *rtn;
	int r;

	/* Lock logfile */
	r = pthread_mutex_lock(&lf->mutex);
	assert(r == 0);

	/* Find entry */
	if (which >= 0 || which < - (int) head->num) {
		r = pthread_mutex_unlock(&lf->mutex);
		assert(r == 0);
		errno = ENOENT;
		return (NULL);
	}
	ent = &head->ents[(head->next + head->maxent + which) % head->maxent];

	/* Sanity check it */
	if (ent->offset > head->maxdata
	    || ent->length > head->maxdata
	    || ent->offset + ent->length > head->maxdata) {
		r = pthread_mutex_unlock(&lf->mutex);
		assert(r == 0);
		errno = EINVAL;
		return (NULL);
	}

	/* Get data and length */
	if (lenp != NULL)
		*lenp = ent->length;
	rtn = (u_char *)lf->head + lf->headlen + ent->offset;

	/* Unlock logfile */
	r = pthread_mutex_unlock(&lf->mutex);
	assert(r == 0);

	/* Done */
	return (rtn);
}

/*
 * Put an entry.
 */
int
logfile_put(struct logfile *lf, const void *data, int len)
{
	struct loghead *const head = lf->head;
	struct logent *ent;
	u_int32_t start;
	int wrap = 0;
	int r;

	if (len < 0) {
		errno = EINVAL;
		return (-1);
	}
	if (len > head->maxdata) {
		errno = EMSGSIZE;
		return (-1);
	}

	/* Lock logfile */
	r = pthread_mutex_lock(&lf->mutex);
	assert(r == 0);

	/* Figure out where this entry's data will go */
	if (head->num > 0) {
		ent = &head->ents[(head->next
		    + head->maxent - 1) % head->maxent];
		start = ALIGN(ent->offset + ent->length);
		if (start + len > head->maxdata) {	/* won't fit, wrap it */
			wrap = start;	/* point where we were forced to wrap */
			start = 0;
		}
	} else {
		head->next = 0;
		start = 0;
	}

	/* Remove all entries whose data overlaps the new guy's data */
	for ( ; head->num > 0; head->num--) {
		ent = &head->ents[(head->next
		    + head->maxent - head->num) % head->maxent];
		if (wrap != 0) {	/* clear out end region we skipped */
			if (ent->offset >= wrap)
				continue;
			wrap = 0;
		}
		if (ent->offset + ent->length <= start
		    || ent->offset >= start + len)
			break;
	}

	/* Save entry */
	ent = &head->ents[head->next];
	ent->offset = start;
	ent->length = len;
	memcpy((u_char *)lf->head + lf->headlen + ent->offset, data, len);
	if (head->num < head->maxent)
		head->num++;
	head->next = (head->next + 1) % head->maxent;

	/* Unlock logfile */
	r = pthread_mutex_unlock(&lf->mutex);
	assert(r == 0);

	/* Done */
	return (0);
}

/*
 * Sync logfile to disk.
 */
void
logfile_sync(struct logfile *lf)
{
	int r;

	r = pthread_mutex_lock(&lf->mutex);
	assert(r == 0);
	pd_mmap_sync(lf->mh, 0, 0, 1);
	r = pthread_mutex_unlock(&lf->mutex);
	assert(r == 0);
}

#ifdef LOGFILE_TEST

#include <err.h>

int
main(int ac, char **av)
{
	const time_t now = time(NULL);
	struct logfile *lf;
	int maxent = 0;
	int maxdata = 0;
	int readonly = 0;
	int alog_ents = 0;
	long seed = 0;
	int num = -1;
	char *path;
	int i;
	int ch;

	srandomdev();

	while ((ch = getopt(ac, av, "s:n:ra")) != -1) {
		switch (ch) {
		case 's':
			seed = atol(optarg);
			break;
		case 'n':
			num = atol(optarg);
			break;
		case 'a':
			alog_ents = 1;
			break;
		case 'r':
			readonly = 1;
			break;
		default:
			goto usage;
		}
	}
	ac -= optind;
	av += optind;

	/* Sanity */
	if (!readonly)
		alog_ents = 0;

	if (!readonly && seed == 0) {
		seed = random();
		printf("Seed is %ld.\n", seed);
	}
	srandom(seed);

	switch (ac) {
	case 3:
		maxent = atoi(av[1]);
		maxdata = atoi(av[2]);
		/* fall through */
	case 1:
		path = av[0];
		break;
	default:
usage:		fprintf(stderr, "usage: logfile [-r] [-s seed] [-n num]"
		    " path [maxent maxdata]\n");
		exit(1);
	}

	/* Open log */
	if ((lf = logfile_open(strcmp(path, "-") == 0 ? NULL : path,
	    maxent, maxdata)) == NULL)
		err(1, "%s", path);

	/* Read only? */
	if (readonly)
		goto readit;

	/* Write some entries into it */
	printf("Logfile \"%s\" has %d entries.\n",
	    path, logfile_num_entries(lf));
	if (num == -1)
		num = random() % 64;
	printf("Writing %d entries...\n", num);
	for (i = 0; i < num; i++) {
		char buf[128];
		int nex;
		int j;

		snprintf(buf, sizeof(buf), "%03ld:%03d ", now % 1000, i);
		nex = random() % 32;
		for (j = 0; j < nex; j++) {
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%02lx", random() % 0x100);
		}
		strlcat(buf, "\n", sizeof(buf));
		if (logfile_put(lf, buf, strlen(buf) + 1) == -1)
			warn("logfile_put: \"%s\"", buf);
	}

readit:
	num = logfile_num_entries(lf);
	printf("Logfile \"%s\" now has %d entries\n", path, num);
	printf("\t maxent=%u\n", lf->head->maxent);
	printf("\tmaxdata=%u\n", lf->head->maxdata);
	printf("\t   next=%u\n", lf->head->next);
	for (i = -num; i < 0; i++) {
		const void *e;
		int len;

		printf("%4d: ", i + num);
		if ((e = logfile_get(lf, i, &len)) == NULL) {
			warn("logfile_get(%d)", i);
			continue;
		}
		if (alog_ents) {
			const struct alog_entry *const ent = e;
			struct tm tm;
			char tbuf[64];

			strftime(tbuf, sizeof(tbuf),
			    "%b %e %T", localtime_r(&ent->when, &tm));
			printf("%s [%d] %s\n", tbuf, ent->sev, ent->msg);
		} else
			printf("(%2d) %s", len, (const char *)e);
	}
	printf("Closing logfile...\n");
	logfile_close(&lf);
	return (0);
}

#endif /* LOGFILE_TEST */


