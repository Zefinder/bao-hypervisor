/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <hypercall.h>
#include <cpu.h>
#include <vm.h>
#include <ipc.h>
#include "scheds/inc/sched.h"
#include <ipi.h>
#include <generic_timer.h>

long int hypercall(unsigned long id)
{
    long int ret = -HC_E_INVAL_ID;

    unsigned long arg0 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(0));
    unsigned long arg1 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(1));
    unsigned long arg2 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(2));
    ipi_data_t data;
    uint64_t start_time, end_time;

    switch (id) {
        case HC_IPC:
            ret = ipc_hypercall(arg0, arg1, arg2);
            break;
        case HC_REQUEST_MEM_ACCESS:
            // Call memory access
            ret = request_memory_access(arg0, arg1);
            break;
        case HC_REVOKE_MEM_ACCESS:
            // Call revoke memory
            revoke_memory_access();
            break;
        case HC_GET_CPU_ID:
            ret = cpu()->id;
            break;
        case HC_NOTIFY_CPU:
            // arg0 is the cpuid
            data.data = 0;
            data.interrupt_number = IPI_IRQ_PAUSE;
            send_ipi(arg0, FPSCHED_EVENT, data);
            break;
        case HC_EMPTY_CALL:
            // Nothing...
            break;
        case HC_REQUEST_MEM_ACCESS_TIMER: 
            // Call memory access but timing it
            start_time = generic_timer_read_counter();
            request_memory_access(arg0, arg1);
            end_time = generic_timer_read_counter();
            ret = end_time - start_time;
            break;
        case HC_DISPLAY_RESULTS:
            // Uses the 3 arguments for result display
            // Of the form: [core number]:[arg0],[arg1],[arg2]
            // No need to lock, there is already a spinlock
            INFO("%d:%d,%d,%d", cpu()->id, arg0, arg1, arg2);
            break;
        case HC_MEASURE_IPI:
            data.data = 0;
            data.interrupt_number = IPI_IRQ_TEST;
            // Send an IPI and measure time. This time will be compared when received by the OS
            send_ipi(cpu()->id, FPSCHED_EVENT, data);
            ret = generic_timer_read_counter();
            break;
        case HC_REVOKE_MEM_ACCESS_TIMER: 
            // Call memory access but timing it
            start_time = generic_timer_read_counter();
            revoke_memory_access();
            end_time = generic_timer_read_counter();
            ret = end_time - start_time;
            break;
        case HC_UPDATE_MEM_ACCESS:
            // Update memory access
            update_memory_access(arg0);
            break;
        default:
            WARNING("Unknown hypercall id %d", id);
    }

    return ret;
}
