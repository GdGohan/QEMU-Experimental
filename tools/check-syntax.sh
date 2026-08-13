#!/bin/sh
# Checagem de sintaxe/tipos dos arquivos goldfish FORA da arvore do QEMU.
#
# Monta uma arvore de headers-stub por era, gera o mesmo
# include/hw/goldfish/gf-version.h que o script de integracao gera, aplica o
# manifest.tsv nos caminhos reais de destino e roda gcc -fsyntax-only em cada
# .c. Nao substitui um build de verdade (isso e o que o workflow faz), mas
# pega erro de digitacao, tipo errado, macro faltando e uso de API que nao
# existe na era em questao.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCHES="$ROOT/goldfish-patches"
STUBS="$ROOT/tools/stubs"
OUT="$ROOT/tools/.check"
FAILED=0

# eras: <rotulo>:<major>:<minor>
ERAS="qemu-2.9:2:9 qemu-3.1:3:1 qemu-4.2:4:2 qemu-5.1:5:1 qemu-wasm-8.2:8:2 qemu-9.1:9:1 qemu-10.0:10:0"

for era in $ERAS; do
    label="${era%%:*}"
    rest="${era#*:}"
    vmaj="${rest%%:*}"
    vmin="${rest#*:}"
    tree="$OUT/$label"

    rm -rf "$tree"
    mkdir -p "$tree/hw/goldfish"

    cp "$STUBS/qemu/osdep.h" "$tree/qemu_osdep_src.h" 2>/dev/null || true
    mkdir -p "$tree/qemu"
    cp "$STUBS/qemu/osdep.h" "$tree/qemu/osdep.h"
    cp "$STUBS/gf_stub_api.h" "$tree/gf_stub_api.h"

    # o mesmo header que ci/goldfish-integrate.sh gera
    mkdir -p "$tree/include/hw/goldfish"
    {
        echo '#ifndef HW_GOLDFISH_GF_VERSION_H'
        echo '#define HW_GOLDFISH_GF_VERSION_H'
        echo "#define GF_VER_MAJOR $vmaj"
        echo "#define GF_VER_MINOR $vmin"
        echo '#endif'
    } > "$tree/include/hw/goldfish/gf-version.h"

    # headers que apenas redirecionam para o stub grande
    for h in qemu/module.h qemu/log.h qemu/timer.h qemu/cutils.h \
             qemu/error-report.h qemu/bswap.h qemu/units.h qapi/error.h \
             migration/vmstate.h hw/sysbus.h hw/irq.h hw/qdev-properties.h \
             hw/qdev-core.h hw/boards.h exec/address-spaces.h \
             exec/cpu-common.h exec/memory.h ui/console.h ui/input.h \
             net/net.h hw/net/smc91c111.h cpu.h; do
        mkdir -p "$tree/$(dirname "$h")"
        printf '#include "gf_stub_api.h"\n' > "$tree/$h"
    done

    # headers que existem so em certas eras -- e assim que o compat.h decide
    # o caminho via __has_include, entao a arvore precisa refletir a era.
    if [ "$vmaj" -eq 2 ] && [ "$vmin" -lt 12 ]; then
        mkdir -p "$tree/sysemu"
        printf '#include "gf_stub_api.h"\n' > "$tree/sysemu/char.h"
    else
        mkdir -p "$tree/chardev"
        printf '#include "gf_stub_api.h"\n' > "$tree/chardev/char.h"
        printf '#include "gf_stub_api.h"\n' > "$tree/chardev/char-fe.h"
    fi

    # hw/arm/arm.h virou hw/arm/boot.h na 4.1
    mkdir -p "$tree/hw/arm"
    if [ "$vmaj" -ge 5 ] || { [ "$vmaj" -eq 4 ] && [ "$vmin" -ge 1 ]; }; then
        printf '#include "gf_stub_api.h"\n' > "$tree/hw/arm/boot.h"
    else
        printf '#include "gf_stub_api.h"\n' > "$tree/hw/arm/arm.h"
    fi

    # smc91c111_init saiu de hw/devices.h para hw/net/smc91c111.h na 5.2
    if [ "$vmaj" -ge 6 ] || { [ "$vmaj" -eq 5 ] && [ "$vmin" -ge 2 ]; }; then
        : # hw/net/smc91c111.h ja criado acima
    else
        rm -f "$tree/hw/net/smc91c111.h"
        printf '#include "gf_stub_api.h"\n' > "$tree/hw/devices.h"
    fi

    # sysemu/sysemu.h existe em todas as eras alvo
    mkdir -p "$tree/sysemu"
    printf '#include "gf_stub_api.h"\n' > "$tree/sysemu/sysemu.h"

    # aplica o manifest nos caminhos reais de destino
    while IFS="$(printf '\t')" read -r src dst sym; do
        case "$src" in ""|\#*) continue ;; esac
        mkdir -p "$tree/$(dirname "$dst")"
        cp "$PATCHES/base/$src" "$tree/$dst"
    done < "$PATCHES/manifest.tsv"

    echo "=== $label (QEMU $vmaj.$vmin)"
    for c in "$tree"/hw/*/*.c; do
        if gcc -fsyntax-only -std=gnu99 -Wall -Werror \
               -Wno-unused-parameter -Wno-unused-variable \
               -Wno-unused-function -Wno-unused-but-set-variable \
               -I "$tree/include" -I "$tree" "$c" 2> "$OUT/err.txt"; then
            echo "  ok      $(basename "$c")"
        else
            echo "  FALHOU  $(basename "$c")"
            sed 's/^/          /' "$OUT/err.txt"
            FAILED=1
        fi
    done
done

if [ "$FAILED" = 0 ]; then
    echo
    echo "TODOS OS ARQUIVOS PASSARAM EM TODAS AS ERAS"
else
    echo
    echo "HOUVE FALHAS"
    exit 1
fi
