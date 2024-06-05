/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <hypercall.h>
#include <cpu.h>
#include <vm.h>
#include <ipc.h>
#include <fp_sched.h>
#include <ipi.h>
#include <generic_timer.h>

long int hypercall(unsigned long id)
{
    long int ret = -HC_E_INVAL_ID;

    unsigned long arg0 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(0));
    unsigned long arg1 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(1));
    unsigned long arg2 = vcpu_readreg(cpu()->vcpu, HYPCALL_ARG_REG(2));

    switch (id) {
        case HC_IPC:
            ret = ipc_hypercall(arg0, arg1, arg2);
            break;
        case HC_REQUEST_MEM_ACCESS:
            // Call memory access
            ret = fp_request_access(arg0);
            break;
        case HC_REVOKE_MEM_ACCESS:
            // Call revoke memory
            fp_revoke_access();
            break;
        case HC_GET_CPU_ID:
            ret = cpu()->id;
            break;
        case HC_NOTIFY_CPU:
            // arg0 is the cpuid
            ipi_data_t data = {{.data = 0, .interrupt_number = IPI_IRQ_PAUSE}};
            send_ipi(arg0, FPSCHED_EVENT, data);
            break;
        case HC_EMPTY_CALL:
            // Nothing...
            break;
        case HC_REQUEST_MEM_ACCESS_TIMER: 
            // Call memory access but timing it
            uint64_t start_time = generic_timer_read_counter();
            fp_request_access(arg0);
            uint64_t end_time = generic_timer_read_counter();
            ret = end_time - start_time;
            break;
        default:
            WARNING("Unknown hypercall id %d", id);
    }

    return ret;
}
