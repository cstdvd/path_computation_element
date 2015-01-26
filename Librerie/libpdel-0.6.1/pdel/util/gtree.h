
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_GTREE_H_
#define _PDEL_UTIL_GTREE_H_

/*
 * General purpose balanced trees.
 */

/**********************************************************************
			TREE FUNCTION TYPES
**********************************************************************/

struct gtree;

/*
 * How to compare two items. Should return -, 0, or + if item1
 * is less than, equal to, or greater than item2.
 *
 * If this function is not specified, then "item1 - item2" is used.
 */
typedef int	gtree_cmp_t(struct gtree *g,
			const void *item1, const void *item2);

/*
 * Notification that an item is being added to the table.
 *
 * Supplying this function is optional.
 */
typedef void	gtree_add_t(struct gtree *g, void *item);

/*
 * Notification that an item is being removed from the table.
 *
 * Supplying this function is optional.
 */
typedef void	gtree_del_t(struct gtree *g, void *item);

/*
 * Return a string description of a node in a static buffer.
 * This is for tracing.
 *
 * Supplying this function is optional.
 */
typedef const	char *gtree_print_t(struct gtree *g, const void *item);

/**********************************************************************
			TREE METHODS
**********************************************************************/

__BEGIN_DECLS

/*
 * Create a new hash table.
 */
extern struct	gtree *gtree_create(void *arg, const char *mtype,
			gtree_cmp_t *cmp, gtree_add_t *add,
			gtree_del_t *del, gtree_print_t *print);

/*
 * Destroy a hash table.
 *
 * Any items remaining in the table will be removed first.
 */
extern void	gtree_destroy(struct gtree **gp);

/*
 * Get the argument supplied to gtree_create().
 */
extern void	*gtree_arg(struct gtree *g);

/*
 * Get an item.
 *
 * Returns the item, or NULL if the item does not exist.
 */
extern void	*gtree_get(struct gtree *g, const void *item);

/*
 * Put an item.
 *
 * Returns 0 if the item is new, 1 if it replaces an existing
 * item, and -1 if there was an error.
 *
 * Note: NULL is an invalid item because gtree_get() returns
 * NULL to indicate that the item was not found.
 */
extern int	gtree_put(struct gtree *g, const void *item);

/*
 * Same as gtree_put() but caller supplies a pre-allocated
 * node structure which must be allocated with the 'memtype'
 * supplied to gtree_new() and have size gtree_node_size().
 * gtree_put_prealloc() assumes responsibility for freeing
 * this memory. gtree_put_prealloc() never returns -1.
 */
extern int	gtree_put_prealloc(struct gtree *g,
			const void *item, void *node);

/*
 * Remove an item.
 *
 * Returns 1 if the item was found and removed, 0 if not found.
 */
extern int	gtree_remove(struct gtree *g, const void *item);

/*
 * Traverse the tree in order.
 *
 * The handler should return -1 to cancel the traverse.
 * Returns -1 if the traverse was stopped, otherwise zero.
 *
 * No modifications to the tree are allowed by the handler.
 */
extern int	gtree_traverse(struct gtree *g,
			int (*handler)(struct gtree *g, void *item));

/*
 * Get the size of the table.
 */
extern u_int	gtree_size(struct gtree *g);

/*
 * Get the size of a node.
 */
extern u_int	gtree_node_size(void);

/*
 * Get the first item.
 *
 * Returns the item, or NULL if the tree is empty.
 */
extern void	*gtree_first(struct gtree *g);

/*
 * Get the next item after 'item'.
 *
 * Returns the next item, or NULL if item was the last item
 * or item does not exist in the tree.
 */
extern void	*gtree_next(struct gtree *g, const void *item);

/*
 * Get the previous item before 'item'.
 *
 * Returns the previous item, or NULL if item was the first item
 * or item does not exist in the tree.
 */
extern void	*gtree_prev(struct gtree *g, const void *item);

/*
 * Get the last item.
 *
 * Returns the item, or NULL if the tree is empty.
 */
extern void	*gtree_last(struct gtree *g);

/*
 * Get a sorted array of items.
 *
 * Returns number of items in the list, or -1 if error.
 * Caller must free the list.
 */
extern int	gtree_dump(struct gtree *g, void ***listp, const char *mtype);

#ifndef _KERNEL
/*
 * Print out a tree
 */
extern void	gtree_print(struct gtree *g, FILE *fp);
#endif

__END_DECLS

#endif	/* _PDEL_UTIL_GTREE_H_ */

