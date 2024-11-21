#include "inc/sched.h"

#include <vm.h>
#include <cpu.h>
#include <spinlock.h>
#include <interrupts.h>
#include <ipi.h>
#include <generic_timer.h>
#include <platform_defs_gen.h>

static spinlock_t memory_lock = SPINLOCK_INITVAL;
static volatile uint64_t memory_requests[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = TOKEN_NULL_PRIORITY};
static volatile struct memory_token memory_token = {(uint64_t)TOKEN_NULL_OWNER, (uint64_t)TOKEN_NULL_PRIORITY};

// Highest priority => closer number to 0 (0 being the highest)
uint64_t request_memory_access(uint64_t priority, uint64_t arg)
{
    // The calling CPU id
    uint64_t cpu_id = cpu()->id;

    spin_lock(&memory_lock);

    // Setting request priority in the array of requests
    memory_requests[cpu_id] = priority;

    // If it has lower priority, then...
    if (priority < memory_token.priority)
    {

        // If someone (other than the current owner) had access to the memory, we send an IPI to pause
        // Try to have the most precise time, after sending IPI is the best!
        if (memory_token.owner != (uint64_t)TOKEN_NULL_OWNER && memory_token.owner != cpu_id)
        {
            ipi_data_t ipi_data = {{0, IPI_IRQ_PAUSE}};
            send_ipi(memory_token.owner, FPSCHED_EVENT, ipi_data);
        }

        // Update the access token data
        memory_token.owner = cpu_id;
        memory_token.priority = priority;
    }

    // Returning FP_REQ_RESP_ACK if memory access granted
    int got_token = (memory_token.owner == cpu_id);
    spin_unlock(&memory_lock);

    return got_token ? FP_REQ_RESP_ACK : FP_REQ_RESP_NACK;
}

void revoke_memory_access()
{
    // The calling CPU id
    uint64_t cpu_id = cpu()->id;

    spin_lock(&memory_lock);

    // Remove request (even if not using it, e.g. timed out, IPI pause arrived after hypervisor mode)
    memory_requests[cpu_id] = TOKEN_NULL_PRIORITY;

    // If owner of the access token then...
    if (memory_token.owner == cpu_id)
    {
        // Reset token
        memory_token.owner = (uint64_t)TOKEN_NULL_OWNER;
        memory_token.priority = (uint64_t)TOKEN_NULL_PRIORITY;

        // Search for pending requests
        for (uint64_t cpu = 0; cpu < PLAT_CPU_NUM; ++cpu)
        {
            if (memory_requests[cpu] < memory_token.priority)
            {
                memory_token.priority = memory_requests[cpu];
                memory_token.owner = cpu;
            }
        }

        // If found, send an IPI to resume the task
        if (memory_token.owner != TOKEN_NULL_OWNER)
        {
            ipi_data_t ipi_data = {{0, IPI_IRQ_RESUME}};
            send_ipi(memory_token.owner, FPSCHED_EVENT, ipi_data);
        }
    }

    spin_unlock(&memory_lock);
}

uint64_t update_memory_access(uint64_t priority)
{
    // Just call request access with a dummy arg1
    return request_memory_access(priority, 0);
}