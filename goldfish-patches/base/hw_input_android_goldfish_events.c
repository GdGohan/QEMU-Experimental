/*
 * Dispositivo de eventos goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/input/android_goldfish_events.h"
#include "ui/console.h"
#include "ui/input.h"

#define ANDROID_GOLDFISH_EVENTS_MMIO_SIZE 0x1000
#define ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS 768

enum {
    ANDROID_GOLDFISH_EVENTS_READ = 0x00,
    ANDROID_GOLDFISH_EVENTS_SET_PAGE = 0x00,
    ANDROID_GOLDFISH_EVENTS_LEN = 0x04,
    ANDROID_GOLDFISH_EVENTS_DATA = 0x08,
};

#define ANDROID_GOLDFISH_EVENTS_PAGE_NAME    0x00000
#define ANDROID_GOLDFISH_EVENTS_PAGE_EVBITS  0x10000
#define ANDROID_GOLDFISH_EVENTS_PAGE_ABSDATA (0x20000 | 3)

#define ANDROID_GOLDFISH_EV_SYN 0
#define ANDROID_GOLDFISH_EV_KEY 1
#define ANDROID_GOLDFISH_EV_ABS 3
#define ANDROID_GOLDFISH_EV_SW 5

#define ANDROID_GOLDFISH_ABS_X 0
#define ANDROID_GOLDFISH_ABS_Y 1

#define ANDROID_GOLDFISH_KEY_BACK 158
#define ANDROID_GOLDFISH_KEY_MENU 139
#define ANDROID_GOLDFISH_KEY_HOME 102
#define ANDROID_GOLDFISH_KEY_END 107
#define ANDROID_GOLDFISH_KEY_POWER 116
#define ANDROID_GOLDFISH_KEY_VOLUMEUP 115
#define ANDROID_GOLDFISH_KEY_VOLUMEDOWN 114
#define ANDROID_GOLDFISH_KEY_SEND 231
#define ANDROID_GOLDFISH_BTN_TOUCH 0x14a

struct AndroidGoldfishEventsState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    uint32_t width;
    uint32_t height;
    uint32_t page;
    uint32_t queue[ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS];
    uint32_t qhead;
    uint32_t qtail;
    uint32_t qcount;
    QemuInputHandlerState *input_handler;
};

typedef struct AndroidGoldfishEventsKey {
    int qcode;
    uint16_t keycode;
} AndroidGoldfishEventsKey;

static const AndroidGoldfishEventsKey android_goldfish_events_keymap[] = {
    { Q_KEY_CODE_A, 30 }, { Q_KEY_CODE_B, 48 },
    { Q_KEY_CODE_C, 46 }, { Q_KEY_CODE_D, 32 },
    { Q_KEY_CODE_E, 18 }, { Q_KEY_CODE_F, 33 },
    { Q_KEY_CODE_G, 34 }, { Q_KEY_CODE_H, 35 },
    { Q_KEY_CODE_I, 23 }, { Q_KEY_CODE_J, 36 },
    { Q_KEY_CODE_K, 37 }, { Q_KEY_CODE_L, 38 },
    { Q_KEY_CODE_M, 50 }, { Q_KEY_CODE_N, 49 },
    { Q_KEY_CODE_O, 24 }, { Q_KEY_CODE_P, 25 },
    { Q_KEY_CODE_Q, 16 }, { Q_KEY_CODE_R, 19 },
    { Q_KEY_CODE_S, 31 }, { Q_KEY_CODE_T, 20 },
    { Q_KEY_CODE_U, 22 }, { Q_KEY_CODE_V, 47 },
    { Q_KEY_CODE_W, 17 }, { Q_KEY_CODE_X, 45 },
    { Q_KEY_CODE_Y, 21 }, { Q_KEY_CODE_Z, 44 },
    { Q_KEY_CODE_1, 2 }, { Q_KEY_CODE_2, 3 },
    { Q_KEY_CODE_3, 4 }, { Q_KEY_CODE_4, 5 },
    { Q_KEY_CODE_5, 6 }, { Q_KEY_CODE_6, 7 },
    { Q_KEY_CODE_7, 8 }, { Q_KEY_CODE_8, 9 },
    { Q_KEY_CODE_9, 10 }, { Q_KEY_CODE_0, 11 },
    { Q_KEY_CODE_RET, 28 }, { Q_KEY_CODE_BACKSPACE, 14 },
    { Q_KEY_CODE_SPC, 57 }, { Q_KEY_CODE_TAB, 15 },
    { Q_KEY_CODE_UP, 103 }, { Q_KEY_CODE_DOWN, 108 },
    { Q_KEY_CODE_LEFT, 105 }, { Q_KEY_CODE_RIGHT, 106 },
    { Q_KEY_CODE_SHIFT, 42 }, { Q_KEY_CODE_SHIFT_R, 54 },
    { Q_KEY_CODE_CTRL, 29 }, { Q_KEY_CODE_CTRL_R, 97 },
    { Q_KEY_CODE_ALT, 56 }, { Q_KEY_CODE_ALT_R, 100 },
    { Q_KEY_CODE_ESC, ANDROID_GOLDFISH_KEY_BACK },
    { Q_KEY_CODE_F1, ANDROID_GOLDFISH_KEY_HOME },
    { Q_KEY_CODE_F2, ANDROID_GOLDFISH_KEY_MENU },
    { Q_KEY_CODE_F3, ANDROID_GOLDFISH_KEY_POWER },
    { Q_KEY_CODE_F5, ANDROID_GOLDFISH_KEY_VOLUMEUP },
    { Q_KEY_CODE_F6, ANDROID_GOLDFISH_KEY_VOLUMEDOWN },
    { Q_KEY_CODE_END, ANDROID_GOLDFISH_KEY_END },
};

static void android_goldfish_events_update_irq(AndroidGoldfishEventsState *s)
{
    qemu_set_irq(s->irq, s->qcount != 0);
}

static void android_goldfish_events_push(AndroidGoldfishEventsState *s,
                                         uint32_t type, uint32_t code,
                                         uint32_t value)
{
    if (s->qcount + 3 > ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "android-goldfish-events: fila cheia\n");
        return;
    }

    s->queue[s->qtail] = type;
    s->qtail = (s->qtail + 1) % ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS;
    s->queue[s->qtail] = code;
    s->qtail = (s->qtail + 1) % ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS;
    s->queue[s->qtail] = value;
    s->qtail = (s->qtail + 1) % ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS;
    s->qcount += 3;
    android_goldfish_events_update_irq(s);
}

static uint32_t android_goldfish_events_pop(AndroidGoldfishEventsState *s)
{
    uint32_t value;

    if (!s->qcount) {
        return 0;
    }

    value = s->queue[s->qhead];
    s->qhead = (s->qhead + 1) % ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS;
    s->qcount--;
    android_goldfish_events_update_irq(s);
    return value;
}

static bool android_goldfish_events_key_supported(unsigned int code)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(android_goldfish_events_keymap); i++) {
        if (android_goldfish_events_keymap[i].keycode == code) {
            return true;
        }
    }

    switch (code) {
    case ANDROID_GOLDFISH_KEY_SEND:
    case ANDROID_GOLDFISH_BTN_TOUCH:
        return true;
    default:
        return false;
    }
}

static uint32_t android_goldfish_events_data_len(AndroidGoldfishEventsState *s)
{
    uint32_t type = s->page & 0xffff;

    if (s->page == ANDROID_GOLDFISH_EVENTS_PAGE_NAME) {
        return sizeof("qwerty2") - 1;
    }
    if ((s->page & 0xffff0000) == ANDROID_GOLDFISH_EVENTS_PAGE_EVBITS) {
        switch (type) {
        case ANDROID_GOLDFISH_EV_SYN:
        case ANDROID_GOLDFISH_EV_ABS:
        case ANDROID_GOLDFISH_EV_SW:
            return 1;
        case ANDROID_GOLDFISH_EV_KEY:
            return (ANDROID_GOLDFISH_BTN_TOUCH / 8) + 1;
        default:
            return 0;
        }
    }
    if (s->page == ANDROID_GOLDFISH_EVENTS_PAGE_ABSDATA) {
        return 32;
    }
    return 0;
}

static uint8_t android_goldfish_events_data_byte(AndroidGoldfishEventsState *s,
                                                  uint32_t offset)
{
    static const uint8_t name[] = "qwerty2";
    uint32_t type = s->page & 0xffff;

    if (offset >= android_goldfish_events_data_len(s)) {
        return 0;
    }
    if (s->page == ANDROID_GOLDFISH_EVENTS_PAGE_NAME) {
        return name[offset];
    }
    if ((s->page & 0xffff0000) == ANDROID_GOLDFISH_EVENTS_PAGE_EVBITS) {
        uint32_t base = offset * 8;
        uint8_t bits = 0;
        unsigned int bit;

        for (bit = 0; bit < 8; bit++) {
            unsigned int code = base + bit;
            bool supported = false;

            switch (type) {
            case ANDROID_GOLDFISH_EV_SYN:
                supported = (code == 0);
                break;
            case ANDROID_GOLDFISH_EV_KEY:
                supported = android_goldfish_events_key_supported(code);
                break;
            case ANDROID_GOLDFISH_EV_ABS:
                supported = (code == ANDROID_GOLDFISH_ABS_X ||
                             code == ANDROID_GOLDFISH_ABS_Y);
                break;
            case ANDROID_GOLDFISH_EV_SW:
                supported = (code == 0);
                break;
            }
            if (supported) {
                bits |= 1U << bit;
            }
        }
        return bits;
    }
    if (s->page == ANDROID_GOLDFISH_EVENTS_PAGE_ABSDATA) {
        uint32_t word = offset / 4;
        uint32_t value;

        switch (word) {
        case 0:
        case 4:
        case 8:
        case 12:
            value = 0;
            break;
        case 1:
            value = s->width ? s->width - 1 : 0;
            break;
        case 5:
            value = s->height ? s->height - 1 : 0;
            break;
        default:
            value = 0;
            break;
        }
        return (value >> ((offset & 3) * 8)) & 0xff;
    }
    return 0;
}

static uint64_t android_goldfish_events_read(void *opaque, hwaddr addr,
                                             unsigned size)
{
    AndroidGoldfishEventsState *s = opaque;
    uint64_t value = 0;
    unsigned int i;

    if (addr == ANDROID_GOLDFISH_EVENTS_READ) {
        return android_goldfish_events_pop(s);
    }
    if (addr == ANDROID_GOLDFISH_EVENTS_LEN) {
        return android_goldfish_events_data_len(s);
    }
    if (addr >= ANDROID_GOLDFISH_EVENTS_DATA &&
        addr + size <= ANDROID_GOLDFISH_EVENTS_MMIO_SIZE) {
        for (i = 0; i < size; i++) {
            value |= (uint64_t)android_goldfish_events_data_byte(
                s, addr - ANDROID_GOLDFISH_EVENTS_DATA + i) << (8 * i);
        }
        return value;
    }

    qemu_log_mask(LOG_UNIMP,
                  "android-goldfish-events: leitura desconhecida em 0x%"
                  HWADDR_PRIx "\n", addr);
    return 0;
}

static void android_goldfish_events_write(void *opaque, hwaddr addr,
                                          uint64_t value, unsigned size)
{
    AndroidGoldfishEventsState *s = opaque;

    (void)size;
    if (addr == ANDROID_GOLDFISH_EVENTS_SET_PAGE) {
        s->page = value;
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "android-goldfish-events: escrita desconhecida em 0x%"
                  HWADDR_PRIx "\n", addr);
}

static const MemoryRegionOps android_goldfish_events_ops_mmio = {
    .read = android_goldfish_events_read,
    .write = android_goldfish_events_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static uint32_t android_goldfish_events_scale(uint32_t value, uint32_t size)
{
    if (!size) {
        return 0;
    }
    if (value > 0x7fff) {
        value = 0x7fff;
    }
    return ((uint64_t)value * (size - 1)) / 0x7fff;
}

/*
 * QemuInputHandler recebe DeviceState* (assim desde a 2.9 e ainda na 9.x),
 * nao void*.
 */
static void android_goldfish_events_input(DeviceState *dev, QemuConsole *src,
                                          InputEvent *evt)
{
    AndroidGoldfishEventsState *s = ANDROID_GOLDFISH_EVENTS(dev);
    size_t i;

    (void)src;
    switch (evt->type) {
    case INPUT_EVENT_KIND_KEY:
        {
            int qcode = qemu_input_key_value_to_qcode(evt->u.key.data->key);

            for (i = 0; i < ARRAY_SIZE(android_goldfish_events_keymap); i++) {
                if (android_goldfish_events_keymap[i].qcode == qcode) {
                    android_goldfish_events_push(
                        s, ANDROID_GOLDFISH_EV_KEY,
                        android_goldfish_events_keymap[i].keycode,
                        evt->u.key.data->down);
                    return;
                }
            }
        }
        break;
    case INPUT_EVENT_KIND_BTN:
        if (evt->u.btn.data->button == INPUT_BUTTON_LEFT) {
            android_goldfish_events_push(s, ANDROID_GOLDFISH_EV_KEY,
                                         ANDROID_GOLDFISH_BTN_TOUCH,
                                         evt->u.btn.data->down);
        }
        break;
    case INPUT_EVENT_KIND_ABS:
        if (evt->u.abs.data->axis == INPUT_AXIS_X) {
            android_goldfish_events_push(
                s, ANDROID_GOLDFISH_EV_ABS, ANDROID_GOLDFISH_ABS_X,
                android_goldfish_events_scale(evt->u.abs.data->value,
                                               s->width));
        } else if (evt->u.abs.data->axis == INPUT_AXIS_Y) {
            android_goldfish_events_push(
                s, ANDROID_GOLDFISH_EV_ABS, ANDROID_GOLDFISH_ABS_Y,
                android_goldfish_events_scale(evt->u.abs.data->value,
                                               s->height));
        }
        break;
    default:
        break;
    }
}

static void android_goldfish_events_sync(DeviceState *dev)
{
    AndroidGoldfishEventsState *s = ANDROID_GOLDFISH_EVENTS(dev);

    android_goldfish_events_push(s, ANDROID_GOLDFISH_EV_SYN, 0, 0);
}

static QemuInputHandler android_goldfish_events_handler = {
    .name = "android-goldfish-events",
    .mask = INPUT_EVENT_MASK_KEY | INPUT_EVENT_MASK_BTN | INPUT_EVENT_MASK_ABS,
    .event = android_goldfish_events_input,
    .sync = android_goldfish_events_sync,
};

static const VMStateDescription vmstate_android_goldfish_events = {
    .name = TYPE_ANDROID_GOLDFISH_EVENTS,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(page, AndroidGoldfishEventsState),
        VMSTATE_UINT32_ARRAY(queue, AndroidGoldfishEventsState,
                             ANDROID_GOLDFISH_EVENTS_QUEUE_WORDS),
        VMSTATE_UINT32(qhead, AndroidGoldfishEventsState),
        VMSTATE_UINT32(qtail, AndroidGoldfishEventsState),
        VMSTATE_UINT32(qcount, AndroidGoldfishEventsState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_events_realize(DeviceState *dev, Error **errp)
{
    AndroidGoldfishEventsState *s = ANDROID_GOLDFISH_EVENTS(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    (void)errp;
    s->page = ANDROID_GOLDFISH_EVENTS_PAGE_NAME;
    s->qhead = 0;
    s->qtail = 0;
    s->qcount = 0;
    memory_region_init_io(&s->iomem, OBJECT(s),
                          &android_goldfish_events_ops_mmio, s,
                          TYPE_ANDROID_GOLDFISH_EVENTS,
                          ANDROID_GOLDFISH_EVENTS_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    s->input_handler = qemu_input_handler_register(
        DEVICE(s), &android_goldfish_events_handler);
}

static Property android_goldfish_events_properties[] = {
    DEFINE_PROP_UINT32("width", AndroidGoldfishEventsState, width, 320),
    DEFINE_PROP_UINT32("height", AndroidGoldfishEventsState, height, 480),
    DEFINE_PROP_END_OF_LIST(),
};

static void android_goldfish_events_class_init(ObjectClass *klass, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    (void)data;
    dc->realize = android_goldfish_events_realize;
    dc->vmsd = &vmstate_android_goldfish_events;
    GF_SET_PROPS(dc, android_goldfish_events_properties);
}

static const TypeInfo android_goldfish_events_type_info = {
    .name = TYPE_ANDROID_GOLDFISH_EVENTS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishEventsState),
    .class_init = android_goldfish_events_class_init,
};

static void android_goldfish_events_register_types(void)
{
    type_register_static(&android_goldfish_events_type_info);
}

type_init(android_goldfish_events_register_types)
