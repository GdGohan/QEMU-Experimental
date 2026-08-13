/*
 * Controlador de interrupcoes da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_INTC_ANDROID_GOLDFISH_PIC_H
#define HW_INTC_ANDROID_GOLDFISH_PIC_H

#include "hw/sysbus.h"

#define TYPE_ANDROID_GOLDFISH_PIC "android-goldfish-pic"
#define ANDROID_GOLDFISH_PIC(obj) \
    OBJECT_CHECK(AndroidGoldfishPICState, (obj), TYPE_ANDROID_GOLDFISH_PIC)

#define ANDROID_GOLDFISH_PIC_IRQ_NB 32

struct AndroidGoldfishPICState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    uint32_t pending;
    uint32_t enabled;
    uint32_t index;
};

typedef struct AndroidGoldfishPICState AndroidGoldfishPICState;

#endif /* HW_INTC_ANDROID_GOLDFISH_PIC_H */
