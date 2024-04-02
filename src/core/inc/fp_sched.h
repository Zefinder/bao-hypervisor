#ifndef FP_SCHED_H
#define FP_SCHED_H

#include <bao.h>

/* CONFIG */
// How many different cpus will compete for the access?
#define NUM_CPUS		8


/* CONSTANTS */
#define FP_REQ_RESP_ACK		0
#define FP_REQ_RESP_NACK	1
#define FP_IPI_PAUSE		6
#define FP_IPI_RESUME		7


/* Ask for arbitration access with a given priority (priority decreases with
 * higher numbers). */
uint64_t fp_request_access(uint64_t dec_prio);

/* Give back the access permissions. */
void fp_revoke_access(void);

#endif // FP_SCHED_H
