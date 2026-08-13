/*
 * Placa "goldfish" do Android Emulator
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Alvo: kernels goldfish do Android 2.3.6 (Gingerbread, goldfish 2.6.29,
 * ARMv5/arm926) ate 4.4 (KitKat, goldfish 3.4, ARMv7/cortex-a8).
 *
 * Mapa de E/S, retirado de arch/arm/mach-goldfish/include/mach/hardware.h
 * do kernel goldfish (IO_START = 0xff000000, IO_SIZE = 8 MiB):
 *
 *   0xff000000  interrupt controller
 *   0xff001000  pdev_bus                     IRQ 1
 *   0xff002000  tty                          IRQ 4
 *   0xff003000  timer                        IRQ 3
 *   0xff010000+ dispositivos dinamicos       IRQ 5+
 *
 * A RAM comeca em 0x00000000 e o boot e por ATAGS (goldfish nao usa
 * device tree). MACH_TYPE_GOLDFISH = 1441.
 *
 * Fora do interrupt controller, do timer e do tty, o kernel descobre todo
 * o resto do hardware enumerando o pdev_bus -- por isso cada dispositivo
 * dinamico precisa ser registrado com android_goldfish_pdev_bus_add_device()
 * com o nome EXATO que o driver do kernel espera.
 */
#define GF_BOARD 1

#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "cpu.h"
#include "hw/arm/android_goldfish_pdev_bus.h"

#define GOLDFISH_IO_BASE        0xff000000
#define GOLDFISH_PIC_BASE       0xff000000
#define GOLDFISH_PDEV_BUS_BASE  0xff001000
#define GOLDFISH_TTY_BASE       0xff002000
#define GOLDFISH_TIMER_BASE     0xff003000

/* area dos dispositivos enumerados pelo pdev_bus */
#define GOLDFISH_DYN_BASE       0xff010000
#define GOLDFISH_DEV_SIZE       0x00001000

#define GOLDFISH_IRQ_PDEV_BUS   1
#define GOLDFISH_IRQ_TIMER      3
#define GOLDFISH_IRQ_TTY        4
#define GOLDFISH_IRQ_DYN_FIRST  5

#define GOLDFISH_NR_IRQS        32

/* MACH_TYPE_GOLDFISH, de arch/arm/tools/mach-types */
#define GOLDFISH_BOARD_ID       1441

static struct arm_boot_info goldfish_binfo;

/* alocador simples para a area dinamica */
typedef struct GoldfishDynAlloc {
    uint32_t next_base;
    uint32_t next_irq;
} GoldfishDynAlloc;

/*
 * Cria um dispositivo na area dinamica, mapeia, conecta o IRQ e registra
 * no pdev_bus com o nome que o driver do kernel procura.
 */
static DeviceState *goldfish_add_dyn_device(GoldfishDynAlloc *alloc,
                                            DeviceState *pdev_bus,
                                            qemu_irq *pic_in,
                                            const char *qom_type,
                                            const char *kernel_name)
{
    DeviceState *dev;
    uint32_t base = alloc->next_base;
    uint32_t irq = alloc->next_irq;

    dev = sysbus_create_simple(qom_type, base, pic_in[irq]);

    android_goldfish_pdev_bus_add_device(pdev_bus, kernel_name,
                                         base, GOLDFISH_DEV_SIZE, irq, 1);

    alloc->next_base += GOLDFISH_DEV_SIZE;
    alloc->next_irq++;
    return dev;
}

static void goldfish_init(MachineState *machine)
{
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ram;
    ARMCPU *cpu;
    DeviceState *pic;
    DeviceState *pdev_bus;
    qemu_irq pic_in[GOLDFISH_NR_IRQS];
    GoldfishDynAlloc alloc;
    int i;

    /* ---------------- CPU ---------------- */
#if GF_ATLEAST(2, 12)
    cpu = ARM_CPU(object_new(machine->cpu_type));
    GF_REALIZE(cpu);
#else
    {
        const char *cpu_model = machine->cpu_model;

        if (!cpu_model) {
            cpu_model = "arm926";
        }
        cpu = ARM_CPU(cpu_generic_init(TYPE_ARM_CPU, cpu_model));
        if (!cpu) {
            error_report("CPU nao suportada pela placa goldfish: %s",
                         cpu_model);
            exit(1);
        }
    }
#endif

    /* ---------------- RAM em 0x00000000 ---------------- */
    ram = GF_BOARD_RAM(machine);
    memory_region_add_subregion(sysmem, 0, ram);

    /* ---------------- interrupt controller ---------------- */
    /*
     * A saida do PIC vai para a linha IRQ da CPU; as 32 entradas ficam
     * disponiveis como GPIOs de entrada do proprio PIC.
     */
    pic = sysbus_create_simple("android-goldfish-pic", GOLDFISH_PIC_BASE,
                               qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    for (i = 0; i < GOLDFISH_NR_IRQS; i++) {
        pic_in[i] = qdev_get_gpio_in(pic, i);
    }

    /* ---------------- dispositivos de endereco fixo ---------------- */
    pdev_bus = sysbus_create_simple(TYPE_ANDROID_GOLDFISH_PDEV_BUS,
                                    GOLDFISH_PDEV_BUS_BASE,
                                    pic_in[GOLDFISH_IRQ_PDEV_BUS]);

    /*
     * O tty pega o chardev serial padrao sozinho, no realize dele. Isso
     * evita ter que criar o dispositivo "na mao" para setar a propriedade
     * antes do realize -- o que exigiria qdev_create()/qdev_new(), APIs
     * que nao coexistem entre 2.9 e 9.x.
     */
    sysbus_create_simple("android-goldfish-tty", GOLDFISH_TTY_BASE,
                         pic_in[GOLDFISH_IRQ_TTY]);

    sysbus_create_simple("android-goldfish-timer", GOLDFISH_TIMER_BASE,
                         pic_in[GOLDFISH_IRQ_TIMER]);

    /*
     * O tty tambem e enumerado pelo pdev_bus: o kernel usa o endereco fixo
     * so para o early printk, o driver completo vem da enumeracao.
     */
    android_goldfish_pdev_bus_add_device(pdev_bus, "goldfish_tty",
                                         GOLDFISH_TTY_BASE,
                                         GOLDFISH_DEV_SIZE,
                                         GOLDFISH_IRQ_TTY, 1);
    android_goldfish_pdev_bus_add_device(pdev_bus, "goldfish_timer",
                                         GOLDFISH_TIMER_BASE,
                                         GOLDFISH_DEV_SIZE,
                                         GOLDFISH_IRQ_TIMER, 1);

    /* ---------------- dispositivos dinamicos ---------------- */
    alloc.next_base = GOLDFISH_DYN_BASE;
    alloc.next_irq = GOLDFISH_IRQ_DYN_FIRST;

    goldfish_add_dyn_device(&alloc, pdev_bus, pic_in,
                            "android-goldfish-fb", "goldfish_fb");
    goldfish_add_dyn_device(&alloc, pdev_bus, pic_in,
                            "android-goldfish-events", "goldfish_events");
    goldfish_add_dyn_device(&alloc, pdev_bus, pic_in,
                            "android-goldfish-battery", "goldfish_battery");

    /* ---------------- rede (opcional) ---------------- */
#ifdef GF_HAVE_SMC91C111
    if (nd_table[0].used) {
        uint32_t base = alloc.next_base;
        uint32_t irq = alloc.next_irq;

        qemu_check_nic_model(&nd_table[0], "smc91c111");
        smc91c111_init(&nd_table[0], base, pic_in[irq]);
        /* o kernel goldfish procura a placa pelo nome "smc91x" */
        android_goldfish_pdev_bus_add_device(pdev_bus, "smc91x", base,
                                             GOLDFISH_DEV_SIZE, irq, 1);
        alloc.next_base += GOLDFISH_DEV_SIZE;
        alloc.next_irq++;
    }
#endif

    /* ---------------- boot do kernel (ATAGS) ---------------- */
    goldfish_binfo.ram_size = machine->ram_size;
    goldfish_binfo.board_id = GOLDFISH_BOARD_ID;
    goldfish_binfo.loader_start = 0;
    goldfish_binfo.nb_cpus = 1;
#if !GF_ATLEAST(4, 2)
    /*
     * Nas versoes antigas estes campos vinham do arm_boot_info; da 4.2 em
     * diante o arm_load_kernel os pega do MachineState.
     */
    goldfish_binfo.kernel_filename = machine->kernel_filename;
    goldfish_binfo.kernel_cmdline = machine->kernel_cmdline;
    goldfish_binfo.initrd_filename = machine->initrd_filename;
#endif

    GF_LOAD_KERNEL(cpu, machine, &goldfish_binfo);
}

static void goldfish_machine_init(MachineClass *mc)
{
    mc->desc = "Android Emulator goldfish board (ARM)";
    mc->init = goldfish_init;
    mc->max_cpus = 1;

#if GF_ATLEAST(2, 12)
    /*
     * arm926 e o padrao porque cobre o Android 2.3.x (goldfish 2.6.29,
     * ARMv5). Para Android 4.x use -cpu cortex-a8.
     */
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm926");
    /*
     * O kernel goldfish sonda enderecos que nao temos implementados; sem
     * isso o QEMU aborta em acesso nao mapeado.
     */
    mc->ignore_memory_transaction_failures = true;
#endif

    /*
     * Obrigatorio a partir da 5.0: sem default_ram_id o codigo generico nao
     * cria machine->ram e a placa recebe NULL. Antes da 5.0 o campo nem
     * existe -- a RAM e alocada na mao pelo GF_BOARD_RAM.
     */
#if GF_ATLEAST(5, 0)
    mc->default_ram_id = "goldfish.ram";
#endif

    /* nao usar MiB: qemu/units.h so aparece na 2.12 */
    mc->default_ram_size = 256 * 1024 * 1024;
}

DEFINE_MACHINE("goldfish", goldfish_machine_init)
