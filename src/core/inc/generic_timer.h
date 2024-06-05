#ifndef __TIMER_H__
#define __TIMER_H__

#include <types.h>

static inline uint64_t generic_timer_get_freq(void)
{
    uint64_t cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (cntfrq));
    return cntfrq;
}

// Get timer value
static inline uint64_t generic_timer_read_counter()
{
    uint64_t timer_value = 0;
    __asm__ volatile(
        "isb\n\t"
        "mrs %0, cntpct_el0\n\t"
        : "=r"(timer_value)
    );

    return timer_value;
}

#endif