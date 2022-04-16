#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <unistd.h>
#define MAX_CO_NUM 128
#define STACK_SIZE 64 * 1024

enum co_status {
    CO_NEW = 1, // 新创建，还未执行，可以从头运行
    CO_RUNNING, // 已经执行过，正在运行或等待被调度
    CO_WAITING, // 在co_wait上等待
    CO_DEAD,    // 已经结束，但资源未释放
};

struct co {
    const char *name;
    void (*func)(void*);
    void *arg;

    enum co_status  status;
    jmp_buf         context; // saved registers (setjmp.h)
    struct co *     waiter;  // Being waiting by other co
    uint8_t         stack[STACK_SIZE];
};

int current_id = 0;
struct co* coroutines[MAX_CO_NUM];

void co_wait(struct co*);
struct co* current_co();

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
    asm volatile (
#if __x86_64__
    "movq %0, %%rsp; call *%1"
      : : "r"((uintptr_t)sp), "r"(entry), "D"(arg)
#else
    "movl %0, %%esp; pushl %2;  call *%1"
      : : "r"((uintptr_t)sp), "r"(entry), "r"(arg) 
#endif
    );
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    int i;
    for (i = 0; i < MAX_CO_NUM; i++) {
        if (coroutines[i] == NULL) {
            struct co* new_co = malloc(sizeof(struct co));
            new_co->name = name;
            new_co->func = func;
            new_co->arg = arg;
            new_co->status = CO_NEW;
            new_co->waiter = NULL;
            for (int i = 0; i < STACK_SIZE; i++) {
                new_co->stack[i] = 0;
            }
            coroutines[i] = new_co;
            return new_co;
        }
    }
    return NULL;
}

__attribute__((constructor)) void add_main() {
    struct co* main_co = malloc(sizeof(struct co));
    main_co->name = "main";
    main_co->func = NULL;
    main_co->arg = NULL;
    main_co->status = CO_RUNNING;
    main_co->waiter = NULL;
    coroutines[0] = main_co;
}


void co_wait(struct co *co) {
    if (co == NULL) {
        return;
    } else if (co->waiter != NULL) {
        printf("double waiter error!\n");
        return;
    } else if (co->status == CO_DEAD) {
        free(co);
        return;
    }

    current_co()->status = CO_WAITING;
    co->waiter = current_co();
    co_yield();
    // come back when co is finished
    free(co);
}

void _scheduler() {
    while(1) {
        int i = current_id;
        // choose a coroutine co
        do {
            i = (i + 1) % MAX_CO_NUM;
            if (coroutines[i] 
                && coroutines[i]->status!= CO_WAITING) {
                break;
            }
        } while (1);
        struct co* c = coroutines[i];
        current_id = i;
        if (c->status == CO_NEW) {
            c->status = CO_RUNNING;
            stack_switch_call((void*)(c->stack + STACK_SIZE), c->func, (uintptr_t)c->arg);
            // if this coroutine returns, mark it as finished
            current_co()->status = CO_DEAD;
        } else if (c->status == CO_RUNNING) {
            longjmp(c->context, 1);
        } else if (c->status == CO_DEAD && c->waiter && c->waiter->status == CO_WAITING) {
            c->waiter->status = CO_RUNNING;
            c->waiter = NULL;
        }
    }
}

void co_yield() {
    int val = setjmp(current_co()->context);
    if (val == 0) {
        _scheduler(); 
    }
}

struct co* current_co() {
    return coroutines[current_id];
}
