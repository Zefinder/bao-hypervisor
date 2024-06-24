#ifndef __IPI_H__
#define __IPI_H__
#include <types.h>

/* IPI events */
enum ipi_event { FPSCHED_EVENT = 1 };

/* IPI interruptions */
#define IPI_IRQ_TEST    0
#define IPI_IRQ_CPU     5
#define IPI_IRQ_PAUSE   6
#define IPI_IRQ_RESUME  7

#define IPI_BROADCAST (cpuid_t)-1

/* 
 * This structure contains the data to sent to the IPI as well as the interrupt
 * number to use.
 */
typedef union ipi_data {
    struct {
        uint32_t data;
        uint32_t interrupt_number;
    } ;

    uint64_t raw;
} ipi_data_t;

/*
 * This is a function used to send an IPI to the targetted CPU [trgtcpu] with an
 * event and a data structure
 */
void send_ipi(cpuid_t trgtcpu, enum ipi_event event, ipi_data_t ipi_data);

#endif