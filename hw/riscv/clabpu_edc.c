#include "qemu/osdep.h"
#include "qemu/typedefs.h"
#include "hw/riscv/clabpu_edc.h"
#include "exec/address-spaces.h"
#include "migration/vmstate.h"
#include "hw/irq.h"
#include "qemu/log.h"

static void clabpu_edc_update_irq(CLabPUEdcState *s)
{
	bool irq_level = (s->int_status & s->int_enable) != 0;
	qemu_set_irq(s->irq, irq_level);
}

static uint64_t clabpu_edc_reg1_read(void *opaque, hwaddr offset, unsigned size)
{
	CLabPUEdcState *s = CLABPU_EDC(opaque);
	uint64_t value = 0;

	switch (offset) {
	case EDC_CTRL_REG:
		value = s->ctrl;
		break;
	case EDC_STATUS_REG:
		value = s->status;
		break;
	case EDC_ERROR_REG:
		value = s->error;
		break;
	case EDC_INT_ENABLE_REG:
		value = s->int_enable;
		break;
	case EDC_INT_STATUS_REG:
		value = s->int_status;
		break;
	default:
		qemu_log_mask(
			LOG_GUEST_ERROR,
			"clabpu_edc: invalid read at offset 0x%" HWADDR_PRIx
			"\n",
			offset);
		break;
	}

	return value;
}

static void clabpu_edc_reg1_write(void *opaque, hwaddr offset, uint64_t value,
				  unsigned size)
{
	CLabPUEdcState *s = CLABPU_EDC(opaque);

	switch (offset) {
	case EDC_CTRL_REG:
		s->ctrl = value & 0xFFFFFFFF;
		/* Trigger some action based on control register */
		if (value & 0x1) {
			/* Example: Start EDC operation */
			s->status |= 0x1; /* Set busy bit */
		}
		break;
	case EDC_INT_ENABLE_REG:
		s->int_enable = value & 0xFFFFFFFF;
		clabpu_edc_update_irq(s);
		break;
	case EDC_INT_STATUS_REG:
		/* Writing 1 clears the interrupt */
		s->int_status &= ~(value & 0xFFFFFFFF);
		clabpu_edc_update_irq(s);
		break;
	default:
		qemu_log_mask(
			LOG_GUEST_ERROR,
			"clabpu_edc: invalid write at offset 0x%" HWADDR_PRIx
			" value 0x%" PRIx64 "\n",
			offset, value);
		break;
	}
}

static const MemoryRegionOps clabpu_edc_reg1_ops = {
    .read = clabpu_edc_reg1_read,
    .write = clabpu_edc_reg1_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t clabpu_edc_err_read(void *opaque, hwaddr offset, unsigned size)
{
	CLabPUEdcState *s = CLABPU_EDC(opaque);

	/* Error reporting registers */
	if (offset == 0) {
		return s->error;
	}

	return 0;
}

static void clabpu_edc_err_write(void *opaque, hwaddr offset, uint64_t value,
				 unsigned size)
{
	CLabPUEdcState *s = CLABPU_EDC(opaque);

	/* Clear error flags */
	if (offset == 0) {
		s->error &= ~(value & 0xFFFFFFFF);
	}
}

static const MemoryRegionOps
	clabpu_edc_err_ops = { .read = clabpu_edc_err_read,
			       .write = clabpu_edc_err_write,
			       .endianness = DEVICE_LITTLE_ENDIAN,
			       .valid = {
				       .min_access_size = 4,
				       .max_access_size = 4,
    },
};

static void clabpu_edc_reset(DeviceState *dev)
{
	CLabPUEdcState *s = CLABPU_EDC(dev);

	s->ctrl = 0;
	s->error = 0;
	s->status = 0;
	s->int_status = 0;
	s->int_enable = 0;
}

static void clabpu_edc_init(Object *obj)
{
	CLabPUEdcState *s = CLABPU_EDC(obj);
	SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
	MemoryRegion *system_memory = get_system_memory();

	/* Initialize memory regions */
	memory_region_init_io(&s->reg1, obj, &clabpu_edc_reg1_ops, s,
			      "clabpu-edc-reg1", CLABPU_EDC_REG1_SIZE);
	memory_region_init_io(&s->err, obj, &clabpu_edc_err_ops, s,
			      "clabpu-edc-err", CLABPU_EDC_ERR_SIZE);

	/* Register as sysbus MMIO region */
	sysbus_init_mmio(sbd, &s->reg1);

	/* The error region will be mapped separately to system memory */
	memory_region_add_subregion(system_memory, CLABPU_EDC_ERR_BASE,
				    &s->err);

	/* Initialize IRQ */
	sysbus_init_irq(sbd, &s->irq);
}

static void clabpu_edc_realize(DeviceState *dev, Error **errp)
{
}

// For migration support
static const VMStateDescription vmstate_clabpu_edc = {
	.name = "clabpu-edc",
	.version_id = 1,
	.minimum_version_id = 1,
	.fields =
		(const VMStateField[]){
			VMSTATE_UINT32(ctrl, CLabPUEdcState),
			VMSTATE_UINT32(status, CLabPUEdcState),
			VMSTATE_UINT32(error, CLabPUEdcState),
			VMSTATE_UINT32(int_enable, CLabPUEdcState),
			VMSTATE_UINT32(int_status, CLabPUEdcState),
			VMSTATE_END_OF_LIST() }
};

static void clabpu_edc_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->desc = "CLab Processor Unit EDC Device";
	dc->realize = clabpu_edc_realize;
	dc->legacy_reset = clabpu_edc_reset;
	dc->vmsd = &vmstate_clabpu_edc;
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo clabpu_edc_info = {
	.name = TYPE_CLABPU_EDC,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(CLabPUEdcState),
	.instance_init = clabpu_edc_init,
	.class_init = clabpu_edc_class_init,
};

static void clabpu_edc_register_types(void)
{
	type_register_static(&clabpu_edc_info);
}

type_init(clabpu_edc_register_types)