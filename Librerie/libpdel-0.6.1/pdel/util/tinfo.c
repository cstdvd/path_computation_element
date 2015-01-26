
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "util/typed_mem.h"
#include "util/tinfo.h"

/* Per-thread tinfo data */
struct tinfo_private {
	struct tinfo	*t;
	void		*data;				/* user data */
};

/* Internal variables */
static struct	tinfo_private *tinfo_init(struct tinfo *t);
static void	tinfo_instance_destroy(void *arg);

/*
 * Get the per-thread instance, creating a new one if none there.
 *
 * Note: the returned value should NOT be free'd by the caller.
 */
void *
tinfo_get(struct tinfo *t)
{
	struct tinfo_private *priv;

	/* Get instance container */
	if ((priv = tinfo_init(t)) == NULL)
		return (NULL);

	/* Has instance been constructed yet? If not, construct one */
	if (priv->data == NULL) {

		/* Create a new instance for this thread */
		if ((priv->data = MALLOC(t->mtype, t->type->size)) == NULL)
			return (NULL);

		/* Run application initializer (if any) or else default */
		if ((t->init != NULL ? (*t->init)(t, priv->data) :
		    structs_init(t->type, NULL, priv->data)) == -1) {
			FREE(t->mtype, priv->data);
			priv->data = NULL;
		}
	}

	/* Done */
	return (priv->data);
}

/*
 * Set a new value for the per-thread variable.
 */
int
tinfo_set(struct tinfo *t, const void *data)
{
	struct tinfo_private *priv;
	void *copy;

	/* Get instance container */
	if ((priv = tinfo_init(t)) == NULL)
		return (-1);

	/* Handle case where data is NULL */
	if (data == NULL) {
		if (priv->data != NULL) {
			structs_free(t->type, NULL, priv->data);
			FREE(t->mtype, priv->data);
			priv->data = NULL;
		}
		return (0);
	}

	/* Copy new data provided by caller */
	if ((copy = MALLOC(t->mtype, t->type->size)) == NULL)
		return (-1);
	if (structs_get(t->type, NULL, data, copy) == -1) {
		FREE(t->mtype, copy);
		return (-1);
	}

	/* Set copy */
	if (tinfo_set_nocopy(t, copy) == -1) {
		structs_free(t->type, NULL, priv->data);
		FREE(t->mtype, copy);
	}

	/* Done */
	return (0);
}

/*
 * Set a new value for the per-thread variable without copying.
 */
int
tinfo_set_nocopy(struct tinfo *t, void *data)
{
	struct tinfo_private *priv;

	/* Get instance container */
	if ((priv = tinfo_init(t)) == NULL)
		return (-1);

	/* Free existing value, if any */
	if (priv->data != NULL) {
		structs_free(t->type, NULL, priv->data);
		FREE(t->mtype, priv->data);
	}

	/* Set new value */
	priv->data = data;
	return (0);
}

/*
 * Initialize per-thread variable if not already initialized.
 */
static struct tinfo_private *
tinfo_init(struct tinfo *t)
{
	struct tinfo_private *priv;

	/* Initialize key, once for all threads */
	if (t->pkey == TINFO_KEY_INIT
	    && (errno = pthread_key_create(&t->pkey,
	      tinfo_instance_destroy)) != 0) {
		t->pkey = TINFO_KEY_INIT;
		return (NULL);
	}

	/* Construct instance container for this thread if needed */
	if ((priv = pthread_getspecific(t->pkey)) == NULL) {

		/* Get new instance container (a struct tinfo_private) */
		if ((priv = MALLOC(t->mtype, sizeof(*priv))) == NULL)
			return (NULL);
		memset(priv, 0, sizeof(*priv));
		priv->t = t;

		/* Store the container as the per-thread variable */
		if ((errno = pthread_setspecific(t->pkey, priv)) != 0) {
			FREE(t->mtype, priv);
			return (NULL);
		}
	}

	/* Done */
	return (priv);
}

/*
 * Destructor called upon thread exit.
 */
static void
tinfo_instance_destroy(void *arg)
{
	struct tinfo_private *const priv = arg;
	struct tinfo *const t = priv->t;

	/* Free instance */
	if (priv->data != NULL) {
		structs_free(t->type, NULL, priv->data);
		FREE(t->mtype, priv->data);
	}

	/* Free container */
	FREE(t->mtype, priv);
}


