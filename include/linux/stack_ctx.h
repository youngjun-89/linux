#ifndef __STACK_CTX_H__
#define __STACK_CTX_H__

#include <linux/gfp.h>

typedef void* stack_ctx_handle_t;
struct stack_ctx_desc *stack_ctx_get(stack_ctx_handle_t handle);
void stack_ctx_put(stack_ctx_handle_t handle);
stack_ctx_handle_t stack_ctx_create(int size, gfp_t gfp);
void stack_ctx_destroy(stack_ctx_handle_t handle);
void *stack_ctx_find(struct task_struct *tsk, stack_ctx_handle_t handle);
void stack_ctx_reserve(void *stack);
#endif
