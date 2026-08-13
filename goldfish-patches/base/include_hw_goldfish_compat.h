/*
 * Camada de compatibilidade para portar a placa goldfish (Android Emulator)
 * para QEMU 2.9.1 .. 9.x (e o fork qemu-wasm).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Estrategia:
 *   1. headers que MUDARAM DE LUGAR sao resolvidos com __has_include, entao
 *      nao dependem de numero de versao nenhum;
 *   2. APIs cuja ASSINATURA mudou sao resolvidas por GF_ATLEAST(maj,min).
 *
 * A versao NAO e adivinhada: o script de integracao le o arquivo VERSION da
 * raiz da arvore do QEMU e gera include/hw/goldfish/gf-version.h. Isso vale
 * inclusive para forks -- o qemu-wasm do ktock, por exemplo, tem VERSION
 * 8.2.0, ou seja, tratar "fork" como "versao mais nova de todas" estaria
 * errado (o class_init const, por exemplo, so aparece no QEMU 10).
 *
 * Este header precisa ser incluido DEPOIS de "qemu/osdep.h".
 */
#ifndef HW_GOLDFISH_COMPAT_H
#define HW_GOLDFISH_COMPAT_H

/* ------------------------------------------------------------------ */
/* deteccao de versao                                                  */
/* ------------------------------------------------------------------ */
#ifndef __has_include
#define __has_include(x) 0
#endif

/* gerado pelo ci/goldfish-integrate.sh a partir do arquivo VERSION */
#if __has_include("hw/goldfish/gf-version.h")
#include "hw/goldfish/gf-version.h"
#endif

#if !defined(GF_VER_MAJOR)
# if defined(QEMU_VERSION_MAJOR)
#  define GF_VER_MAJOR QEMU_VERSION_MAJOR
#  define GF_VER_MINOR QEMU_VERSION_MINOR
# else
/*
 * Sem versao nao ha como escolher as assinaturas certas, e chutar da erro de
 * link ou -- pior -- comportamento errado em silencio. Melhor falhar aqui.
 */
#  error "hw/goldfish/gf-version.h ausente: gere-o (ver ci/goldfish-integrate.sh) ou compile com -DGF_VER_MAJOR=x -DGF_VER_MINOR=y"
# endif
#endif

#define GF_ATLEAST(maj, min) \
    ((GF_VER_MAJOR > (maj)) || \
     (GF_VER_MAJOR == (maj) && GF_VER_MINOR >= (min)))

/* ------------------------------------------------------------------ */
/* headers comuns (presentes em todas as versoes suportadas)           */
/* ------------------------------------------------------------------ */
#include "qemu/module.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "exec/address-spaces.h"

/* ------------------------------------------------------------------ */
/* sysemu/sysemu.h -> system/sysemu.h (QEMU 10)                        */
/* ------------------------------------------------------------------ */
#if __has_include("system/sysemu.h")
#include "system/sysemu.h"
#else
#include "sysemu/sysemu.h"
#endif

/* serial_hds[] -> serial_hd() na 4.0 */
#if GF_ATLEAST(4, 0)
#define GF_SERIAL_HD(i) serial_hd(i)
#else
#define GF_SERIAL_HD(i) serial_hds[i]
#endif

/* ------------------------------------------------------------------ */
/* chardev: CharDriverState* (<=2.11) -> CharBackend (>=2.12)          */
/* ------------------------------------------------------------------ */
#if __has_include("chardev/char-fe.h")

#include "chardev/char.h"
#include "chardev/char-fe.h"

#define GF_CHR_FIELD                CharBackend chr
#define gf_chr_connected(s)         qemu_chr_fe_backend_connected(&(s)->chr)
#define gf_chr_write_all(s, b, l)   qemu_chr_fe_write_all(&(s)->chr, (b), (l))
#define gf_chr_accept_input(s)      qemu_chr_fe_accept_input(&(s)->chr)
#define gf_chr_set_handlers(s, can_read, read, ev, opaque)                 \
    qemu_chr_fe_set_handlers(&(s)->chr, (can_read), (read), (ev),          \
                             NULL, (opaque), NULL, true)
/* liga um chardev depois do realize (usado para pegar o serial padrao) */
#define gf_chr_init(s, c)           qemu_chr_fe_init(&(s)->chr, (c), &error_abort)

#else /* 2.9 .. 2.11 */

#include "sysemu/char.h"

/* o tipo passou a se chamar Chardev na 2.12; use sempre Chardev */
typedef CharDriverState Chardev;

#define GF_CHR_FIELD                CharDriverState *chr
#define gf_chr_connected(s)         ((s)->chr != NULL)
#define gf_chr_write_all(s, b, l)   qemu_chr_fe_write_all((s)->chr, (b), (l))
#define gf_chr_accept_input(s)      qemu_chr_accept_input((s)->chr)
#define gf_chr_set_handlers(s, can_read, read, ev, opaque)                 \
    qemu_chr_add_handlers((s)->chr, (can_read), (read), (ev), (opaque))
#define gf_chr_init(s, c)           do { (s)->chr = (c); } while (0)

#endif

/* ------------------------------------------------------------------ */
/* Parte so-da-placa.                                                  */
/*                                                                     */
/* Estes includes sao especificos do alvo ARM e nao podem aparecer em  */
/* codigo compilado de forma independente de alvo (hw/char, hw/display, */
/* hw/input, ...). O arquivo da placa define GF_BOARD antes de incluir  */
/* este header; os dispositivos nao.                                   */
/* ------------------------------------------------------------------ */
#ifdef GF_BOARD

#include "hw/boards.h"
#include "net/net.h"

/* hw/arm/arm.h (<=4.1) -> hw/arm/boot.h (>=4.1, unico a partir da 4.2) */
#if __has_include("hw/arm/boot.h")
#include "hw/arm/boot.h"
#else
#include "hw/arm/arm.h"
#endif

/*
 * arm_load_kernel ganhou o parametro MachineState na 4.2 (verificado nos
 * headers: 4.1 = (cpu, info), 4.2 = (cpu, ms, info)).
 */
#if GF_ATLEAST(4, 2)
#define GF_LOAD_KERNEL(cpu, ms, info) arm_load_kernel((cpu), (ms), (info))
#else
#define GF_LOAD_KERNEL(cpu, ms, info) arm_load_kernel((cpu), (info))
#endif

/* smc91c111_init: hw/devices.h (<=5.1) -> hw/net/smc91c111.h (>=5.2) */
#if __has_include("hw/net/smc91c111.h")
#include "hw/net/smc91c111.h"
#define GF_HAVE_SMC91C111 1
#elif __has_include("hw/devices.h")
#include "hw/devices.h"
#define GF_HAVE_SMC91C111 1
#endif

/*
 * RAM: memory_region_allocate_system_memory() foi removida na 5.2, quando
 * as placas passaram a receber a RAM pronta em machine->ram (habilitado
 * por mc->default_ram_id).
 */
#if GF_ATLEAST(5, 2)
#define GF_BOARD_RAM(ms) ((ms)->ram)
#else
static inline MemoryRegion *gf_board_ram(MachineState *ms)
{
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_allocate_system_memory(ram, NULL, "goldfish.ram",
                                         ms->ram_size);
    return ram;
}
#define GF_BOARD_RAM(ms) gf_board_ram(ms)
#endif

#endif /* GF_BOARD */

/* ------------------------------------------------------------------ */
/* DeviceClass::props -> device_class_set_props() na 5.1               */
/* ------------------------------------------------------------------ */
#if GF_ATLEAST(5, 1)
#define GF_SET_PROPS(dc, p) device_class_set_props((dc), (p))
#else
#define GF_SET_PROPS(dc, p) do { (dc)->props = (p); } while (0)
#endif

/* ------------------------------------------------------------------ */
/* realize: qdev_init_nofail() (<=5.2) -> qdev_realize() (>=6.0)        */
/* ------------------------------------------------------------------ */
#if GF_ATLEAST(6, 0)
#define GF_REALIZE(dev) qdev_realize(DEVICE(dev), NULL, &error_fatal)
#else
#define GF_REALIZE(dev) qdev_init_nofail(DEVICE(dev))
#endif

/*
 * Tipos QOM: usar sempre o par TYPE_xxx + OBJECT_CHECK, que existe em
 * todas as versoes. NAO usar OBJECT_DECLARE_SIMPLE_TYPE (5.0+) nem
 * DECLARE_INSTANCE_CHECKER (5.2+), que nao existem na 2.9/3.1/4.2.
 *
 *   #define TYPE_ANDROID_GOLDFISH_X "android-goldfish-x"
 *   #define ANDROID_GOLDFISH_X(obj) \
 *       OBJECT_CHECK(AndroidGoldfishXState, (obj), TYPE_ANDROID_GOLDFISH_X)
 */

/*
 * A assinatura de TypeInfo::class_init passou de (ObjectClass*, void*) para
 * (ObjectClass*, const void*) no QEMU 10. Use sempre GF_CLASS_INIT_DATA.
 */
#if GF_ATLEAST(10, 0)
#define GF_CLASS_INIT_DATA const void
#else
#define GF_CLASS_INIT_DATA void
#endif

/* leitura/escrita da memoria do convidado (fb, pdev_bus) */
#include "exec/cpu-common.h"
#define gf_guest_read(addr, buf, len)  cpu_physical_memory_read((addr), (buf), (len))
#define gf_guest_write(addr, buf, len) cpu_physical_memory_write((addr), (buf), (len))

#endif /* HW_GOLDFISH_COMPAT_H */
