/*
 * Temporizador da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/timer/android_goldfish_timer.h"

#define TIMER_TIME_LOW         0x00
#define TIMER_TIME_HIGH        0x04
#define TIMER_ALARM_LOW        0x08
#define TIMER_ALARM_HIGH       0x0c
#define TIMER_IRQ_ENABLED      0x10
#define TIMER_CLEAR_ALARM      0x14
#define TIMER_ALARM_STATUS     0x18
#define TIMER_CLEAR_INTERRUPT  0x1c

static uint64_t android_goldfish_timer_get_time(AndroidGoldfishTimerState *s)
{
    return s->tick_offset + (uint64_t)qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
}

static void android_goldfish_timer_update_irq(AndroidGoldfishTimerState *s)
{
    qemu_set_irq(s->irq, s->irq_pending && s->irq_enabled);
}

static void android_goldfish_timer_interrupt(void *opaque)
{
    AndroidGoldfishTimerState *s = opaque;

    s->alarm_running = 0;
    s->irq_pending = 1;
    android_goldfish_timer_update_irq(s);
}

static void android_goldfish_timer_clear_alarm(AndroidGoldfishTimerState *s)
{
    timer_del(s->timer);
    s->alarm_running = 0;
}

static void android_goldfish_timer_set_alarm(AndroidGoldfishTimerState *s)
{
    uint64_t now = android_goldfish_timer_get_time(s);

    if (s->alarm_next <= now) {
        android_goldfish_timer_clear_alarm(s);
        android_goldfish_timer_interrupt(s);
        return;
    }

    timer_mod(s->timer, s->alarm_next - s->tick_offset);
    s->alarm_running = 1;
}

static uint64_t android_goldfish_timer_read(void *opaque, hwaddr addr,
                                            unsigned size)
{
    AndroidGoldfishTimerState *s = opaque;
    uint64_t now;

    switch (addr) {
    case TIMER_TIME_LOW:
        /* TIME_HIGH deve usar a mesma captura que TIME_LOW. */
        now = android_goldfish_timer_get_time(s);
        s->time_high = now >> 32;
        return (uint32_t)now;
    case TIMER_TIME_HIGH:
        return s->time_high;
    case TIMER_ALARM_LOW:
        return (uint32_t)s->alarm_next;
    case TIMER_ALARM_HIGH:
        return s->alarm_next >> 32;
    case TIMER_IRQ_ENABLED:
        return s->irq_enabled;
    case TIMER_ALARM_STATUS:
        return s->alarm_running;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: leitura de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        return 0;
    }
}

static void android_goldfish_timer_write(void *opaque, hwaddr addr,
                                         uint64_t value, unsigned size)
{
    AndroidGoldfishTimerState *s = opaque;
    uint64_t current_time;
    uint64_t new_time;

    switch (addr) {
    case TIMER_TIME_LOW:
        current_time = android_goldfish_timer_get_time(s);
        new_time = (current_time & UINT64_C(0xffffffff00000000)) |
                   (uint32_t)value;
        s->tick_offset += new_time - current_time;
        break;
    case TIMER_TIME_HIGH:
        current_time = android_goldfish_timer_get_time(s);
        new_time = (current_time & UINT64_C(0x00000000ffffffff)) |
                   ((uint64_t)(uint32_t)value << 32);
        s->tick_offset += new_time - current_time;
        break;
    case TIMER_ALARM_LOW:
        s->alarm_next = (s->alarm_next & UINT64_C(0xffffffff00000000)) |
                        (uint32_t)value;
        android_goldfish_timer_set_alarm(s);
        break;
    case TIMER_ALARM_HIGH:
        s->alarm_next = (s->alarm_next & UINT64_C(0x00000000ffffffff)) |
                        ((uint64_t)(uint32_t)value << 32);
        break;
    case TIMER_IRQ_ENABLED:
        s->irq_enabled = value & 1;
        android_goldfish_timer_update_irq(s);
        break;
    case TIMER_CLEAR_ALARM:
        android_goldfish_timer_clear_alarm(s);
        break;
    case TIMER_CLEAR_INTERRUPT:
        s->irq_pending = 0;
        android_goldfish_timer_update_irq(s);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: escrita de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        break;
    }
}

static const MemoryRegionOps android_goldfish_timer_ops = {
    .read = android_goldfish_timer_read,
    .write = android_goldfish_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static int android_goldfish_timer_post_load(void *opaque, int version_id)
{
    AndroidGoldfishTimerState *s = opaque;

    if (s->alarm_running) {
        android_goldfish_timer_set_alarm(s);
    }
    android_goldfish_timer_update_irq(s);
    return 0;
}

static const VMStateDescription vmstate_android_goldfish_timer = {
    .name = TYPE_ANDROID_GOLDFISH_TIMER,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = android_goldfish_timer_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(tick_offset, AndroidGoldfishTimerState),
        VMSTATE_UINT64(alarm_next, AndroidGoldfishTimerState),
        VMSTATE_UINT32(time_high, AndroidGoldfishTimerState),
        VMSTATE_UINT32(alarm_running, AndroidGoldfishTimerState),
        VMSTATE_UINT32(irq_pending, AndroidGoldfishTimerState),
        VMSTATE_UINT32(irq_enabled, AndroidGoldfishTimerState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_timer_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *busdev = SYS_BUS_DEVICE(dev);
    AndroidGoldfishTimerState *s = ANDROID_GOLDFISH_TIMER(dev);

    s->tick_offset = 0;
    s->alarm_next = 0;
    s->time_high = 0;
    s->alarm_running = 0;
    s->irq_pending = 0;
    s->irq_enabled = 0;
    memory_region_init_io(&s->iomem, OBJECT(s), &android_goldfish_timer_ops, s,
                          TYPE_ANDROID_GOLDFISH_TIMER, 0x1000);
    sysbus_init_mmio(busdev, &s->iomem);
    sysbus_init_irq(busdev, &s->irq);
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                            android_goldfish_timer_interrupt, s);
}

static void android_goldfish_timer_class_init(ObjectClass *oc, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = android_goldfish_timer_realize;
    dc->vmsd = &vmstate_android_goldfish_timer;
}

static const TypeInfo android_goldfish_timer_info = {
    .name = TYPE_ANDROID_GOLDFISH_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishTimerState),
    .class_init = android_goldfish_timer_class_init,
};

static void android_goldfish_timer_register_types(void)
{
    type_register_static(&android_goldfish_timer_info);
}

type_init(android_goldfish_timer_register_types)
