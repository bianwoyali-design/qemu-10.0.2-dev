#include "qemu/osdep.h"
#include "hw/riscv/numa.h"
#include "qemu/error-report.h"
#include "qom/object.h"
#include "hw/boards.h"
#include "hw/riscv/clabpu.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"

static const MemMapEntry clabpu_memmap[] = {
	[CLABPU_MROM] = { 0x1000, 0xf000 },
	[CLABPU_HTIF] = { 0x1000000, 0x1000 },
	[CLABPU_CLINT] = { 0x2000000, 0x10000 },
	[CLABPU_DRAM] = { 0x80000000, 0x0 },
};

static void clabpu_init_cpu(CLabPUState *clabpu, MachineState *machine)
{
	int base_hartid, hart_count;
	char *soc_name;
	if (riscv_socket_count(machine) != 1) {
		error_report("number of sockets/nodes should be 1 for CLabPU");
		exit(1);
	}

	if (!riscv_socket_check_hartids(machine, 0)) {
		error_report("discontinuous hartids in socket0");
		exit(1);
	}

	base_hartid = riscv_socket_first_hartid(machine, 0);
	if (base_hartid < 0) {
		error_report("can't find hartid base for socket%d", 0);
		exit(1);
	}

	hart_count = riscv_socket_hart_count(machine, 0);
	if (hart_count < 0) {
		error_report("can't find hart count for socket%d", 0);
		exit(1);
	}

	soc_name = g_strdup_printf("clabpu-socket0");
	object_initialize_child(OBJECT(machine), soc_name, &clabpu->soc,
				TYPE_RISCV_HART_ARRAY);
	g_free(soc_name);
	object_property_set_str(OBJECT(&clabpu->soc), "cpu-type",
				machine->cpu_type, &error_abort);
	object_property_set_int(OBJECT(&clabpu->soc), "hartid-base",
				base_hartid, &error_abort);
	object_property_set_int(OBJECT(&clabpu->soc), "num-harts", hart_count,
				&error_abort);
	sysbus_realize(SYS_BUS_DEVICE(&clabpu->soc), &error_fatal);
}

static void clabpu_init_mem(CLabPUState *clabpu, MachineState *machine)
{
	const MemMapEntry *memmap = clabpu_memmap;
	MemoryRegion *system_memory = get_system_memory();
	MemoryRegion *mask_rom = g_new(MemoryRegion, 1);

	/*register system main memory*/
	memory_region_add_subregion(system_memory, memmap[CLABPU_DRAM].base,
				    machine->ram);
	/*boot rom*/
	memory_region_init_rom(mask_rom, NULL, "riscv.clabpu.mrom",
			       memmap[CLABPU_MROM].size, &error_fatal);
	memory_region_add_subregion(system_memory, memmap[CLABPU_MROM].base,
				    mask_rom);
}

static void clabpu_init_dev(CLabPUState *clabpu, MachineState *machine)
{
	/*
	 * Step 2 for board bring-up:
	 * Create and map devices (UART/CLINT/PLIC/...).
	 */
	(void)clabpu;
	(void)machine;
}

static void clabpu_init_boot(CLabPUState *clabpu)
{
	/*
	 * Step 3 for board bring-up:
	 * Load firmware/kernel and set reset vector.
	 */
	(void)clabpu;
}

static void clabpu_machine_instance_init(Object *obj)
{
	/* Optional per-instance default properties. */
	(void)obj;
}

static void clabpu_init(MachineState *machine)
{
	CLabPUState *clabpu = CLABPU_MACHINE(machine);

	clabpu_init_cpu(clabpu, machine);
	clabpu_init_mem(clabpu, machine);
	clabpu_init_dev(clabpu, machine);
	clabpu_init_boot(clabpu);
}

void clabpu_machine_init(ObjectClass *oc, void *data)
{
	MachineClass *mc = MACHINE_CLASS(oc);

	mc->desc = "CLab Processor Units";
	mc->init = clabpu_init;
	mc->default_cpu_type = TYPE_RISCV_CPU_THEAD_C906;
	mc->default_ram_size = 0x80000000;
	mc->default_ram_id = "clabpu.ram";
	mc->max_cpus = 1;
	mc->default_cpus = 1;
	/*RISC-V specific properties*/
	mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;
	mc->cpu_index_to_instance_props = riscv_numa_cpu_index_to_props;
	mc->get_default_cpu_node_id = riscv_numa_get_default_cpu_node_id;
	mc->numa_mem_supported = true;
}

static const TypeInfo clabpu_machine_typeinfo = {
	.name = MACHINE_TYPE_NAME("clabpu"),
	.parent = TYPE_MACHINE,
	.class_init = clabpu_machine_init,
	.instance_init = clabpu_machine_instance_init,
	.instance_size = sizeof(CLabPUState),
};

static void clabpu_machine_init_register_types(void)
{
	type_register_static(&clabpu_machine_typeinfo);
}

type_init(clabpu_machine_init_register_types)