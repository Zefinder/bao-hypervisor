#ifndef __IPI_H__
#define __IPI_H__
#include <types.h>

/* IPI events */
enum ipi_event { FPSCHED_EVENT = 1 };

/* IPI interruptions */
#define IPI_IRQ_CPU     5
#define IPI_IRQ_PAUSE   6
#define IPI_IRQ_RESUME  7

#define IPI_BROADCAST (cpuid_t)-1

/* IPI data */
typedef union ipi_data {
    struct {
        uint32_t data;
        uint32_t interrupt_number;
    } ;

    uint64_t raw;
} ipi_data_t;

void send_ipi(cpuid_t trgtcpu, enum ipi_event, ipi_data_t ipi_data);

#endif