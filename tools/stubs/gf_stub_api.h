/*
 * Stubs MINIMOS da API do QEMU -- somente para checagem de sintaxe/tipos
 * dos arquivos goldfish fora da arvore do QEMU (tools/check-syntax.sh).
 * Nao e reimplementacao de nada e nao entra no build real.
 */
#ifndef GF_STUB_API_H
#define GF_STUB_API_H

#include "qemu/osdep.h"

/*
 * Le so o header de versao gerado (o mesmo que o compat.h usa) para decidir
 * quais assinaturas de API emular. Nao inclui o compat.h: isso daria include
 * circular, ja que o compat.h inclui os headers que este arquivo emula.
 */
#include "hw/goldfish/gf-version.h"
#define GF_STUB_ATLEAST(maj, min) \
    ((GF_VER_MAJOR > (maj)) || \
     (GF_VER_MAJOR == (maj) && GF_VER_MINOR >= (min)))

#define g_assert(x)         ((void)0)
#define g_assert_not_reached()  abort()

/* ---------------- QOM ---------------- */
typedef struct ObjectClass ObjectClass;
typedef struct Object Object;
typedef struct DeviceState DeviceState;
typedef struct Error Error;

extern Error *error_abort;
extern Error *error_fatal;

typedef struct TypeInfo {
    const char *name;
    const char *parent;
    size_t instance_size;
    void (*instance_init)(Object *obj);
#if GF_STUB_ATLEAST(10, 0)
    void (*class_init)(ObjectClass *klass, const void *data);
#else
    void (*class_init)(ObjectClass *klass, void *data);
#endif
} TypeInfo;

void type_register_static(const TypeInfo *info);
#define type_init(fn) \
    static void __attribute__((unused)) gf_stub_type_init_##fn(void) { fn(); }

#define OBJECT(obj)             ((Object *)(obj))
#define DEVICE(obj)             ((DeviceState *)(obj))
#define OBJECT_CHECK(type, obj, name)  ((type *)(obj))
#define OBJECT_CLASS_CHECK(c, k, n)    ((c *)(k))
Object *object_new(const char *type);

/* ---------------- memoria ---------------- */
typedef struct MemoryRegion MemoryRegion;
struct MemoryRegion { int dummy; };

typedef enum {
    DEVICE_NATIVE_ENDIAN, DEVICE_LITTLE_ENDIAN, DEVICE_BIG_ENDIAN
} gf_endian;

typedef struct MemoryRegionOps {
    uint64_t (*read)(void *opaque, hwaddr addr, unsigned size);
    void (*write)(void *opaque, hwaddr addr, uint64_t data, unsigned size);
    gf_endian endianness;
    struct { unsigned min_access_size; unsigned max_access_size; } valid;
    struct { unsigned min_access_size; unsigned max_access_size; } impl;
} MemoryRegionOps;

void memory_region_init_io(MemoryRegion *mr, Object *owner,
                           const MemoryRegionOps *ops, void *opaque,
                           const char *name, uint64_t size);
void memory_region_add_subregion(MemoryRegion *parent, hwaddr off,
                                 MemoryRegion *sub);
MemoryRegion *get_system_memory(void);
/* saiu na 5.0: declarar sempre esconderia erro de fronteira de versao */
#if !GF_STUB_ATLEAST(5, 0)
void memory_region_allocate_system_memory(MemoryRegion *mr, Object *owner,
                                          const char *name, uint64_t size);
#endif

void cpu_physical_memory_read(hwaddr addr, void *buf, uint64_t len);
void cpu_physical_memory_write(hwaddr addr, const void *buf, uint64_t len);

/* ---------------- irq / qdev / sysbus ---------------- */
typedef struct IRQState *qemu_irq;
void qemu_set_irq(qemu_irq irq, int level);
void qemu_irq_raise(qemu_irq irq);
void qemu_irq_lower(qemu_irq irq);
void qemu_irq_pulse(qemu_irq irq);
qemu_irq qdev_get_gpio_in(DeviceState *dev, int n);
void qdev_init_gpio_in(DeviceState *dev,
                       void (*handler)(void *opaque, int n, int level), int n);

typedef struct Property { int dummy; } Property;
typedef struct VMStateDescription VMStateDescription;

typedef struct DeviceClass {
    void (*realize)(DeviceState *dev, Error **errp);
    const VMStateDescription *vmsd;
    const char *desc;
    Property *props;
} DeviceClass;

#define DEVICE_CLASS(klass) ((DeviceClass *)(klass))
void device_class_set_props(DeviceClass *dc, Property *props);

#define DEFINE_PROP_UINT32(n, s, f, d)  ((Property){0})
#define DEFINE_PROP_UINT64(n, s, f, d)  ((Property){0})
#define DEFINE_PROP_BOOL(n, s, f, d)    ((Property){0})
#define DEFINE_PROP_CHR(n, s, f)        ((Property){0})
#define DEFINE_PROP_END_OF_LIST()       ((Property){0})

typedef struct SysBusDevice { int dummy; } SysBusDevice;
#define TYPE_SYS_BUS_DEVICE "sys-bus-device"
#define SYS_BUS_DEVICE(obj) ((SysBusDevice *)(obj))
void sysbus_init_mmio(SysBusDevice *dev, MemoryRegion *mr);
void sysbus_init_irq(SysBusDevice *dev, qemu_irq *p);
DeviceState *sysbus_create_simple(const char *name, hwaddr addr, qemu_irq irq);

/* qdev_init_nofail saiu na 5.1, substituido por qdev_realize */
#if GF_STUB_ATLEAST(5, 1)
bool qdev_realize_and_unref(DeviceState *dev, void *bus, Error **errp);
bool qdev_realize(DeviceState *dev, void *bus, Error **errp);
#else
void qdev_init_nofail(DeviceState *dev);
#endif
DeviceState *qdev_new(const char *name);
void sysbus_realize_and_unref(SysBusDevice *dev, Error **errp);
void sysbus_mmio_map(SysBusDevice *dev, int n, hwaddr addr);
void sysbus_connect_irq(SysBusDevice *dev, int n, qemu_irq irq);

/* ---------------- vmstate ---------------- */
typedef struct VMStateField { int dummy; } VMStateField;
struct VMStateDescription {
    const char *name;
    int version_id;
    int minimum_version_id;
    int (*post_load)(void *opaque, int version_id);
#if GF_STUB_ATLEAST(9, 0)
    const VMStateField *fields;
#else
    VMStateField *fields;
#endif
};

#define VMSTATE_UINT8(f, s)             ((VMStateField){0})
#define VMSTATE_UINT16(f, s)            ((VMStateField){0})
#define VMSTATE_UINT32(f, s)            ((VMStateField){0})
#define VMSTATE_UINT64(f, s)            ((VMStateField){0})
#define VMSTATE_INT32(f, s)             ((VMStateField){0})
#define VMSTATE_BOOL(f, s)              ((VMStateField){0})
#define VMSTATE_UINT8_ARRAY(f, s, n)    ((VMStateField){0})
#define VMSTATE_UINT32_ARRAY(f, s, n)   ((VMStateField){0})
#define VMSTATE_TIMER_PTR(f, s)         ((VMStateField){0})
#define VMSTATE_END_OF_LIST()           ((VMStateField){0})

/* ---------------- log ---------------- */
#define LOG_UNIMP   1
#define LOG_GUEST_ERROR 2
void qemu_log_mask(int mask, const char *fmt, ...);
void error_report(const char *fmt, ...);
void error_setg(Error **errp, const char *fmt, ...);

/* ---------------- timer ---------------- */
typedef struct QEMUTimer QEMUTimer;
typedef enum { QEMU_CLOCK_VIRTUAL, QEMU_CLOCK_REALTIME, QEMU_CLOCK_HOST } QEMUClockType;
int64_t qemu_clock_get_ns(QEMUClockType type);
QEMUTimer *timer_new_ns(QEMUClockType type, void (*cb)(void *), void *opaque);
void timer_mod(QEMUTimer *ts, int64_t expire);
void timer_mod_ns(QEMUTimer *ts, int64_t expire);
void timer_del(QEMUTimer *ts);
#define NANOSECONDS_PER_SECOND 1000000000LL

/* ---------------- endianness ---------------- */
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)

/* ---------------- chardev ---------------- */
#if !GF_STUB_ATLEAST(2, 12)
typedef struct CharDriverState CharDriverState;
int qemu_chr_fe_write_all(CharDriverState *s, const uint8_t *buf, int len);
void qemu_chr_accept_input(CharDriverState *s);
void qemu_chr_add_handlers(CharDriverState *s, int (*can_read)(void *),
                           void (*read)(void *, const uint8_t *, int),
                           void (*event)(void *, int), void *opaque);
extern CharDriverState *serial_hds[4];
#else
typedef struct Chardev Chardev;
typedef struct CharBackend { int dummy; } CharBackend;
bool qemu_chr_fe_backend_connected(CharBackend *be);
int qemu_chr_fe_write_all(CharBackend *be, const uint8_t *buf, int len);
void qemu_chr_fe_accept_input(CharBackend *be);
bool qemu_chr_fe_init(CharBackend *be, Chardev *s, Error **errp);
void qemu_chr_fe_set_handlers(CharBackend *be, int (*can_read)(void *),
                              void (*read)(void *, const uint8_t *, int),
                              void (*event)(void *, unsigned),
                              void (*be_change)(void *),
                              void *opaque, void *context, bool set_open);
Chardev *serial_hd(int i);
extern Chardev *serial_hds[4];   /* pre-4.0 */
#endif

/* ---------------- ui / console ---------------- */
typedef struct QemuConsole QemuConsole;
typedef struct DisplaySurface DisplaySurface;
typedef struct GraphicHwOps {
    void (*invalidate)(void *opaque);
    void (*gfx_update)(void *opaque);
} GraphicHwOps;
QemuConsole *graphic_console_init(DeviceState *dev, uint32_t head,
                                  const GraphicHwOps *ops, void *opaque);
void qemu_console_resize(QemuConsole *con, int width, int height);
DisplaySurface *qemu_console_surface(QemuConsole *con);
int surface_bits_per_pixel(DisplaySurface *s);
int surface_stride(DisplaySurface *s);
int surface_height(DisplaySurface *s);
int surface_width(DisplaySurface *s);
void *surface_data(DisplaySurface *s);
void dpy_gfx_update(QemuConsole *con, int x, int y, int w, int h);

/* ---------------- input ---------------- */
typedef enum QKeyCode {
    Q_KEY_CODE_0,
    Q_KEY_CODE_1,
    Q_KEY_CODE_2,
    Q_KEY_CODE_3,
    Q_KEY_CODE_4,
    Q_KEY_CODE_5,
    Q_KEY_CODE_6,
    Q_KEY_CODE_7,
    Q_KEY_CODE_8,
    Q_KEY_CODE_9,
    Q_KEY_CODE_A,
    Q_KEY_CODE_ALT,
    Q_KEY_CODE_ALT_R,
    Q_KEY_CODE_B,
    Q_KEY_CODE_BACKSPACE,
    Q_KEY_CODE_C,
    Q_KEY_CODE_CTRL,
    Q_KEY_CODE_CTRL_R,
    Q_KEY_CODE_D,
    Q_KEY_CODE_DOWN,
    Q_KEY_CODE_E,
    Q_KEY_CODE_END,
    Q_KEY_CODE_ESC,
    Q_KEY_CODE_F,
    Q_KEY_CODE_F1,
    Q_KEY_CODE_F2,
    Q_KEY_CODE_F3,
    Q_KEY_CODE_F5,
    Q_KEY_CODE_F6,
    Q_KEY_CODE_G,
    Q_KEY_CODE_H,
    Q_KEY_CODE_I,
    Q_KEY_CODE_J,
    Q_KEY_CODE_K,
    Q_KEY_CODE_L,
    Q_KEY_CODE_LEFT,
    Q_KEY_CODE_M,
    Q_KEY_CODE_N,
    Q_KEY_CODE_O,
    Q_KEY_CODE_P,
    Q_KEY_CODE_Q,
    Q_KEY_CODE_R,
    Q_KEY_CODE_RET,
    Q_KEY_CODE_RIGHT,
    Q_KEY_CODE_S,
    Q_KEY_CODE_SHIFT,
    Q_KEY_CODE_SHIFT_R,
    Q_KEY_CODE_SPC,
    Q_KEY_CODE_T,
    Q_KEY_CODE_TAB,
    Q_KEY_CODE_U,
    Q_KEY_CODE_UP,
    Q_KEY_CODE_V,
    Q_KEY_CODE_W,
    Q_KEY_CODE_X,
    Q_KEY_CODE_Y,
    Q_KEY_CODE_Z,
    Q_KEY_CODE__MAX
} QKeyCode;

typedef struct QemuInputHandlerState QemuInputHandlerState;
typedef struct InputEvent InputEvent;
/*
 * InputEvent e uma uniao QAPI: cada membro e um "wrapper" com um ponteiro
 * .data. Por isso o acesso correto e evt->u.key.data->key, e nao
 * evt->u.key->key.
 */
typedef struct KeyValue { int qcode; int number; } KeyValue;
typedef struct InputKeyEvent { KeyValue *key; bool down; } InputKeyEvent;
typedef struct InputMoveEvent { int axis; int value; } InputMoveEvent;
typedef struct InputBtnEvent { int button; bool down; } InputBtnEvent;
typedef struct { InputKeyEvent *data; } InputKeyEventWrapper;
typedef struct { InputMoveEvent *data; } InputMoveEventWrapper;
typedef struct { InputBtnEvent *data; } InputBtnEventWrapper;
struct InputEvent {
    int type;
    union {
        InputKeyEventWrapper key;
        InputMoveEventWrapper abs;
        InputMoveEventWrapper rel;
        InputBtnEventWrapper btn;
    } u;
};
#define INPUT_EVENT_KIND_KEY 0
#define INPUT_EVENT_KIND_ABS 1
#define INPUT_EVENT_KIND_REL 2
#define INPUT_EVENT_KIND_BTN 3
#define INPUT_EVENT_MASK_KEY 1
#define INPUT_EVENT_MASK_BTN 2
#define INPUT_EVENT_MASK_ABS 4
#define INPUT_EVENT_MASK_REL 8
#define INPUT_BUTTON_LEFT 0
#define INPUT_AXIS_X 0
#define INPUT_AXIS_Y 1
typedef struct QemuInputHandler {
    const char *name;
    uint32_t mask;
    void (*event)(DeviceState *dev, QemuConsole *src, InputEvent *evt);
    void (*sync)(DeviceState *dev);
} QemuInputHandler;
QemuInputHandlerState *qemu_input_handler_register(DeviceState *dev,
                                                   QemuInputHandler *handler);
int qemu_input_key_value_to_qcode(const KeyValue *value);
int qemu_input_key_value_to_number(const KeyValue *value);
int qemu_input_scale_axis(int value, int min_in, int max_in,
                          int min_out, int max_out);

/* ---------------- machine / net ---------------- */
typedef struct MachineState {
    uint64_t ram_size;
    const char *cpu_type;
    const char *cpu_model;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
#if GF_STUB_ATLEAST(5, 0)
    MemoryRegion *ram;          /* so existe a partir da 5.0 */
#endif
} MachineState;

typedef enum { IF_NONE, IF_SD } BlockInterfaceType;

typedef struct MachineClass {
    const char *desc;
    void (*init)(MachineState *ms);
    int max_cpus;
    const char *default_cpu_type;
#if GF_STUB_ATLEAST(5, 0)
    const char *default_ram_id; /* so existe a partir da 5.0 */
#endif
    uint64_t default_ram_size;
    bool ignore_memory_transaction_failures;
    BlockInterfaceType block_default_type;
} MachineClass;

#define DEFINE_MACHINE(namestr, initfn)                                 \
    static void gf_stub_machine_##initfn(MachineClass *mc) { initfn(mc); }

typedef struct NICInfo { int used; } NICInfo;
extern NICInfo nd_table[8];
void qemu_check_nic_model(NICInfo *nd, const char *model);
void smc91c111_init(NICInfo *nd, uint32_t base, qemu_irq irq);

/* ---------------- ARM ---------------- */
typedef struct ARMCPU ARMCPU;
#define ARM_CPU(obj)  ((ARMCPU *)(obj))
#define TYPE_ARM_CPU  "arm-cpu"
#define ARM_CPU_TYPE_NAME(name) (name "-" TYPE_ARM_CPU)
enum { ARM_CPU_IRQ, ARM_CPU_FIQ };
Object *cpu_generic_init(const char *typename, const char *model);

struct arm_boot_info {
    uint64_t ram_size;
    int board_id;
    hwaddr loader_start;
    int nb_cpus;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
};
#if GF_STUB_ATLEAST(4, 2)
void arm_load_kernel(ARMCPU *cpu, MachineState *ms, struct arm_boot_info *info);
#else
void arm_load_kernel(ARMCPU *cpu, struct arm_boot_info *info);
#endif

#endif /* GF_STUB_API_H */
