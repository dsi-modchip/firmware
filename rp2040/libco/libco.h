// vim: set et:

/* derived from libco v20, by byuu (ISC) */

#ifndef LIBCO_H_
#define LIBCO_H_

#define LIBCO_SMALL_IMPL 1

typedef void* cothread_t;

cothread_t co_active(void);
cothread_t co_derive(void* memory, unsigned int heapsize, void (*coentry)(void));
void co_switch_base(cothread_t);
int co_serializable(void);

#if LIBCO_SMALL_IMPL
inline static void co_switch(cothread_t t) {
    //asm volatile("bkpt #69\n");
    //co_switch_base(t);
    asm volatile(
        "mov r0, %[t]\n"
        "bl co_switch_base\n"
        "dsb\n"
        "isb\n"
        :[t]"+r"(t)
        :
        :"memory","cc",
        /*"r0",*/"r1","r2","r3",
        "r4","r5","r6","r7","r8","r9",
        "r10","r11","r12","r14"
    );
    //asm volatile("bkpt #42\n");
}
#else
#define co_switch(t) do{co_switch_base(t);asm volatile("dsb\nisb\n":::"memory");}while(0)
#endif

#endif

