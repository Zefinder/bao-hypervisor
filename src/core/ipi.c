#include <ipi.h>
#include <vm.h>

#define CPU_MSG(handler, event, data) (&(struct cpu_msg){handler, event, data})

void ipi_send_handler(uint32_t event, uint64_t data)
{
    ipi_data_t ipi_data = {.raw = data};

    switch (event)
    {
    case FPSCHED_EVENT:
        INFO("Sending IPI interrupt %d to CPU %d", ipi_data.interrupt_number, cpu()->id)
        vcpu_inject_hw_irq(cpu()->vcpu, ipi_data.interrupt_number);
        break;
    }
}

CPU_MSG_HANDLER(ipi_send_handler, INTER_VM_IRQ);
void send_ipi(cpuid_t trgtcpu, enum ipi_event event, ipi_data_t ipi_data)
{
    // Notify only the target CPU
    cpu_send_msg(trgtcpu, CPU_MSG(INTER_VM_IRQ, event, ipi_data.raw));
}