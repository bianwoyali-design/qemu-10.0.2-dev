#ifndef HW_RISCV_CLABPU_EDC_H
#define HW_RISCV_CLABPU_EDC_H

#include "hw/sysbus.h"

/* EDC Memory Map */
#define CLABPU_EDC_REG1_BASE 0x20001000
#define CLABPU_EDC_REG1_SIZE 0x1000
#define CLABPU_EDC_ERR_BASE 0x20002000
#define CLABPU_EDC_ERR_SIZE 0x2000

/* EDC Register offsets */
#define EDC_CTRL_REG 0x00
#define EDC_STATUS_REG 0x04
#define EDC_ERROR_REG 0x08
#define EDC_INT_ENABLE_REG 0x0C
#define EDC_INT_STATUS_REG 0x10

#define TYPE_CLABPU_EDC "clabpu-edc"
typedef struct CLabPUEdcState CLabPUEdcState;
OBJECT_DECLARE_SIMPLE_TYPE(CLabPUEdcState, CLABPU_EDC)

struct CLabPUEdcState {
	/*< private >*/
	SysBusDevice parent_obj;

	/*< public >*/
	MemoryRegion reg1;
	MemoryRegion err;

	/* Interal state */
	uint32_t ctrl;
	uint32_t status;
	uint32_t error;
	uint32_t int_enable;
	uint32_t int_status;

	/* IRQ line */
	qemu_irq irq;
};

/* Memory map entry for EDC in clabpu.h */
enum {
	CLABPU_MMAP_EDC_REG = CLABPU_EDC_REG1_BASE,
};

#endif /* HW_RISCV_CLABPU_EDC_H */