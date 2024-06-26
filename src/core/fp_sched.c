#include <fp_sched.h>

#include <vm.h>
#include <cpu.h>
#include <spinlock.h>
#include <interrupts.h>
#include <ipi.h>
#include <generic_timer.h>
#include <platform_defs_gen.h>

static spinlock_t memory_lock = SPINLOCK_INITVAL;
static volatile int64_t memory_requests[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = TOKEN_NULL_PRIORITY};
static volatile struct memory_token memory_token = {TOKEN_NULL_OWNER, TOKEN_NULL_PRIORITY};

#ifdef MEMORY_REQUEST_WAIT
static volatile uint64_t next_fetch_time_array[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};
static volatile uint64_t current_memory_fetch_time[PLAT_CPU_NUM] = {[0 ... PLAT_CPU_NUM - 1] = 0};
#endif

uint64_t fp_request_access(uint64_t dec_prio)
{
    // The calling CPU id
    int cpu_id = cpu()->id;
    

    // Using increasing priorities for the rest of the code
    int64_t priority = (int64_t)(PLAT_CPU_NUM - dec_prio);

    spin_lock(&memory_lock);

#ifdef MEMORY_REQUEST_WAIT
    // If current time < next ok time then indicate how much time left to wait
    // Else it means that it's ok to check the priority
    uint64_t current_time = generic_timer_read_counter();
    uint64_t next_fetch_time = next_fetch_time_array[cpu_id];
    if (current_time < next_fetch_time)
    {
        union memory_request_answer answer = {{.ack = FP_REQ_RESP_NACK, .ttw = next_fetch_time - current_time}};
        spin_unlock(&memory_lock);
        return answer.raw;
    }
#endif

    // Setting request priority in the array of requests
    memory_requests[cpu_id] = priority;

    // If it has greater priority, then...
    if (priority > memory_token.priority)
    {

        // If someone (other than the current owner) had access to the memory, we send an IPI to pause
        // Try to have the most precise time, after sending IPI is the best!
        if (memory_token.owner != TOKEN_NULL_OWNER && (uint64_t)memory_token.owner != cpu_id)
        {
            ipi_data_t ipi_data = {{0, IPI_IRQ_PAUSE}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);

        #ifdef MEMORY_REQUEST_WAIT
            // Freeze the "fetch timer" for the one that was fetching
            // That means add the current time to the current memory time (current time - start time)
            current_time = generic_timer_read_counter();
            current_memory_fetch_time[memory_token.owner] += current_time - next_fetch_time_array[memory_token.owner];
        } 
        else 
        {
            current_time = generic_timer_read_counter();
        }
        #else
        }
        #endif

    #ifdef MEMORY_REQUEST_WAIT
        // Put in the next_fetch_array the start time (the value is not used when fetching)
        next_fetch_time_array[cpu_id] = current_time;
        current_time = generic_timer_read_counter();
    #endif
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

    // Remove request (even if not using it, e.g. timed out, IPI pause arrived after hypervisor mode)
    memory_requests[cpu_id] = TOKEN_NULL_PRIORITY;

    // If owner of the access token then...
    if (memory_token.owner == (int64_t)cpu_id)
    {
    #ifdef MEMORY_REQUEST_WAIT
        // Add the time to the current fetching time and compute next possible fetch (3 * time needed)
        uint64_t current_time = generic_timer_read_counter();
        uint64_t time_taken = current_memory_fetch_time[cpu_id] + (current_time - next_fetch_time_array[cpu_id]);
        next_fetch_time_array[cpu_id] = current_time + 3 * time_taken;

        // Reset time taken
        current_memory_fetch_time[cpu_id] = 0;
    #endif

        // Reset token
        memory_token.owner = TOKEN_NULL_OWNER;
        memory_token.priority = TOKEN_NULL_PRIORITY;

        // Search for pending requests
        for (uint64_t cpu = 0; cpu < PLAT_CPU_NUM; ++cpu)
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
            ipi_data_t ipi_data = {{0, IPI_IRQ_RESUME}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);

        #ifdef MEMORY_REQUEST_WAIT
            // Unfreeze the "fetch timer" for the new fetcher. That means reset the start time
            current_time = generic_timer_read_counter();
            next_fetch_time_array[memory_token.owner] = current_time;
        #endif
        }
    }

    spin_unlock(&memory_lock);
}
