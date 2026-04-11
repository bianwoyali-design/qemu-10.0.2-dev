#ifndef HW_INTC_CLABPU_INTC_H
#define HW_INTC_CLABPU_INTC_H

#include "hw/sysbus.h"

/* Interrupt Controller Memory Map */
#define CLABPU_INTC_BASE 0x10000000
#define CLABPU_INTC_SIZE 0x1000

#define CLABPU_INTC_NUM_SOURCES 32

#define INTC_IRQ_PENDING 0x00 /* Interrupt pending register */
#define INTC_IRQ_ENABLE 0x04 /* Interrupt enable register */
#define INTC_IRQ_PRIORITY_BASE 0x08 /* Priority registers (0x08-0x87) */
#define INTC_IRQ_STATUS 0x8C /* Current interrupt status */

#define TYPE_CLABPU_INTC "clabpu-intc"
typedef struct CLabPUIntcState CLabPUIntcState;
OBJECT_DECLARE_SIMPLE_TYPE(CLabPUIntcState, CLABPU_INTC)

struct CLabPUIntcState {
	/*< private >*/
	SysBusDevice parent_obj;

	/*< public >*/
	MemoryRegion iomem;
	uint32_t pending;
	uint32_t enable;
	uint8_t priority[CLABPU_INTC_NUM_SOURCES];
	qemu_irq cpu_irq;
};

enum {
	CLABPU_IRQ_EDC_ERR = 0,
	CLABPU_IRQ_TIMER = 1,
	CLABPU_IRQ_GPIO = 2,
};

#endif // HW_INTC_CLABPU_INTC_H