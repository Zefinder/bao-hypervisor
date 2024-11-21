#ifndef __FP_SCHED_H__
#define __FP_SCHED_H__

#include <bao.h>

/* Memory token */
#define TOKEN_NULL_OWNER -1
#define TOKEN_NULL_PRIORITY -1

struct memory_token
{
    uint64_t owner;
    uint64_t priority;
};

/* Answer of hypervisor after a memory request */
union memory_request_answer
{
    struct
    {
        uint64_t ack:1;     // 0 = no, 1 = yes
        uint64_t ttw:63;    // Time to spend in low prio
    };
    uint64_t raw;
};

/* Memory request answers */
#define FP_REQ_RESP_ACK 1
#define FP_REQ_RESP_NACK 0

/* Ask for arbitration access with a given priority (priority decreases with
 * higher numbers). */
uint64_t request_memory_access(uint64_t priority, uint64_t arg);

/* Give back the access permissions. */
void revoke_memory_access(void);

/* Updates the priority for the memory access */
uint64_t update_memory_access(uint64_t priority);

#endif