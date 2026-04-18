#ifndef HW_TIMER_CLABPU_TIMER_H
#define HW_TIMER_CLABPU_TIMER_H

#include "hw/sysbus.h"

/* Timer Memory Map */
#define CLABPU_TIMER_BASE 0x10001000
#define CLABPU_TIMER_SIZE 0x1000

/* Timer register offsets */
#define CLABPU_TIMER_COUNTER 0x00 /* R/W: Timer counter value */
#define CLABPU_TIMER_CONTROL 0x04 /* R/W: Timer control register */
#define CLABPU_TIMER_STATUS 0x08 /* R/W1C: Timer status register */
#define CLABPU_TIMER_PRESCALER 0x0C /* R/W: Timer prescaler */

#define CLABPU_TIMER_CTRL_ENABLE (1 << 0) /* Timer enable */
#define CLABPU_TIMER_CTRL_INT_EN (1 << 1) /* Interrupt enable */
#define CLABPU_TIMER_STAT_EXPIRED (1 << 0) /* Timer expired */

#define CLABPU_TIMER_FREQ_HZ 1000000 /* 1MHz default frequency */

#define TYPE_CLABPU_TIMER "clabpu-timer"
OBJECT_DECLARE_SIMPLE_TYPE(CLabPUTimerState, CLABPU_TIMER)

typedef struct clabpu_clock {
    QEMUTimer *qemu_timer;
    uint32_t *trigger;
    int64_t restart;
    double duration;
} clabpu_clock_t;

typedef struct CLabPUTimerState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;

    /*< Timer registers >*/
    uint32_t counter;
    uint32_t control;
    uint32_t status;
    uint32_t prescaler;

    qemu_irq irq;
    clabpu_clock_t tick;
    uint32_t frequency;

} CLabPUTimerState;

#endif /* HW_TIMER_CLABPU_TIMER_H */
