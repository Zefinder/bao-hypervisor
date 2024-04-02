#include <fp_sched.h>

#include <vm.h>
#include <cpu.h>
#include <spinlock.h>
#include <interrupts.h>

#define TOKEN_NULL_OWNER    -1
#define TOKEN_NULL_PRIORITY -1

static spinlock_t memory_lock                     = SPINLOCK_INITVAL;
static volatile int64_t memory_requests[NUM_CPUS] = { [0 ... NUM_CPUS-1] = TOKEN_NULL_PRIORITY };
static volatile int64_t token_owner               = TOKEN_NULL_OWNER;
static volatile int64_t token_priority            = TOKEN_NULL_PRIORITY;

enum { INJECT_SGI = 1 };

void inter_vm_irq_handler(uint32_t event, uint64_t data) {
    if(event == INJECT_SGI) {
        interrupts_vm_inject(cpu.vcpu->vm, data);
    }
}
CPU_MSG_HANDLER(inter_vm_irq_handler, INTER_VM_IRQ);

#define CPU_MSG(handler,event,data) (&(cpu_msg_t){handler, event, data})

uint64_t fp_request_access(uint64_t dec_prio)
{
    // using increasing priorities for the rest of the code
    int64_t priority = (int64_t)(NUM_CPUS - dec_prio);

    spin_lock(&memory_lock);

    memory_requests[cpu.id] = priority;

    if (priority > token_priority) {
        if (token_owner != TOKEN_NULL_OWNER &&
            (uint64_t)token_owner != cpu.id) {
//            INFO("Send interrupt to reclaim the token from %d and give to %d",
//                 token_owner,
//                 cpu.id);
            cpu_send_msg((uint64_t)token_owner, 
                         CPU_MSG(INTER_VM_IRQ,
                                 INJECT_SGI,
                                 FP_IPI_PAUSE));
        }
        token_owner    = (int64_t)cpu.id;
        token_priority = priority;
    }

    int got_token = (token_owner == (int64_t)cpu.id);

    spin_unlock(&memory_lock);

    return got_token ? FP_REQ_RESP_ACK : FP_REQ_RESP_NACK;
}

void fp_revoke_access()
{
    spin_lock(&memory_lock);

    memory_requests[cpu.id] = TOKEN_NULL_PRIORITY;

    if(token_owner == (int64_t)cpu.id)
    {
        token_owner    = TOKEN_NULL_OWNER;
        token_priority = TOKEN_NULL_PRIORITY;

        for (uint64_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
            if (memory_requests[cpu] > token_priority) {
                token_priority = memory_requests[cpu];
                token_owner    = (int64_t)cpu;
            }
        }

        if (token_owner != TOKEN_NULL_OWNER) {
//            INFO("Send interrupt to pass the token from %d to %d", cpu.id, token_owner);
            cpu_send_msg((uint64_t)token_owner,
                         CPU_MSG(INTER_VM_IRQ,
                                 INJECT_SGI,
                                 FP_IPI_RESUME));
        }
    }

    spin_unlock(&memory_lock);
}
