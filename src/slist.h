/*
Copyright (c) 2011 Phil Jordan <phil@philjordan.eu>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef GENCCONT_SLIST_H
#define GENCCONT_SLIST_H

/* for standard C typedefs, NULL, offsetof(), etc. */
#if defined(LINUX) && defined(__KERNEL__)
/* compiling the linux kernel (or rather a module) */
#include <linux/types.h>
#include <linux/stddef.h>
#elif defined(KERNEL) && defined(APPLE)
/* xnu kernel */
#include <IOKit/IOTypes.h>
/* xnu for some reason doesn't typedef ptrdiff_t. To avoid stepping on toes,
 * we'll temporarily re-#define it in case another header sets it */
#define ptrdiff_t __darwin_ptrdiff_t
#else
/* assume libc is available */
#include <stddef.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* generic singly-linked list */

/* embed this in the list entry struct; if you want void pointers, use slist_gen_entry */
struct slist_head
{
	/* NULL if we've hit the end */
	struct slist_head* next;
};
/* predicate function type for filtering list entries, returns 0 for no match, non-0 for match */
typedef int (*genc_slist_entry_pred_fn)(struct slist_head* entry);

/** Locates a specific list entry based on the given predicate function.
 * Returns a pointer to the first matched element, or NULL if none is found.
 */
struct slist_head* genc_slist_find_entry(struct slist_head* start, genc_slist_entry_pred_fn pred);

/** Locates a specific list entry based on the given predicate function.
 * Returns a pointer to the previous element's 'next' field or the list head,
 * or a pointer to the last element's 'next' field (which is NULL) when no
 * match is found.
 * Unlike with genc_slist_find_entry(), the found element may be removed from
 * the list, or a new element inserted before it by altering the returned
 * pointer.
 */
struct slist_head** genc_slist_find_entry_ref(struct slist_head** start, genc_slist_entry_pred_fn pred);

/** Inserts a single new list element at the given position.
 * The 'at' argument can be a pointer to the head of the list or to a 'next' field.
 * Returns the pointer to the 'next' field of the inserted element, usable for further insertions.
 */
struct slist_head** genc_slist_insert_at(struct slist_head* new_entry, struct slist_head** at);

/** Inserts a single new list element after an existing entry.
 */
void genc_slist_insert_after(struct slist_head* new_entry, struct slist_head* after_entry);

/** Removes the list element at the given position.
 * The 'at' argument can be a pointer to the head of the list or to a 'next'
 * field. The element pointed to will be removed and returned with its 'next' field nulled.
 * NULL is returned if there was no element to remove.
 */
struct slist_head* genc_slist_remove_at(struct slist_head** at);

/** Removes the element following the given list element and returns it, or NULL if there was no such element.
 */
struct slist_head* genc_slist_remove_after(struct slist_head* after_entry);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* Handy macros for working with embedded list heads.
 *
 * Examples assume the following:
 * struct my_list
 * {
 *   int data1;
 *   ...
 *   struct slist_head list_head;
 *   ...
 *   int data2;
 * };
 */



/** genc_container_of(obj, cont_type, member_name)
 * Get pointer to struct object from a pointer to one of its members.
 * obj - pointer to a struct member, or NULL
 * cont_type - structure type of the containing object
 * member_name - name of the member of cont_type referenced by obj
 *
 * Similar to Linux's container_of macro, except it also handles NULL pointers
 * correctly, which is to say it evaluates to NULL when the obj is NULL. We also
 * try to support non-GCC compilers which don't support the ({ }) expression
 * syntax.
 */ 
#ifndef genc_container_of

/* function for avoiding multiple evaluation */
static inline char* genc_container_of_helper(const void* obj, ptrdiff_t offset)
{
	return (obj ? ((char*)obj - offset) : NULL);
}
#define genc_container_of(obj, cont_type, member_name) \
((cont_type*)genc_container_of_helper(obj, offsetof(cont_type, member_name)))

#endif


/** Get next list item with appropriate type.
 */
#define genc_slist_next(cur, list_type, list_head_member_name) \
genc_container_of((cur)->list_head_member_name.next, list_type, list_head_member_name)
/* Example:
 *
 * struct my_list* p = blah(); // not NULL
 * struct my_list* next = genc_slist_next(p, struct my_list, list_head);
 */

/** Iterate through (the rest of) a list with a for() loop.
 * genc_slist_for_each(loop_var, list_head, list_type, list_head_member_name)
 * loop_var - the "running" loop variable, must be declared as list_type* outside the loop
 * 
 * You may safely break out of this loop or use the 'continue' statement.
 */
#define genc_slist_for_each(loop_var, list_head, list_type, list_head_member_name) \
for (loop_var = genc_container_of((list_head), list_type, list_head_member_name); loop_var != NULL; loop_var = genc_slist_next(loop_var, list_type, list_head_member_name))

/* Example:
 * struct int_list* cur = NULL;
 * struct slist_head* list = setup_test_list();
 * genc_slist_for_each(cur, list, struct my_list, list_head)
 * {
 *   printf("%p: %d\n", cur, cur->data1);
 * }
 */


/** Iterate through (the rest of) a list with a for() loop while also tracking
 * the last link followed to arrive at the current element. This allows
 * insertion/removal at the current position.
 * genc_slist_for_each(loop_var, list_head_ref, list_type, list_head_member_name)
 * loop_var - the "running" loop variable, must be declared as list_type* outside the loop
 * list_head_ref - lvalue of type struct slist_head**. Must be initialised to address of
 * head of the list or 'next' pointer which to follow first. Will be updated to the followed
 * link on every iteration.
 *
 * Note that (*list_head_ref)->next is follewed for the next iteration, so after
 * inserting, you may wish to update list_head_ref to avoid running the loop body
 * more than once for the same element:
 * list_head_ref = genc_slist_insert_at(&new_entry->list_head, list_head_ref);
 *
 * You may safely break out of this loop or use the 'continue' statement.
 */
#define genc_slist_for_each_ref(loop_var, list_head_ref, list_type, list_head_member_name) \
for (loop_var = genc_container_of(*(list_head_ref), list_type, list_head_member_name); loop_var != NULL; (list_head_ref = &(*list_head_ref)->next), (loop_var = genc_container_of(*(list_head_ref), list_type, list_head_member_name)))

/* Example:
 * Insert an element before each existing element.
 *
 * struct my_list* cur = NULL;
 * struct slist_head* head = setup_test_list();
 * struct slist_head** pos = &head;
 * genc_slist_for_each(cur, pos, struct my_list, list_head)
 * {
 *   struct my_list* new_dbl = calloc(1, sizeof(struct my_list));
 *   new_dbl->data1 = cur->data1 * 2;
 *   pos = genc_slist_insert_at(&new_dbl->list_head, pos);
 * }
 */


/** genc_slist_remove_object_at(at, list_type, list_head_member_name)
 * Typed version of genc_slist_remove_at(). */
#define genc_slist_remove_object_at(at, list_type, list_head_member_name) \
genc_container_of(genc_slist_remove_at(at), list_type, list_head_member_name)

/* Iterate through (the rest of) a list with a for() loop, removing each element before entering the loop body.
 * genc_slist_for_each(removed_element, list_head, list_type, list_head_member_name)
 * list_head - where to start removing elements (pointer to variable or field), must be of type struct slist_head**.
 *   list_head itself won't be modified, but *list_head will (necessarily).
 * removed_element - variable of pointer to list_type type, which references the
 *   element which has been removed from the list in the loop body.
 *   The loop body is responsible for reusing or freeing the memory.
 *
 * You may safely break out of this loop or use the 'continue' statement. When
 * using break, goto or return to leave the loop, the list remains in a
 * consistent state, with the elements not yet removed remaining in the list.
 */
#define genc_slist_for_each_remove(removed_element, list_head, list_type, list_head_member_name) \
for ((removed_element = genc_slist_remove_object_at((list_head), list_type, list_head_member_name)); removed_element; (removed_element = genc_slist_remove_object_at((list_head), list_type, list_head_member_name)))

/* Example:
 *
 * void free_my_list(struct slist_head* l)
 * {
 *   struct my_list* d = NULL;
 *   genc_slist_for_each_remove(d, &l, struct my_list, list_head)
 *   {
 *     free(d);
 *   }
 * }
 */


#if defined(KERNEL) && defined(APPLE)
#undef ptrdiff_t
#endif

#endif
