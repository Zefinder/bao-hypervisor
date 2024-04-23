#ifndef __FP_SCHED_H__
#define __FP_SCHED_H__

#include <bao.h>

/* CONFIG */
// TODO Change because ugly
// How many different cpus will compete for the access?
#define NUM_CPUS 8

/* Memory token */
#define TOKEN_NULL_OWNER -1
#define TOKEN_NULL_PRIORITY -1

struct memory_token
{
    int64_t owner;
    int64_t priority;
};

/* Memory request answers */
#define FP_REQ_RESP_ACK 1
#define FP_REQ_RESP_NACK 0

/* Ask for arbitration access with a given priority (priority decreases with
 * higher numbers). */
uint64_t fp_request_access(uint64_t dec_prio);

/* Give back the access permissions. */
void fp_revoke_access(void);

#endif