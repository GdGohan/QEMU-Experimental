/*
 * Terminal serial da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_CHAR_ANDROID_GOLDFISH_TTY_H
#define HW_CHAR_ANDROID_GOLDFISH_TTY_H

#include "hw/goldfish/compat.h"

#define TYPE_ANDROID_GOLDFISH_TTY "android-goldfish-tty"
#define ANDROID_GOLDFISH_TTY(obj) \
    OBJECT_CHECK(AndroidGoldfishTtyState, (obj), TYPE_ANDROID_GOLDFISH_TTY)

#define ANDROID_GOLDFISH_TTY_BUFFER_SIZE 128

struct AndroidGoldfishTtyState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    GF_CHR_FIELD;
    uint32_t data_len;
    uint64_t data_ptr;
    bool int_enabled;
    uint8_t rx_buffer[ANDROID_GOLDFISH_TTY_BUFFER_SIZE];
    uint32_t rx_read;
    uint32_t rx_write;
    uint32_t rx_count;
};

typedef struct AndroidGoldfishTtyState AndroidGoldfishTtyState;

#endif /* HW_CHAR_ANDROID_GOLDFISH_TTY_H */
