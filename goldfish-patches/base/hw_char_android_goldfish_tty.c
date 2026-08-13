/*
 * Terminal serial da placa Android goldfish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"
#include "hw/char/android_goldfish_tty.h"

#define TTY_PUT_CHAR       0x00
#define TTY_BYTES_READY    0x04
#define TTY_CMD            0x08
#define TTY_DATA_PTR       0x10
#define TTY_DATA_LEN       0x14
#define TTY_DATA_PTR_HIGH  0x18
#define TTY_VERSION        0x20

#define TTY_CMD_INT_DISABLE   0
#define TTY_CMD_INT_ENABLE    1
#define TTY_CMD_WRITE_BUFFER  2
#define TTY_CMD_READ_BUFFER   3

static bool android_goldfish_tty_rx_empty(AndroidGoldfishTtyState *s)
{
    return s->rx_count == 0;
}

static uint32_t android_goldfish_tty_rx_free(AndroidGoldfishTtyState *s)
{
    return ANDROID_GOLDFISH_TTY_BUFFER_SIZE - s->rx_count;
}

static void android_goldfish_tty_update_irq(AndroidGoldfishTtyState *s)
{
    qemu_set_irq(s->irq, s->int_enabled && !android_goldfish_tty_rx_empty(s));
}

static uint64_t android_goldfish_tty_read(void *opaque, hwaddr addr,
                                          unsigned size)
{
    AndroidGoldfishTtyState *s = opaque;

    switch (addr) {
    case TTY_BYTES_READY:
        return s->rx_count;
    case TTY_VERSION:
        /* O kernel Android antigo espera versao zero, e nao a versao um. */
        return 0;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: leitura de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        return 0;
    }
}

static void android_goldfish_tty_write_buffer(AndroidGoldfishTtyState *s)
{
    uint8_t buffer[ANDROID_GOLDFISH_TTY_BUFFER_SIZE];
    uint64_t ptr = s->data_ptr;
    uint32_t remaining = s->data_len;

    while (remaining) {
        uint32_t length = MIN(remaining, (uint32_t)sizeof(buffer));

        gf_guest_read(ptr, buffer, length);
        if (gf_chr_connected(s)) {
            gf_chr_write_all(s, buffer, length);
        }
        ptr += length;
        remaining -= length;
    }
}

static void android_goldfish_tty_read_buffer(AndroidGoldfishTtyState *s)
{
    uint8_t buffer[ANDROID_GOLDFISH_TTY_BUFFER_SIZE];
    uint64_t ptr = s->data_ptr;
    uint32_t remaining = s->data_len;

    while (remaining && !android_goldfish_tty_rx_empty(s)) {
        uint32_t length = MIN(remaining, (uint32_t)sizeof(buffer));
        uint32_t count = 0;

        while (count < length && !android_goldfish_tty_rx_empty(s)) {
            buffer[count++] = s->rx_buffer[s->rx_read];
            s->rx_read = (s->rx_read + 1) % ANDROID_GOLDFISH_TTY_BUFFER_SIZE;
            s->rx_count--;
        }
        gf_guest_write(ptr, buffer, count);
        ptr += count;
        remaining -= count;
    }
    android_goldfish_tty_update_irq(s);
    if (gf_chr_connected(s)) {
        gf_chr_accept_input(s);
    }
}

static void android_goldfish_tty_command(AndroidGoldfishTtyState *s,
                                         uint32_t command)
{
    switch (command) {
    case TTY_CMD_INT_DISABLE:
        s->int_enabled = false;
        android_goldfish_tty_update_irq(s);
        break;
    case TTY_CMD_INT_ENABLE:
        s->int_enabled = true;
        android_goldfish_tty_update_irq(s);
        break;
    case TTY_CMD_WRITE_BUFFER:
        android_goldfish_tty_write_buffer(s);
        break;
    case TTY_CMD_READ_BUFFER:
        android_goldfish_tty_read_buffer(s);
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: comando nao implementado 0x%x\n",
                      __func__, command);
        break;
    }
}

static void android_goldfish_tty_write(void *opaque, hwaddr addr,
                                       uint64_t value, unsigned size)
{
    AndroidGoldfishTtyState *s = opaque;
    uint8_t c;

    switch (addr) {
    case TTY_PUT_CHAR:
        c = value;
        if (gf_chr_connected(s)) {
            gf_chr_write_all(s, &c, 1);
        }
        break;
    case TTY_CMD:
        android_goldfish_tty_command(s, value);
        break;
    case TTY_DATA_PTR:
        s->data_ptr = (s->data_ptr & UINT64_C(0xffffffff00000000)) |
                      (uint32_t)value;
        break;
    case TTY_DATA_LEN:
        s->data_len = value;
        break;
    case TTY_DATA_PTR_HIGH:
        s->data_ptr = (s->data_ptr & UINT64_C(0x00000000ffffffff)) |
                      ((uint64_t)(uint32_t)value << 32);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: escrita de registrador nao implementado 0x%02"HWADDR_PRIx"\n",
                      __func__, addr);
        break;
    }
}

static const MemoryRegionOps android_goldfish_tty_ops = {
    .read = android_goldfish_tty_read,
    .write = android_goldfish_tty_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static int android_goldfish_tty_can_receive(void *opaque)
{
    AndroidGoldfishTtyState *s = opaque;

    return android_goldfish_tty_rx_free(s);
}

static void android_goldfish_tty_receive(void *opaque, const uint8_t *buffer,
                                         int size)
{
    AndroidGoldfishTtyState *s = opaque;

    g_assert(size <= android_goldfish_tty_rx_free(s));
    while (size--) {
        s->rx_buffer[s->rx_write] = *buffer++;
        s->rx_write = (s->rx_write + 1) % ANDROID_GOLDFISH_TTY_BUFFER_SIZE;
        s->rx_count++;
    }
    android_goldfish_tty_update_irq(s);
}

static const VMStateDescription vmstate_android_goldfish_tty = {
    .name = TYPE_ANDROID_GOLDFISH_TTY,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(data_len, AndroidGoldfishTtyState),
        VMSTATE_UINT64(data_ptr, AndroidGoldfishTtyState),
        VMSTATE_BOOL(int_enabled, AndroidGoldfishTtyState),
        VMSTATE_UINT8_ARRAY(rx_buffer, AndroidGoldfishTtyState,
                            ANDROID_GOLDFISH_TTY_BUFFER_SIZE),
        VMSTATE_UINT32(rx_read, AndroidGoldfishTtyState),
        VMSTATE_UINT32(rx_write, AndroidGoldfishTtyState),
        VMSTATE_UINT32(rx_count, AndroidGoldfishTtyState),
        VMSTATE_END_OF_LIST()
    }
};

static void android_goldfish_tty_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *busdev = SYS_BUS_DEVICE(dev);
    AndroidGoldfishTtyState *s = ANDROID_GOLDFISH_TTY(dev);

    s->data_len = 0;
    s->data_ptr = 0;
    s->int_enabled = false;
    memset(s->rx_buffer, 0, sizeof(s->rx_buffer));
    s->rx_read = 0;
    s->rx_write = 0;
    s->rx_count = 0;
    memory_region_init_io(&s->iomem, OBJECT(s), &android_goldfish_tty_ops, s,
                          TYPE_ANDROID_GOLDFISH_TTY, 0x1000);
    sysbus_init_mmio(busdev, &s->iomem);
    sysbus_init_irq(busdev, &s->irq);

    /*
     * Se ninguem ligou um chardev na propriedade, pegamos o console serial
     * padrao aqui mesmo.
     *
     * Isso e de proposito: a placa cria os dispositivos com
     * sysbus_create_simple(), a unica API que faz create+realize+map+irq e
     * que existe igual da 2.9 a 9.x. Ela realiza o dispositivo na hora, ou
     * seja, nao da para setar propriedade antes do realize. As alternativas
     * (qdev_create/qdev_init_nofail) sairam na 6.0 e qdev_realize(dev, NULL)
     * dispara assert em SysBusDevice.
     */
    if (!gf_chr_connected(s)) {
        Chardev *serial = GF_SERIAL_HD(0);

        if (serial) {
            gf_chr_init(s, serial);
        }
    }

    if (gf_chr_connected(s)) {
        gf_chr_set_handlers(s, android_goldfish_tty_can_receive,
                            android_goldfish_tty_receive, NULL, s);
    }
}

static Property android_goldfish_tty_properties[] = {
    DEFINE_PROP_CHR("chardev", AndroidGoldfishTtyState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void android_goldfish_tty_class_init(ObjectClass *oc, GF_CLASS_INIT_DATA *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = android_goldfish_tty_realize;
    dc->vmsd = &vmstate_android_goldfish_tty;
    GF_SET_PROPS(dc, android_goldfish_tty_properties);
}

static const TypeInfo android_goldfish_tty_info = {
    .name = TYPE_ANDROID_GOLDFISH_TTY,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndroidGoldfishTtyState),
    .class_init = android_goldfish_tty_class_init,
};

static void android_goldfish_tty_register_types(void)
{
    type_register_static(&android_goldfish_tty_info);
}

type_init(android_goldfish_tty_register_types)
