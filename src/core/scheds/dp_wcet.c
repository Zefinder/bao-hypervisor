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
static volatile uint64_t execution_time[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};
static volatile uint64_t start_prefetch[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};
static volatile uint64_t next_prefetch[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};
static volatile uint64_t prefetch_time[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};

// Highest priority => closer number to 0 (0 being the highest)
uint64_t request_memory_access(uint64_t priority, uint64_t wcet)
{
    // The calling CPU id
    uint64_t cpu_id = cpu()->id;

    spin_lock(&memory_lock);

    // If current time < next ok time then indicate how much time left to wait
    // Else it means that it's ok to check the priority
    uint64_t current_time = generic_timer_read_counter();
    uint64_t low_prio_time = 0;
    uint64_t current_priority = priority;
    if (current_time < next_prefetch[cpu_id])
    {
        // Set time being in low prio for the cpu
        low_prio_time = next_prefetch[cpu_id] - current_time;

        // Adding the number of CPU to the priorty (lower it)
        current_priority += PLAT_CPU_NUM;
    }

    // Setting request priority in the array of requests
    memory_requests[cpu_id] = current_priority;

    // Save the execution time
    execution_time[cpu_id] = wcet;

    // If it has lower priority, then...
    if (current_priority < memory_token.priority)
    {

        // If someone (other than the current owner) had access to the memory, we send an IPI to pause
        // Try to have the most precise time, after sending IPI is the best!
        if (memory_token.owner != (uint64_t)TOKEN_NULL_OWNER && memory_token.owner != cpu_id)
        {
            ipi_data_t ipi_data = {{0, IPI_IRQ_PAUSE}};
            send_ipi(memory_token.owner, FPSCHED_EVENT, ipi_data);

            // Freeze the "fetch timer" for the one that was fetching
            // That means add the current time to the current memory time (current time - start time)
            current_time = generic_timer_read_counter();
            prefetch_time[memory_token.owner] += current_time - start_prefetch[memory_token.owner];
        }

        // Put in the next_fetch_array the start time (the value is not used when fetching)
        current_time = generic_timer_read_counter();
        start_prefetch[cpu_id] = current_time;

        // Update the access token data
        memory_token.owner = cpu_id;
        memory_token.priority = current_priority;
    }

    // Returning the answer and the time in low prio
    int got_token = (memory_token.owner == cpu_id);
    spin_unlock(&memory_lock);

    union memory_request_answer answer = {{.ack = got_token ? FP_REQ_RESP_ACK : FP_REQ_RESP_NACK}, .ttw = low_prio_time};
    return answer.raw;
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
        // Add the time to the current fetching time and compute next possible fetch (3 * time needed)
        uint64_t current_time = generic_timer_read_counter();
        uint64_t time_taken = prefetch_time[cpu_id] + (current_time - start_prefetch[cpu_id]);
        next_prefetch[cpu_id] = current_time + (execution_time[cpu_id] - time_taken);

        // Reset time taken
        prefetch_time[cpu_id] = 0;

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

            // Unfreeze the "fetch timer" for the new fetcher. That means reset the start time
            current_time = generic_timer_read_counter();
            strcat[memory_token.owner] = current_time;
        }
    }

    spin_unlock(&memory_lock);
}

void update_memory_access(uint64_t priority)
{
    // Just call request access with the save wcet
    // No need to spinlock, only this cpu modifies its wcet
    uint64_t cpu_id = cpu()->id;
    request_memory_access(priority, execution_time[cpu_id]);
}