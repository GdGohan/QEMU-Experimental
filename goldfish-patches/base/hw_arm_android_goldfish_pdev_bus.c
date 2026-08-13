/*
 * Goldfish platform device bus (Android Emulator)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Protocolo, conforme drivers/platform/goldfish/pdev_bus.c do kernel:
 *
 *   1. o driver escreve OP_INIT em REG_OP;
 *   2. o dispositivo levanta o IRQ enquanto houver device pendente;
 *   3. o handler do driver LE REG_OP num laco:
 *        OP_ADD_DEV  -> avanca para o proximo device pendente e o driver
 *                       le IO_BASE/IO_SIZE/IRQ/IRQ_COUNT/NAME_LEN/ID;
 *        OP_DONE     -> fim da lista, IRQ baixa;
 *   4. para pegar o nome, o driver escreve o endereco fisico do buffer
 *      dele em GET_NAME (e os 32 bits altos em GET_NAME_HIGH antes), e
 *      nos copiamos NAME_LEN bytes para lá.
 *
 * Detalhe importante: OP_DONE e OP_INIT valem os dois 0x00 -- um e valor
 * de leitura, o outro de escrita.
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/arm/android_goldfish_pdev_bus.h"

/* registradores */
enum {
    REG_OP             = 0x00,
    REG_GET_NAME       = 0x04,
    REG_NAME_LEN       = 0x08,
    REG_ID             = 0x0c,
    REG_IO_BASE        = 0x10,
    REG_IO_SIZE        = 0x14,
    REG_IRQ            = 0x18,
    REG_IRQ_COUNT      = 0x1c,
    REG_GET_NAME_HIGH  = 0x20,
};

/* operacoes */
enum {
    OP_DONE       = 0x00,   /* leitura */
    OP_REMOVE_DEV = 0x04,   /* leitura */
    OP_ADD_DEV    = 0x08,   /* leitura */

    OP_INIT       = 0x00,   /* escrita */
};

typedef struct AndroidGoldfishPdevBusDev {
    char name[ANDROID_GOLDFISH_PDEV_BUS_NAME_MAX];
    uint32_t id;
    uint32_t io_base;
    uint32_t io_size;
    uint32_t irq;
    uint32_t irq_count;
} AndroidGoldfishPdevBusDev;

typedef struct AndroidGoldfishPdevBusState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;

    AndroidGoldfishPdevBusDev devs[ANDROID_GOLDFISH_PDEV_BUS_MAX_DEVS];
    uint32_t count;      /* quantos registrados      */
    uint32_t pos;        /* proximo a ser enumerado  */
    int32_t current;     /* device "selecionado", -1 = nenhum */

    /* endereco do buffer de nome fornecido pelo convidado */
    uint32_t name_addr_low;
    uint32_t name_addr_high;
} AndroidGoldfishPdevBusState;

#define ANDROID_GOLDFISH_PDEV_BUS(obj) \
    OBJECT_CHECK(AndroidGoldfishPdevBusState, (obj), \
                 TYPE_ANDROID_GOLDFISH_PDEV_BUS)

void android_goldfish_pdev_bus_add_device(DeviceState *dev,
                                          const char *name,
                                          uint32_t io_base,
                                          uint32_t io_size,
                                          uint32_t irq,
                                          uint32_t irq_count)
{
    AndroidGoldfishPdevBusState *s = ANDROID_GOLDFISH_PDEV_BUS(dev);
    AndroidGoldfishPdevBusDev *d;

    if (s->count >= ANDROID_GOLDFISH_PDEV_BUS_MAX_DEVS) {
        error_report("android-goldfish-pdev-bus: lista cheia, "
                     "dispositivo '%s' ignorado", name);
        return;
    }

    d = &s->devs[s->count];
    /* pstrcpy trunca e garante o NUL, sem depender de strncpy */
    pstrcpy(d->name, sizeof(d->name), name);
    d->id = s->count;
    d->io_base = io_base;
    d->io_size = io_size;
    d->irq = irq;
    d->irq_count = irq_count;
    s->count++;
}

static void android_goldfish_pdev_bus_update_irq(AndroidGoldfishPdevBusState *s)
{
    if (s->pos < s->count) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static uint64_t android_goldfish_pdev_bus_read(void *opaque, hwaddr addr,
                                               unsigned size)
{
    AndroidGoldfishPdevBusState *s = opaque;
    AndroidGoldfishPdevBusDev *d = NULL;

    if (s->current >= 0 && s->current < (int32_t)s->count) {
        d = &s->devs[s->current];
    }

    switch (addr) {
    case REG_OP:
        /*
         * Cada leitura de REG_OP consome um device da lista. O driver
         * chama isso em laco ate receber OP_DONE.
         */
        if (s->pos < s->count) {
            s->current = s->pos;
            s->pos++;
            android_goldfish_pdev_bus_update_irq(s);
            return OP_ADD_DEV;
        }
        s->current = -1;
        android_goldfish_pdev_bus_update_irq(s);
        return OP_DONE;

    case REG_NAME_LEN:
        /*
         * O driver faz kzalloc(name_len + 1), ou seja, ele mesmo cuida do
         * NUL -- devolvemos o strlen puro.
         */
        return d ? strlen(d->name) : 0;

    case REG_ID:
        return d ? d->id : 0;

    case REG_IO_BASE:
        return d ? d->io_base : 0;

    case REG_IO_SIZE:
        return d ? d->io_size : 0;

    case REG_IRQ:
        return d ? d->irq : 0;

    case REG_IRQ_COUNT:
        return d ? d->irq_count : 0;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-pdev-bus: leitura nao suportada "
                      "em 0x%" HWADDR_PRIx "\n", addr);
        return 0;
    }
}

static void android_goldfish_pdev_bus_write(void *opaque, hwaddr addr,
                                            uint64_t value, unsigned size)
{
    AndroidGoldfishPdevBusState *s = opaque;

    switch (addr) {
    case REG_OP:
        if (value == OP_INIT) {
            /* reinicia a enumeracao */
            s->pos = 0;
            s->current = -1;
            android_goldfish_pdev_bus_update_irq(s);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "android-goldfish-pdev-bus: operacao "
                          "desconhecida 0x%" PRIx64 "\n", value);
        }
        break;

    case REG_GET_NAME_HIGH:
        s->name_addr_high = (uint32_t)value;
        break;

    case REG_GET_NAME: {
        /*
         * O driver escreve GET_NAME_HIGH antes de GET_NAME; em ARM 32 bits
         * o valor alto e sempre 0, mas respeitamos os dois de todo jeito.
         */
        AndroidGoldfishPdevBusDev *d;
        hwaddr dest;

        if (s->current < 0 || s->current >= (int32_t)s->count) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "android-goldfish-pdev-bus: GET_NAME sem "
                          "dispositivo selecionado\n");
            break;
        }

        d = &s->devs[s->current];
        s->name_addr_low = (uint32_t)value;
        dest = ((hwaddr)s->name_addr_high << 32) | s->name_addr_low;

        gf_guest_write(dest, (const uint8_t *)d->name, strlen(d->name));
        break;
    }

    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-pdev-bus: escrita nao suportada "
                      "em 0x%" HWADDR_PRIx "\n", addr);
        break;
    }
}

static const MemoryRegionOps android_goldfish_pdev_bus_ops = {
    .read = android_goldfish_pdev_bus_read,
    .write = android_goldfish_pdev_bus_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_android_goldfish_pdev_bus = {
    .name = "android-goldfish-pdev-bus",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(count, AndroidGoldfishPdevBusState),
        VMSTATE_UINT32(pos, AndroidGoldfishPdevBusState),
        VMSTATE_INT32(current, AndroidGoldfishPdevBusState),
        VMSTATE_UINT32(name_addr_low, AndroidGoldfishPdevBusState),
        VMSTATE_UINT32(name_addr_high, AndroidGoldfishPdevBusState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_pdev_bus_realize(DeviceState *dev, Error **errp)
{
    AndroidGoldfishPdevBusState *s = ANDROID_GOLDFISH_PDEV_BUS(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    s->pos = 0;
    s->current = -1;
    s->name_addr_low = 0;
    s->name_addr_high = 0;

    memory_region_init_io(&s->iomem, OBJECT(s),
                          &android_goldfish_pdev_bus_ops, s,
                          "android-goldfish-pdev-bus", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void android_goldfish_pdev_bus_class_init(ObjectClass *oc, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = android_goldfish_pdev_bus_realize;
    dc->vmsd = &vmstate_android_goldfish_pdev_bus;
    dc->desc = "Goldfish platform device bus";
}

static const TypeInfo android_goldfish_pdev_bus_info = {
    .name = TYPE_ANDROID_GOLDFISH_PDEV_BUS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishPdevBusState),
    .class_init = android_goldfish_pdev_bus_class_init,
};

static void android_goldfish_pdev_bus_register_types(void)
{
    type_register_static(&android_goldfish_pdev_bus_info);
}

type_init(android_goldfish_pdev_bus_register_types)
