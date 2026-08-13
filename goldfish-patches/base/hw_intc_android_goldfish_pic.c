/*
 * Controlador de interrupcoes da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/intc/android_goldfish_pic.h"

#define PIC_STATUS       0x00
#define PIC_IRQ_PENDING  0x04
#define PIC_DISABLE_ALL  0x08
#define PIC_DISABLE      0x0c
#define PIC_ENABLE       0x10

static void android_goldfish_pic_update(AndroidGoldfishPICState *s)
{
    qemu_set_irq(s->irq, (s->pending & s->enabled) != 0);
}

static void android_goldfish_pic_irq(void *opaque, int irq, int level)
{
    AndroidGoldfishPICState *s = opaque;
    uint32_t mask = UINT32_C(1) << irq;

    if (level) {
        s->pending |= mask;
    } else {
        s->pending &= ~mask;
    }
    android_goldfish_pic_update(s);
}

static uint32_t android_goldfish_pic_pending_count(uint32_t pending)
{
    uint32_t count = 0;

    while (pending) {
        count += pending & 1;
        pending >>= 1;
    }
    return count;
}

static uint64_t android_goldfish_pic_read(void *opaque, hwaddr addr,
                                          unsigned size)
{
    AndroidGoldfishPICState *s = opaque;

    switch (addr) {
    case PIC_STATUS:
        return android_goldfish_pic_pending_count(s->pending & s->enabled);
    case PIC_IRQ_PENDING:
        return s->pending & s->enabled;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: leitura de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        return 0;
    }
}

static void android_goldfish_pic_write(void *opaque, hwaddr addr,
                                       uint64_t value, unsigned size)
{
    AndroidGoldfishPICState *s = opaque;
    uint32_t mask = value;

    switch (addr) {
    case PIC_DISABLE_ALL:
        s->enabled = 0;
        s->pending = 0;
        break;
    case PIC_DISABLE:
        s->enabled &= ~mask;
        break;
    case PIC_ENABLE:
        s->enabled |= mask;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: escrita de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        return;
    }
    android_goldfish_pic_update(s);
}

static const MemoryRegionOps android_goldfish_pic_ops = {
    .read = android_goldfish_pic_read,
    .write = android_goldfish_pic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_android_goldfish_pic = {
    .name = TYPE_ANDROID_GOLDFISH_PIC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(pending, AndroidGoldfishPICState),
        VMSTATE_UINT32(enabled, AndroidGoldfishPICState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_pic_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *busdev = SYS_BUS_DEVICE(dev);
    AndroidGoldfishPICState *s = ANDROID_GOLDFISH_PIC(dev);

    s->pending = 0;
    s->enabled = 0;
    memory_region_init_io(&s->iomem, OBJECT(s), &android_goldfish_pic_ops, s,
                          TYPE_ANDROID_GOLDFISH_PIC, 0x1000);
    sysbus_init_mmio(busdev, &s->iomem);
    sysbus_init_irq(busdev, &s->irq);
    qdev_init_gpio_in(dev, android_goldfish_pic_irq,
                      ANDROID_GOLDFISH_PIC_IRQ_NB);
}

static Property android_goldfish_pic_properties[] = {
    DEFINE_PROP_UINT32("index", AndroidGoldfishPICState, index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void android_goldfish_pic_class_init(ObjectClass *oc, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = android_goldfish_pic_realize;
    dc->vmsd = &vmstate_android_goldfish_pic;
    GF_SET_PROPS(dc, android_goldfish_pic_properties);
}

static const TypeInfo android_goldfish_pic_info = {
    .name = TYPE_ANDROID_GOLDFISH_PIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishPICState),
    .class_init = android_goldfish_pic_class_init,
};

static void android_goldfish_pic_register_types(void)
{
    type_register_static(&android_goldfish_pic_info);
}

type_init(android_goldfish_pic_register_types)
