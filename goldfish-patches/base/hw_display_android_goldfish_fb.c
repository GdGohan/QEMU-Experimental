/*
 * Framebuffer goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/display/android_goldfish_fb.h"
#include "ui/console.h"

#define ANDROID_GOLDFISH_FB_MMIO_SIZE       0x1000
#define ANDROID_GOLDFISH_FB_VSYNC_NS        16666667LL

enum {
    ANDROID_GOLDFISH_FB_GET_WIDTH = 0x00,
    ANDROID_GOLDFISH_FB_GET_HEIGHT = 0x04,
    ANDROID_GOLDFISH_FB_INT_STATUS = 0x08,
    ANDROID_GOLDFISH_FB_INT_ENABLE = 0x0c,
    ANDROID_GOLDFISH_FB_SET_BASE = 0x10,
    ANDROID_GOLDFISH_FB_SET_ROTATION = 0x14,
    ANDROID_GOLDFISH_FB_SET_BLANK = 0x18,
    ANDROID_GOLDFISH_FB_GET_PHYS_WIDTH = 0x1c,
    ANDROID_GOLDFISH_FB_GET_PHYS_HEIGHT = 0x20,
    ANDROID_GOLDFISH_FB_GET_FORMAT = 0x24,
};

#define ANDROID_GOLDFISH_FB_INT_VSYNC            (1U << 0)
#define ANDROID_GOLDFISH_FB_INT_BASE_UPDATE_DONE (1U << 1)

struct AndroidGoldfishFbState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    QemuConsole *con;
    QEMUTimer *vsync_timer;
    uint32_t width;
    uint32_t height;
    uint64_t base;
    uint32_t int_status;
    uint32_t int_enable;
    uint32_t rotation;
    uint32_t blank;
};

static void android_goldfish_fb_update_irq(AndroidGoldfishFbState *s)
{
    qemu_set_irq(s->irq, !!(s->int_status & s->int_enable));
}

static void android_goldfish_fb_vsync_control(AndroidGoldfishFbState *s)
{
    timer_del(s->vsync_timer);
    if (s->int_enable & ANDROID_GOLDFISH_FB_INT_VSYNC) {
        timer_mod_ns(s->vsync_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                     ANDROID_GOLDFISH_FB_VSYNC_NS);
    }
}

static void android_goldfish_fb_render(AndroidGoldfishFbState *s)
{
    DisplaySurface *surface;
    uint16_t *line;
    uint32_t x;
    uint32_t y;
    int bpp;

    if (!s->con) {
        return;
    }

    surface = qemu_console_surface(s->con);
    if (!surface) {
        return;
    }

    bpp = surface_bits_per_pixel(surface);
    if (bpp != 16 && bpp != 32) {
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-fb: surface com %d bpp\n", bpp);
        return;
    }

    if (s->blank) {
        memset(surface_data(surface), 0,
               surface_stride(surface) * surface_height(surface));
        dpy_gfx_update(s->con, 0, 0, s->width, s->height);
        return;
    }

    line = g_new(uint16_t, s->width);
    for (y = 0; y < s->height; y++) {
        uint8_t *dst = (uint8_t *)surface_data(surface) +
                       y * surface_stride(surface);

        gf_guest_read(s->base + (uint64_t)y * s->width * 2,
                      line, s->width * 2);
        if (bpp == 16) {
            memcpy(dst, line, s->width * 2);
        } else {
            uint32_t *pixels = (uint32_t *)dst;

            for (x = 0; x < s->width; x++) {
                uint16_t rgb565 = le16_to_cpu(line[x]);
                uint32_t red = (rgb565 >> 11) & 0x1f;
                uint32_t green = (rgb565 >> 5) & 0x3f;
                uint32_t blue = rgb565 & 0x1f;

                red = (red << 3) | (red >> 2);
                green = (green << 2) | (green >> 4);
                blue = (blue << 3) | (blue >> 2);
                pixels[x] = 0xff000000 | (red << 16) |
                            (green << 8) | blue;
            }
        }
    }
    g_free(line);
    dpy_gfx_update(s->con, 0, 0, s->width, s->height);
}

static void android_goldfish_fb_invalidate(void *opaque)
{
    android_goldfish_fb_render(opaque);
}

static void android_goldfish_fb_gfx_update(void *opaque)
{
    android_goldfish_fb_render(opaque);
}

static const GraphicHwOps android_goldfish_fb_ops = {
    .invalidate = android_goldfish_fb_invalidate,
    .gfx_update = android_goldfish_fb_gfx_update,
};

static void android_goldfish_fb_vsync(void *opaque)
{
    AndroidGoldfishFbState *s = opaque;

    if (!(s->int_enable & ANDROID_GOLDFISH_FB_INT_VSYNC)) {
        return;
    }

    s->int_status |= ANDROID_GOLDFISH_FB_INT_VSYNC;
    android_goldfish_fb_update_irq(s);
    timer_mod_ns(s->vsync_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                 ANDROID_GOLDFISH_FB_VSYNC_NS);
}

static uint64_t android_goldfish_fb_read(void *opaque, hwaddr addr,
                                         unsigned size)
{
    AndroidGoldfishFbState *s = opaque;
    uint32_t value = 0;

    (void)size;
    switch (addr) {
    case ANDROID_GOLDFISH_FB_GET_WIDTH:
        value = s->width;
        break;
    case ANDROID_GOLDFISH_FB_GET_HEIGHT:
        value = s->height;
        break;
    case ANDROID_GOLDFISH_FB_INT_STATUS:
        value = s->int_status;
        s->int_status = 0;
        qemu_set_irq(s->irq, 0);
        break;
    case ANDROID_GOLDFISH_FB_INT_ENABLE:
        value = s->int_enable;
        break;
    case ANDROID_GOLDFISH_FB_GET_PHYS_WIDTH:
        value = s->width;
        break;
    case ANDROID_GOLDFISH_FB_GET_PHYS_HEIGHT:
        value = s->height;
        break;
    case ANDROID_GOLDFISH_FB_GET_FORMAT:
        value = 0;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-fb: leitura desconhecida em 0x%"
                      HWADDR_PRIx "\n", addr);
        break;
    }
    return value;
}

static void android_goldfish_fb_write(void *opaque, hwaddr addr,
                                      uint64_t value, unsigned size)
{
    AndroidGoldfishFbState *s = opaque;

    (void)size;
    switch (addr) {
    case ANDROID_GOLDFISH_FB_INT_ENABLE:
        s->int_enable = value;
        android_goldfish_fb_update_irq(s);
        android_goldfish_fb_vsync_control(s);
        break;
    case ANDROID_GOLDFISH_FB_SET_BASE:
        s->base = value;
        s->int_status |= ANDROID_GOLDFISH_FB_INT_BASE_UPDATE_DONE;
        android_goldfish_fb_update_irq(s);
        android_goldfish_fb_render(s);
        break;
    case ANDROID_GOLDFISH_FB_SET_ROTATION:
        s->rotation = value;
        android_goldfish_fb_render(s);
        break;
    case ANDROID_GOLDFISH_FB_SET_BLANK:
        s->blank = value;
        android_goldfish_fb_render(s);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "android-goldfish-fb: escrita desconhecida em 0x%"
                      HWADDR_PRIx "\n", addr);
        break;
    }
}

static const MemoryRegionOps android_goldfish_fb_ops_mmio = {
    .read = android_goldfish_fb_read,
    .write = android_goldfish_fb_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_android_goldfish_fb = {
    .name = TYPE_ANDROID_GOLDFISH_FB,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(base, AndroidGoldfishFbState),
        VMSTATE_UINT32(int_status, AndroidGoldfishFbState),
        VMSTATE_UINT32(int_enable, AndroidGoldfishFbState),
        VMSTATE_UINT32(rotation, AndroidGoldfishFbState),
        VMSTATE_UINT32(blank, AndroidGoldfishFbState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_fb_realize(DeviceState *dev, Error **errp)
{
    AndroidGoldfishFbState *s = ANDROID_GOLDFISH_FB(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    (void)errp;
    s->base = 0;
    s->int_status = 0;
    s->int_enable = 0;
    s->rotation = 0;
    s->blank = 0;
    memory_region_init_io(&s->iomem, OBJECT(s),
                          &android_goldfish_fb_ops_mmio, s,
                          TYPE_ANDROID_GOLDFISH_FB,
                          ANDROID_GOLDFISH_FB_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    s->con = graphic_console_init(DEVICE(s), 0, &android_goldfish_fb_ops, s);
    qemu_console_resize(s->con, s->width, s->height);
    s->vsync_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                  android_goldfish_fb_vsync, s);
}

static Property android_goldfish_fb_properties[] = {
    DEFINE_PROP_UINT32("width", AndroidGoldfishFbState, width, 320),
    DEFINE_PROP_UINT32("height", AndroidGoldfishFbState, height, 480),
    DEFINE_PROP_END_OF_LIST(),
};

static void android_goldfish_fb_class_init(ObjectClass *klass, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    (void)data;
    dc->realize = android_goldfish_fb_realize;
    dc->vmsd = &vmstate_android_goldfish_fb;
    GF_SET_PROPS(dc, android_goldfish_fb_properties);
}

static const TypeInfo android_goldfish_fb_type_info = {
    .name = TYPE_ANDROID_GOLDFISH_FB,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishFbState),
    .class_init = android_goldfish_fb_class_init,
};

static void android_goldfish_fb_register_types(void)
{
    type_register_static(&android_goldfish_fb_type_info);
}

type_init(android_goldfish_fb_register_types)
