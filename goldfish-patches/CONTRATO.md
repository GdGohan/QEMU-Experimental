# Contrato de portabilidade (obrigatorio para todo arquivo em base/)

Todo dispositivo goldfish escrito aqui tem que compilar, SEM alteracao, em:
QEMU 2.9.1, 3.1.1, 4.2.1, 5.1.0 e no fork moderno qemu-wasm (~9.x).

## 1. Nomes (evitar colisao com o upstream)

O QEMU 5.1+ ja tem `goldfish_rtc` e o 5.2+ tem `goldfish_pic`/`goldfish_tty`
(usados pela maquina m68k `virt`). Por isso TODO nome nosso e prefixado:

| item | padrao |
|---|---|
| arquivo fonte | `hw/<subdir>/android_goldfish_<x>.c` |
| header | `include/hw/<subdir>/android_goldfish_<x>.h` |
| nome no repo de patches | `hw_<subdir>_android_goldfish_<x>.c` |
| tipo QOM | `"android-goldfish-<x>"` em `TYPE_ANDROID_GOLDFISH_<X>` |
| struct | `AndroidGoldfish<X>State` |
| macro de cast | `ANDROID_GOLDFISH_<X>(obj)` |
| simbolo Kconfig | `CONFIG_ANDROID_GOLDFISH_<X>` |

## 2. Cabecalho obrigatorio de todo .c

```c
#include "qemu/osdep.h"
#include "hw/goldfish/compat.h"     /* SEMPRE em segundo lugar */
/* ... headers especificos depois ... */
```

O `compat.h` ja inclui: qemu/module.h, qemu/log.h, qemu/timer.h,
qapi/error.h, migration/vmstate.h, hw/sysbus.h, hw/irq.h,
hw/qdev-properties.h, exec/address-spaces.h, exec/cpu-common.h,
sysemu/sysemu.h (ou system/sysemu.h) e o chardev correto da era.
Nao inclua nenhum desses de novo.

## 3. Proibido (nao existe em alguma das versoes alvo)

- `#include "trace.h"` e `trace_*()` -> usar `qemu_log_mask(LOG_UNIMP|LOG_GUEST_ERROR, ...)`
- `#include "hw/hw.h"` (removido na 4.0)
- `OBJECT_DECLARE_SIMPLE_TYPE`, `DECLARE_INSTANCE_CHECKER`, `OBJECT_DEFINE_TYPE` (5.0+/5.2+)
- `dc->reset` / `device_class_set_legacy_reset` -> NAO registrar reset;
  inicializar todo o estado no `realize`
- `dc->props = ...` direto -> usar `GF_SET_PROPS(dc, props)`
- `qdev_init_nofail` / `qdev_realize` direto -> usar `GF_REALIZE(dev)`
- `object_property_set_bool` (ordem dos argumentos mudou na 5.2)
- `qdev_new`/`sysbus_realize_and_unref` (5.1+) -> usar `sysbus_create_simple()`
- `InterruptStatsProvider` / `hw/intc/intc.h`
- `memory_region_allocate_system_memory` direto (removido na 5.2)
- `g_autofree` e afins: permitidos (glib), mas prefira liberacao explicita

## 4. Padrao de dispositivo

```c
#define TYPE_ANDROID_GOLDFISH_X "android-goldfish-x"
#define ANDROID_GOLDFISH_X(obj) \
    OBJECT_CHECK(AndroidGoldfishXState, (obj), TYPE_ANDROID_GOLDFISH_X)

struct AndroidGoldfishXState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    /* ... */
};
```

- Uma regiao MMIO de `0x1000` bytes por dispositivo.
- `MemoryRegionOps`: `.endianness = DEVICE_NATIVE_ENDIAN`,
  `.valid.min_access_size = 4`, `.valid.max_access_size = 4`.
- Acesso desconhecido: `qemu_log_mask(LOG_UNIMP, ...)` e retornar 0.
- Migracao via `dc->vmsd` (nunca `vmstate_register`).
- `realize` faz `memory_region_init_io` + `sysbus_init_mmio` + `sysbus_init_irq`.
- O device NAO se instancia sozinho: a placa usa `sysbus_create_simple()`.

## 5. Chardev (so o tty usa)

Use exclusivamente os wrappers do compat.h, que escondem a troca
`CharDriverState*` -> `CharBackend` da 2.12:

- campo do struct: `GF_CHR_FIELD;`
- `gf_chr_connected(s)`, `gf_chr_write_all(s, buf, len)`,
  `gf_chr_accept_input(s)`, `gf_chr_set_handlers(s, can_read, read, ev, opaque)`
- propriedade: `DEFINE_PROP_CHR("chardev", AndroidGoldfishTtyState, chr)`

## 6. Memoria do convidado (fb e pdev_bus)

`gf_guest_read(addr, buf, len)` / `gf_guest_write(addr, buf, len)`.

## 7. Mapa de hardware da placa (autoritativo)

Vem de `arch/arm/mach-goldfish/include/mach/hardware.h` do kernel goldfish
(IO_START 0xff000000) e dos drivers em `drivers/` do Linux mainline.

| endereco | dispositivo | IRQ |
|---|---|---|
| 0xff000000 | interrupt controller (pic) | — (cascata na CPU) |
| 0xff001000 | pdev_bus | 1 |
| 0xff002000 | tty | 4 |
| 0xff003000 | timer/rtc | 3 |
| 0xff010000+ | dispositivos dinamicos (fb, events, battery, ...) | 5+ |

RAM em 0x00000000. Boot por ATAGS (sem device tree).
`MACH_TYPE_GOLDFISH = 1441`.
