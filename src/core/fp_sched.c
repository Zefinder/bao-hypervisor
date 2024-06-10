#include <fp_sched.h>

#include <vm.h>
#include <cpu.h>
#include <spinlock.h>
#include <interrupts.h>
#include <ipi.h>
#include <generic_timer.h>

static spinlock_t memory_lock = SPINLOCK_INITVAL;
static volatile int64_t memory_requests[NUM_CPUS] = {[0 ... NUM_CPUS - 1] = TOKEN_NULL_PRIORITY};
static volatile struct memory_token memory_token = {TOKEN_NULL_OWNER, TOKEN_NULL_PRIORITY};
// TODO Add the ifdef thing
static volatile uint64_t next_fetch_time_array[NUM_CPUS] = {[0 ... NUM_CPUS - 1] = 0};
static volatile uint64_t current_memory_fetch_time[NUM_CPUS] = {[0 ... NUM_CPUS - 1] = 0};

uint64_t fp_request_access(uint64_t dec_prio)
{
    // The calling CPU id
    int cpu_id = cpu()->id;

    // Using increasing priorities for the rest of the code
    int64_t priority = (int64_t)(NUM_CPUS - dec_prio);

    spin_lock(&memory_lock);

    // TODO ifdef
    // If current time < next ok time then indicate how much time left to wait
    // Else it means that it's ok to check the priority
    uint64_t current_time = generic_timer_read_counter();
    uint64_t next_fetch_time = next_fetch_time_array[cpu_id];
    if (cpu_id != 0)
    {
        INFO("current_time=%ld, next_fetch_time=%ld (CPU %d)", current_time, next_fetch_time, cpu_id);
    }
    if (current_time < next_fetch_time)
    {
        union memory_request_answer answer = {{.ack = FP_REQ_RESP_NACK, .ttw = next_fetch_time - current_time}};
        spin_unlock(&memory_lock);
        return answer.raw;
    }

    // Setting request priority in the array of requests
    memory_requests[cpu_id] = priority;

    // If it has greater priority, then...
    if (priority > memory_token.priority)
    {
        // If someone (other than the current owner) had access to the memory, we send an IPI to pause
        if (memory_token.owner != TOKEN_NULL_OWNER && (uint64_t)memory_token.owner != cpu_id)
        {
            ipi_data_t ipi_data = {{0, IPI_IRQ_PAUSE}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);

            // TODO ifdef
            // Freeze the "fetch timer" for the one that was fetching
            // That means add the current time to the current memory time (current time - start time)
            current_memory_fetch_time[memory_token.owner] += current_time - next_fetch_time_array[memory_token.owner];
        }

        // Update the access token data
        memory_token.owner = (int64_t)cpu_id;
        memory_token.priority = priority;

        // TODO ifdef
        // Put in the next_fetch_array the start time (the value is not used when fetching)
        next_fetch_time_array[cpu_id] = current_time;
    }

    // Returning FP_REQ_RESP_ACK if memory access granted
    int got_token = (memory_token.owner == (int64_t)cpu_id);
    if (cpu_id != 0)
        INFO("END OF MEMORY REQUEST");
    spin_unlock(&memory_lock);

    return got_token ? FP_REQ_RESP_ACK : FP_REQ_RESP_NACK;
}

void fp_revoke_access()
{
    // The calling CPU id
    int cpu_id = cpu()->id;

    spin_lock(&memory_lock);

    // Remove request (even if not using it, e.g. timed out)
    memory_requests[cpu_id] = TOKEN_NULL_PRIORITY;

    // If owner of the access token then...
    if (memory_token.owner == (int64_t)cpu_id)
    {
        // TODO ifdef
        // Add the time to the current fetching time and compute next possible fetch (3 * time needed)
        uint64_t current_time = generic_timer_read_counter();
        uint64_t time_taken = current_memory_fetch_time[cpu_id] + (current_time - next_fetch_time_array[cpu_id]);
        next_fetch_time_array[cpu_id] = current_time + 3 * time_taken;
        if (cpu_id != 0)
        {
            INFO("current_time=%ld, time_taken=%ld, current_time+3*time_taken=%ld (CPU %d)", current_time, time_taken, current_time + 3 * time_taken, cpu_id);
        }
        
        // Reset time taken
        current_memory_fetch_time[cpu_id] = 0;

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
            ipi_data_t ipi_data = {{0, IPI_IRQ_RESUME}};
            send_ipi((cpuid_t)memory_token.owner, FPSCHED_EVENT, ipi_data);

            // TODO ifdef
            // Unfreeze the "fetch timer" for the new fetcher. That means reset the start time
            current_memory_fetch_time[memory_token.owner] = current_time;
        }
    }

    spin_unlock(&memory_lock);
}
