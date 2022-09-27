#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/sched.h>
#include "stack_ctx.h"

#define STACK_CTX_SIZE 	1024 * 4

spinlock_t stack_ctx_desc_lck;
struct list_head stack_ctx_descs;
int stack_ctx_total_size;

struct stack_ctx_hdr {
	/* stack ctx desc */
	stack_ctx_handle_t handle;
	/* size */
	int size; /* TODO remove. handle can be used */
};

struct stack_ctx_desc {
	/* stack ctx descriptor reference count */
	atomic_t refcnt;
	/* stack ctx list */
	struct list_head next;
	/* rcu callback */
	struct rcu_head head;
	/* sizeof memory */
	int size;
};

#ifdef CONFIG_STACK_CTX
struct stack_ctx_desc *stack_ctx_get(stack_ctx_handle_t handle)
{
	struct stack_ctx_desc *desc = (struct stack_ctx_desc *)handle;
	if (!atomic_inc_not_zero(&desc->refcnt))
		return NULL;

	return desc;
}

void stack_ctx_put(stack_ctx_handle_t handle)
{
	struct stack_ctx_desc *desc = (struct stack_ctx_desc *)handle;
	if (!atomic_dec_and_test(&desc->refcnt))
		return;
	
	spin_lock(&stack_ctx_desc_lck);
	stack_ctx_total_size -= desc->size;
	list_del(&desc->next);
	spin_unlock(&stack_ctx_desc_lck);
	kfree_rcu(&desc->head);
}

stack_ctx_handle_t stack_ctx_create(int size, gfp_t gfp)
{
	struct stack_ctx_desc *desc;
	
	desc = kmalloc(sizeof(*desc), gfp);
	if (!desc)
		return NULL;
	
	atomic_set(&desc->refcnt, 1);
	desc->size = size;

	spin_lock(&stack_ctx_desc_lck);
	if (stack_ctx_total_size > size + STACK_CTX_SIZE) {
		kfree(desc);
		desc = NULL;
	} else {
		stack_ctx_total_size += size;
		list_add(&desc->next, &stack_ctx_descs);
	}
	spin_unlock(&stack_ctx_desc_lck);

	return desc;
}

void stack_ctx_destroy(stack_ctx_handle_t handle)
{
	stack_ctx_put((struct stack_ctx_desc *)handle);
}

void *stack_ctx_find(struct task_struct *tsk, stack_ctx_handle_t handle)
{	
	struct stack_ctx_hdr *hdr;
	struct stack_ctx_desc *desc = (struct stack_ctx_desc *)handle;

	if (tsk->stack_ctx_end == NULL)
		return NULL;
	
	hdr = (struct stack_ctx_hdr *)tsk->stack;
	do {
		if (hdr->handle == desc && hdr->size == desc->size)
			return (void *)((char *)hdr + sizeof(*hdr));
		hdr = (struct stack_ctx_hdr *)((char *)hdr + sizeof(*hdr) + hdr->size);
	} while ((void *)hdr < tsk->stack_ctx_end);

	return NULL;
}

/* entry handler calls it */
void stack_ctx_reserve(void *stack)
{
	struct stack_ctx_desc *desc;
	struct stack_ctx_hdr hdr;

	/* TODO */
// config option
	// arch_stack_reserve(stack);
// spanning
	// free task stack
	// realloc task stack
	// arch_stack_reserve(stack);
	// arch_stack_span(stack);

	rcu_read_lock();
	list_for_each_entry(desc, &stack_ctx_descs, next) {
		hdr.handle = (void *)desc;
		hdr.size = desc->size;
		memcpy(stack, &hdr, sizeof(struct stack_ctx_hdr));
		stack = (void *)((char *)stack + sizeof(struct stack_ctx_hdr) + hdr.size);
		current->stack_ctx_end += hdr.size;
	}
	rcu_read_unlock();
	return;
}
#else
struct stack_ctx_desc *stack_ctx_get(stack_ctx_handle_t handle)
{
	return NULL;
}

void stack_ctx_put(stack_ctx_handle_t handle)
{
	return;
}

stack_ctx_handle_t stack_ctx_create(int size, gfp_t gfp)
{
	return NULL;
}

void stack_ctx_destroy(stack_ctx_handle_t handle)
{
	return;
}

void *stack_ctx_find(struct task_struct *tsk, stack_ctx_handle_t handle)
{
	return NULL;
}

void stack_ctx_reserve(void *stack)
{
	return;
}
#endif
