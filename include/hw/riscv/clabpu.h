#ifndef HW_RISCV_CLABPU_H
#define HW_RISCV_CLABPU_H

#include "hw/boards.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/sysbus.h"

#define TYPE_CLABPU_MACHINE MACHINE_TYPE_NAME("clabpu")
typedef struct CLabPUState CLabPUState;
DECLARE_INSTANCE_CHECKER(CLabPUState, CLABPU_MACHINE, TYPE_CLABPU_MACHINE)

enum {
	CLABPU_MROM,
	CLABPU_HTIF,
	CLABPU_CLINT,
	CLABPU_INTC_ADDR,
	CLABPU_TIMER_ADDR,
	CLABPU_DRAM,
};

void clabpu_machine_init(ObjectClass *oc, void *data);

struct CLabPUState {
	/*< private >*/
	MachineState parent;

	/*< public >*/
	RISCVHartArrayState soc;
	DeviceState *intc;
	DeviceState *edc;
};

#endif // HW_RISCV_CLABPU_H