#include <fp_sched.h>

#include <vm.h>
#include <cpu.h>
#include <spinlock.h>
#include <interrupts.h>
#include <ipi.h>

static spinlock_t memory_lock = SPINLOCK_INITVAL;
static volatile int64_t memory_requests[NUM_CPUS] = {[0 ... NUM_CPUS - 1] = TOKEN_NULL_PRIORITY};
static volatile struct memory_token memory_token = {TOKEN_NULL_OWNER, TOKEN_NULL_PRIORITY};

uint64_t fp_request_access(uint64_t dec_prio)
{
    // The calling CPU id
    int cpu_id = cpu()->id;

    // Using increasing priorities for the rest of the code
    int64_t priority = (int64_t)(NUM_CPUS - dec_prio);

    spin_lock(&memory_lock);

    // Setting request priority in the array of requests
    memory_requests[cpu_id] = priority;

    // If it has greater priority, then...
    if (priority > memory_token.priority)
    {
        // If someone had access to the memory, we send an IPI to pause
        if (memory_token.owner != TOKEN_NULL_OWNER && (uint64_t)memory_token.owner != cpu_id)
        {
            // INFO("CPU %d takes the memory token from CPU %d!", cpu_id, memory_token.owner);
            ipi_data_t ipi_data = {{0, IPI_IRQ_PAUSE}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);
        }

        // Update the access token data
        memory_token.owner = (int64_t)cpu_id;
        memory_token.priority = priority;
    }

    // Returning FP_REQ_RESP_ACK if memory access granted
    int got_token = (memory_token.owner == (int64_t)cpu_id);
    spin_unlock(&memory_lock);

    return got_token ? FP_REQ_RESP_ACK : FP_REQ_RESP_NACK;
}

void fp_revoke_access()
{
    // The calling CPU id
    int cpu_id = cpu()->id;

    spin_lock(&memory_lock);
    // INFO("Revoke access for CPU %d", cpu_id);

    // Remove request (even if not using it, e.g. timed out)
    memory_requests[cpu_id] = TOKEN_NULL_PRIORITY;

    // If owner of the access token then...
    if (memory_token.owner == (int64_t)cpu_id)
    {
        // Reset token
        memory_token.owner = TOKEN_NULL_OWNER;
        memory_token.priority = TOKEN_NULL_PRIORITY;

        // Search for pending requests
        for (uint64_t cpu = 0; cpu < NUM_CPUS; ++cpu)
        {
            if (memory_requests[cpu] > memory_token.priority)
            {
                memory_token.priority = memory_requests[cpu];
                memory_token.owner = (int64_t)cpu;
            }
        }

        // If found, send an IPI to resume the task
        if (memory_token.owner != TOKEN_NULL_OWNER)
        {
            // INFO("Giving access to person waiting: CPU %d", memory_token.owner);
            ipi_data_t ipi_data = {{0, IPI_IRQ_RESUME}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);
            // INFO("Access given to CPU %d", memory_token.owner);
        }
    }

    spin_unlock(&memory_lock);
}
