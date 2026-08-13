/*
 * Bateria goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/misc/android_goldfish_battery.h"

#define ANDROID_GOLDFISH_BATTERY_MMIO_SIZE 0x1000

enum {
    ANDROID_GOLDFISH_BATTERY_INT_STATUS = 0x00,
    ANDROID_GOLDFISH_BATTERY_INT_ENABLE = 0x04,
    ANDROID_GOLDFISH_BATTERY_AC_ONLINE = 0x08,
    ANDROID_GOLDFISH_BATTERY_STATUS = 0x0c,
    ANDROID_GOLDFISH_BATTERY_HEALTH = 0x10,
    ANDROID_GOLDFISH_BATTERY_PRESENT = 0x14,
    ANDROID_GOLDFISH_BATTERY_CAPACITY = 0x18,
};

#define ANDROID_GOLDFISH_BATTERY_STATUS_CHANGED (1U << 0)
#define ANDROID_GOLDFISH_BATTERY_AC_STATUS_CHANGED (1U << 1)

struct AndroidGoldfishBatteryState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    uint32_t int_status;
    uint32_t int_enable;
    uint32_t ac_online;
    uint32_t status;
    uint32_t health;
    uint32_t present;
    uint32_t capacity;
};

static void android_goldfish_battery_update_irq(
    AndroidGoldfishBatteryState *s)
{
    qemu_set_irq(s->irq, !!(s->int_status & s->int_enable));
}

static uint64_t android_goldfish_battery_read(void *opaque, hwaddr addr,
                                              unsigned size)
{
    AndroidGoldfishBatteryState *s = opaque;
    uint32_t value = 0;

    (void)size;
    switch (addr) {
    case ANDROID_GOLDFISH_BATTERY_INT_STATUS:
        value = s->int_status;
        s->int_status = 0;
        qemu_set_irq(s->irq, 0);
        break;
    case ANDROID_GOLDFISH_BATTERY_INT_ENABLE:
        value = s->int_enable;
        break;
    case ANDROID_GOLDFISH_BATTERY_AC_ONLINE:
        value = s->ac_online;
        break;
    case ANDROID_GOLDFISH_BATTERY_STATUS:
        value = s->status;
        break;
    case ANDROID_GOLDFISH_BATTERY_HEALTH:
        value = s->health;
        break;
    case ANDROID_GOLDFISH_BATTERY_PRESENT:
        value = s->present;
        break;
    case ANDROID_GOLDFISH_BATTERY_CAPACITY:
        value = s->capacity;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-battery: leitura desconhecida em 0x%"
                      HWADDR_PRIx "\n", addr);
        break;
    }
    return value;
}

static void android_goldfish_battery_write(void *opaque, hwaddr addr,
                                           uint64_t value, unsigned size)
{
    AndroidGoldfishBatteryState *s = opaque;

    (void)size;
    switch (addr) {
    case ANDROID_GOLDFISH_BATTERY_INT_ENABLE:
        s->int_enable = value;
        android_goldfish_battery_update_irq(s);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-battery: escrita desconhecida em 0x%"
                      HWADDR_PRIx "\n", addr);
        break;
    }
}

static const MemoryRegionOps android_goldfish_battery_ops_mmio = {
    .read = android_goldfish_battery_read,
    .write = android_goldfish_battery_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_android_goldfish_battery = {
    .name = TYPE_ANDROID_GOLDFISH_BATTERY,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(int_status, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(int_enable, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(ac_online, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(status, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(health, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(present, AndroidGoldfishBatteryState),
        VMSTATE_UINT32(capacity, AndroidGoldfishBatteryState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_battery_realize(DeviceState *dev, Error **errp)
{
    AndroidGoldfishBatteryState *s = ANDROID_GOLDFISH_BATTERY(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    (void)errp;
    s->int_status = 0;
    s->int_enable = 0;
    s->ac_online = 1;
    s->status = 1;
    s->health = 1;
    s->present = 1;
    s->capacity = 50;
    memory_region_init_io(&s->iomem, OBJECT(s),
                          &android_goldfish_battery_ops_mmio, s,
                          TYPE_ANDROID_GOLDFISH_BATTERY,
                          ANDROID_GOLDFISH_BATTERY_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void android_goldfish_battery_class_init(ObjectClass *klass, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    (void)data;
    dc->realize = android_goldfish_battery_realize;
    dc->vmsd = &vmstate_android_goldfish_battery;
}

static const TypeInfo android_goldfish_battery_type_info = {
    .name = TYPE_ANDROID_GOLDFISH_BATTERY,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishBatteryState),
    .class_init = android_goldfish_battery_class_init,
};

static void android_goldfish_battery_register_types(void)
{
    type_register_static(&android_goldfish_battery_type_info);
}

type_init(android_goldfish_battery_register_types)
