#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/intc/clabpu_intc.h"
#include "migration/vmstate.h"

static void clabpu_intc_update(CLabPUIntcState *s)
{
	uint32_t active;
	int highest_priority = -1;
	int highest_irq = -1;
	int i;

	active = s->enable & s->pending;

	if (!active) {
		qemu_set_irq(s->cpu_irq, 0);
		return;
	}

	/* Find highest priority active interrupt */
	for (i = 0; i < CLABPU_INTC_NUM_SOURCES; ++i) {
		if (active & (1 << i)) {
			if (highest_priority == -1 ||
			    s->priority[i] > s->priority[highest_irq]) {
				highest_priority = s->priority[i];
				highest_irq = i;
			}
		}
	}

	/* Raise CPU interrupt if we have an active interrupt */
	qemu_set_irq(s->cpu_irq, highest_irq >= 0 ? 1 : 0);
}

static void clabpu_intc_set_irq(void *opaque, int irq, int level)
{
	CLabPUIntcState *s = CLABPU_INTC(opaque);

	if (irq >= CLABPU_INTC_NUM_SOURCES) {
		qemu_log_mask(LOG_GUEST_ERROR, "clabpu_intc: invalid irq %d\n",
			      irq);
		return;
	}

	if (level) {
		s->pending |= (1 << irq);
	} else {
		s->pending &= ~(1 << irq);
	}

	clabpu_intc_update(s);
}

static uint64_t clabpu_intc_read(void *opaque, hwaddr offset, unsigned size)
{
	CLabPUIntcState *s = CLABPU_INTC(opaque);
	uint64_t value = 0;

	switch (offset) {
	case INTC_IRQ_PENDING:
		value = s->pending;
		break;
	case INTC_IRQ_ENABLE:
		value = s->enable;
		break;
	case INTC_IRQ_STATUS:
		value = s->enable & s->pending;
		break;
	default:
		if (offset >= INTC_IRQ_PRIORITY_BASE &&
		    offset < INTC_IRQ_PRIORITY_BASE + CLABPU_INTC_NUM_SOURCES) {
			int irq = offset - INTC_IRQ_PRIORITY_BASE;
			value = s->priority[irq];
		} else {
			qemu_log_mask(
				LOG_GUEST_ERROR,
				"clabpu_intc: invalid read at offset 0x%" HWADDR_PRIx
				"\n",
				offset);
		}
		break;
	}

	return value;
}

static void clabpu_intc_write(void *opaque, hwaddr offset, uint64_t value,
			      unsigned size)
{
	CLabPUIntcState *s = CLABPU_INTC(opaque);

	switch (offset) {
	case INTC_IRQ_ENABLE:
		s->enable = value & ((1ULL << CLABPU_INTC_NUM_SOURCES) - 1);
		clabpu_intc_update(s);
		break;
	default:
		if (offset >= INTC_IRQ_PRIORITY_BASE &&
		    offset < INTC_IRQ_PRIORITY_BASE + CLABPU_INTC_NUM_SOURCES) {
			int irq = offset - INTC_IRQ_PRIORITY_BASE;
			s->priority[irq] = value & 0xFF;
			clabpu_intc_update(s);
		} else {
			qemu_log_mask(
				LOG_GUEST_ERROR,
				"clabpu_intc: invalid write at offset 0x%" HWADDR_PRIx
				" value 0x%" PRIx64 "\n",
				offset, value);
		}
	}
}

static const MemoryRegionOps clabpu_intc_ops = {
	.read = clabpu_intc_read,
	.write = clabpu_intc_write,
	.endianness = DEVICE_LITTLE_ENDIAN,
	.valid = {
		.min_access_size = 1,
		.max_access_size = 4,
    },
};

static void clabpu_intc_init(Object *obj)
{
	CLabPUIntcState *s = CLABPU_INTC(obj);
	SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
	DeviceState *dev = DEVICE(obj);

	/* Initialize memory region */
	memory_region_init_io(&s->iomem, obj, &clabpu_intc_ops, s,
			      "clabpu-intc", CLABPU_INTC_SIZE);
	sysbus_init_mmio(sbd, &s->iomem);

	/* Initialize input GPIO lines (from devices) */
	qdev_init_gpio_in(dev, clabpu_intc_set_irq, CLABPU_INTC_NUM_SOURCES);

	/* Initialize output IRQ lines, it will be connected to the CPU */
	sysbus_init_irq(sbd, &s->cpu_irq);
}

static void clabpu_intc_realize(DeviceState *dev, Error **errp)
{
}

static void clabpu_intc_reset(DeviceState *dev)
{
	CLabPUIntcState *s = CLABPU_INTC(dev);
	int i;

	s->pending = 0;
	s->enable = 0;

	for (i = 0; i < CLABPU_INTC_NUM_SOURCES; ++i) {
		s->priority[i] = 0;
	}

	clabpu_intc_update(s);
}

static const VMStateDescription vmstate_clabpu_intc = {
	.name = "vmstate_clabpu_intc",
	.version_id = 1,
	.minimum_version_id = 1,
	.fields =
		(const VMStateField[]){
			VMSTATE_UINT32(pending, CLabPUIntcState),
			VMSTATE_UINT32(enable, CLabPUIntcState),
			VMSTATE_UINT8_ARRAY(priority, CLabPUIntcState,
					    CLABPU_INTC_NUM_SOURCES),
			VMSTATE_END_OF_LIST() }
};

static void clabpu_intc_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->desc = "CLab Processor Unit Interrupt Controller";
	dc->realize = clabpu_intc_realize;
	dc->legacy_reset = clabpu_intc_reset;
	dc->vmsd = &vmstate_clabpu_intc;
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo clabpu_intc_info = {
	.name = TYPE_CLABPU_INTC,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(CLabPUIntcState),
	.instance_init = clabpu_intc_init,
	.class_init = clabpu_intc_class_init,
};

static void clabpu_intc_register_types(void)
{
	type_register_static(&clabpu_intc_info);
}

type_init(clabpu_intc_register_types);