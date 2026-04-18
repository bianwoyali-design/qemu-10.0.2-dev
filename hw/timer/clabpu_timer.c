#include "qemu/osdep.h"
#include "hw/timer/clabpu_timer.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "qemu/timer.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "migration/vmstate.h"

static void clabpu_timer_update_irq(CLabPUTimerState *s)
{
	bool pending = (s->status & CLABPU_TIMER_STAT_EXPIRED) &&
		       (s->control & CLABPU_TIMER_CTRL_INT_EN);

	qemu_set_irq(s->irq, pending);
}

static void tick_expired(void *opaque)
{
	CLabPUTimerState *s = (CLabPUTimerState *)opaque;

	s->status |= CLABPU_TIMER_STAT_EXPIRED;
	s->counter = 0;
	s->control &= ~CLABPU_TIMER_CTRL_ENABLE;
	clabpu_timer_update_irq(s);
}

static void clock_setup(CLabPUTimerState *s, clabpu_clock_t *clk,
			uint32_t count)
{
	if (count == 0) {
		return;
	}

	/* Calculate period in nanoseconds */
	uint64_t prescaler = s->prescaler ? s->prescaler : 1;
	double period_ns = (1000000000.0 / s->frequency) * prescaler;

	clk->duration = period_ns * count;
	clk->restart = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

	uint64_t expire = clk->restart + (int64_t)clk->duration;
	timer_mod_ns(clk->qemu_timer, expire);
}

static uint64_t read_counter(CLabPUTimerState *s)
{
	if (!(s->control & CLABPU_TIMER_CTRL_ENABLE) ||
	    !timer_pending(s->tick.qemu_timer)) {
		return s->counter;
	}

	int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
	uint64_t prescaler = s->prescaler ? s->prescaler : 1;
	double period_ns = (1000000000.0 / s->frequency) * prescaler;

	uint32_t elapsed_counts = (now - s->tick.restart) / period_ns;

	if (elapsed_counts >= s->counter) {
		return 0;
	}

	return s->counter - elapsed_counts;
}

static void write_counter(CLabPUTimerState *s, uint32_t new_value)
{
	s->counter = new_value;

	if (!(s->control & CLABPU_TIMER_CTRL_ENABLE)) {
		return;
	}

	timer_del(s->tick.qemu_timer);

	if (new_value == 0) {
		tick_expired(s);
	} else {
		clock_setup(s, &s->tick, new_value);
	}
}

static void write_control(CLabPUTimerState *s, uint32_t new_value)
{
	uint32_t old_control = s->control;
	s->control = new_value & 0x3; /* Only bits 0-1 are valid */

	bool was_enabled = old_control & CLABPU_TIMER_CTRL_ENABLE;
	bool now_enabled = s->control & CLABPU_TIMER_CTRL_ENABLE;

	if (!was_enabled && now_enabled) {
		if (s->counter > 0) {
			clock_setup(s, &s->tick, s->counter);
		}
	} else if (was_enabled && !now_enabled) {
		timer_del(s->tick.qemu_timer);
	}
	clabpu_timer_update_irq(s);
}

static uint64_t clabpu_timer_read(void *opaque, hwaddr offset, unsigned size)
{
	CLabPUTimerState *s = CLABPU_TIMER(opaque);
	uint64_t value = 0;

	switch (offset) {
	case CLABPU_TIMER_COUNTER:
		value = read_counter(s);
		break;
	case CLABPU_TIMER_CONTROL:
		value = s->control;
		break;
	case CLABPU_TIMER_STATUS:
		value = s->status;
		break;
	case CLABPU_TIMER_PRESCALER:
		value = s->prescaler;
		break;
	default:
		qemu_log_mask(LOG_GUEST_ERROR,
			      "clabpu_timer: invalid read at offset 0x%02x\n",
			      (unsigned)offset);
		break;
	}

	return value;
}

static void clabpu_timer_write(void *opaque, hwaddr offset, uint64_t value,
			       unsigned size)
{
	CLabPUTimerState *s = CLABPU_TIMER(opaque);

	switch (offset) {
	case CLABPU_TIMER_COUNTER:
		write_counter(s, value);
		break;
	case CLABPU_TIMER_CONTROL:
		write_control(s, value);
		break;
	case CLABPU_TIMER_STATUS:
		value = s->status;
		break;
	case CLABPU_TIMER_PRESCALER:
		value = s->prescaler;
		break;
	default:
		qemu_log_mask(LOG_GUEST_ERROR,
			      "clabpu_timer: invalid write af offset 0x%02x\n",
			      (unsigned)offset);
		break;
	}
}

static const MemoryRegionOps clabpu_timer_ops = {
	.read = clabpu_timer_read,
	.write = clabpu_timer_write,
	.endianness = DEVICE_LITTLE_ENDIAN,
	.valid = {
		.min_access_size = 4,
		.max_access_size = 4,
  },
};

static void clabpu_timer_reset(DeviceState *dev)
{
	CLabPUTimerState *s = CLABPU_TIMER(dev);

	s->control = 0;
	s->counter = 0;
	s->status = 0;
	s->prescaler = 1;

	timer_del(s->tick.qemu_timer);
	qemu_irq_lower(s->irq);
}

static void clabpu_timer_realize(DeviceState *dev, Error **errp)
{
	CLabPUTimerState *s = CLABPU_TIMER(dev);
	SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

	/* Initialize memory region */
	memory_region_init_io(&s->iomem, OBJECT(s), &clabpu_timer_ops, s,
			      TYPE_CLABPU_TIMER, CLABPU_TIMER_SIZE);
	sysbus_init_mmio(sbd, &s->iomem);

	/* Initialize IRQ */
	sysbus_init_irq(sbd, &s->irq);

	/* Create QEMU Timer */
	s->tick.qemu_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, tick_expired, s);
	s->tick.trigger = &s->counter;
}

static void clabpu_timer_unrealize(DeviceState *dev)
{
	CLabPUTimerState *s = CLABPU_TIMER(dev);

	if (s->tick.qemu_timer) {
		timer_free(s->tick.qemu_timer);
		s->tick.trigger = NULL;
	}
}

static Property clabpu_timer_properties[] = { DEFINE_PROP_UINT32(
	"frequency", CLabPUTimerState, frequency, CLABPU_TIMER_FREQ_HZ) };

static const VMStateDescription vmstate_clabpu_timer = {
	.name = "clabpu-timer",
	.version_id = 1,
	.minimum_version_id = 1,
	.fields =
		(const VMStateField[]){
			VMSTATE_UINT32(counter, CLabPUTimerState),
			VMSTATE_UINT32(control, CLabPUTimerState),
			VMSTATE_UINT32(status, CLabPUTimerState),
			VMSTATE_UINT32(prescaler, CLabPUTimerState),
			VMSTATE_UINT32(frequency, CLabPUTimerState),
			VMSTATE_END_OF_LIST() }
};

static void clabpu_timer_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->desc = "CLabPU Timer";
	dc->realize = clabpu_timer_realize;
	dc->unrealize = clabpu_timer_unrealize;
	dc->vmsd = &vmstate_clabpu_timer;
	dc->legacy_reset = clabpu_timer_reset;
	device_class_set_props_n(dc, clabpu_timer_properties,
				 ARRAY_SIZE(clabpu_timer_properties));
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo clabpu_timer_info = {
	.name = TYPE_CLABPU_TIMER,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(CLabPUTimerState),
	.class_init = clabpu_timer_class_init
};

static void clabpu_timer_register_type(void)
{
	type_register_static(&clabpu_timer_info);
}

type_init(clabpu_timer_register_type)