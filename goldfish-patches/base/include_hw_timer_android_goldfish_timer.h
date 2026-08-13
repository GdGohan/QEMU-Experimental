/*
 * Temporizador da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_TIMER_ANDROID_GOLDFISH_TIMER_H
#define HW_TIMER_ANDROID_GOLDFISH_TIMER_H

#include "hw/sysbus.h"

#define TYPE_ANDROID_GOLDFISH_TIMER "android-goldfish-timer"
#define ANDROID_GOLDFISH_TIMER(obj) \
    OBJECT_CHECK(AndroidGoldfishTimerState, (obj), TYPE_ANDROID_GOLDFISH_TIMER)

struct AndroidGoldfishTimerState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;
    uint64_t tick_offset;
    uint64_t alarm_next;
    uint32_t time_high;
    uint32_t alarm_running;
    uint32_t irq_pending;
    uint32_t irq_enabled;
};

typedef struct AndroidGoldfishTimerState AndroidGoldfishTimerState;

#endif /* HW_TIMER_ANDROID_GOLDFISH_TIMER_H */
