#!/bin/sh
# Testa o ci/goldfish-integrate.sh gerado pelo workflow contra arvores QEMU
# simuladas das tres eras de build system, usando o manifest.tsv REAL.
#
#   eraA = 2.9.1  -> so Makefile.objs + default-configs/arm-softmmu.mak
#   eraB = 5.1.0  -> Makefile.objs + Kconfig + configs/devices/.../default.mak
#   eraC = 8.2/9.x -> meson.build + Kconfig
#
# Roda duas vezes por era para provar idempotencia.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/tools/.itest"
# o workflow pode estar na raiz (rascunho) ou no lugar definitivo
if [ -f "$ROOT/.github/workflows/build-qemu-goldfish.yml" ]; then
    YML="$ROOT/.github/workflows/build-qemu-goldfish.yml"
else
    YML="$ROOT/build-qemu-goldfish.yml"
fi

rm -rf "$WORK"
mkdir -p "$WORK"

# extrai o script de integracao de dentro do heredoc do workflow
python3 - "$YML" "$WORK/goldfish-integrate.sh" <<'PY'
import re, sys
yml, out = sys.argv[1], sys.argv[2]
lines = open(yml).read().split('\n')
start = end = None
for i, l in enumerate(lines):
    if "cat > ci/goldfish-integrate.sh" in l:
        start = i + 1
    elif start is not None and l.strip() == "INTEGRATE":
        end = i
        break
if start is None or end is None:
    sys.exit("nao achei o heredoc do goldfish-integrate.sh no workflow")
body = lines[start:end]
indent = min(len(l) - len(l.lstrip()) for l in body if l.strip())
open(out, 'w').write('\n'.join(l[indent:] for l in body) + '\n')
print("script extraido: %d linhas" % len(body))
PY

make_tree() {
    era="$1"; ver="$2"; dir="$WORK/$era"
    mkdir -p "$dir/hw/arm" "$dir/hw/intc" "$dir/hw/char" "$dir/hw/timer" \
             "$dir/hw/display" "$dir/hw/input" "$dir/hw/misc" "$dir/include"
    echo "$ver" > "$dir/VERSION"
    case "$era" in
    eraA)
        for d in arm intc char timer display input misc; do
            printf 'obj-y += existing.o\n' > "$dir/hw/$d/Makefile.objs"
        done
        mkdir -p "$dir/default-configs"
        printf 'CONFIG_ARM_VIRT=y\n' > "$dir/default-configs/arm-softmmu.mak"
        ;;
    eraB)
        for d in arm intc char timer display input misc; do
            printf 'obj-y += existing.o\n' > "$dir/hw/$d/Makefile.objs"
            printf 'config EXISTING\n    bool\n' > "$dir/hw/$d/Kconfig"
        done
        mkdir -p "$dir/configs/devices/arm-softmmu"
        printf 'CONFIG_ARM_VIRT=y\n' > "$dir/configs/devices/arm-softmmu/default.mak"
        ;;
    eraC)
        printf "arm_ss.add(when: 'CONFIG_EXISTING', if_true: files('existing.c'))\n" \
            > "$dir/hw/arm/meson.build"
        for d in intc char timer display input misc; do
            printf "system_ss.add(when: 'CONFIG_EXISTING', if_true: files('existing.c'))\n" \
                > "$dir/hw/$d/meson.build"
        done
        for d in arm intc char timer display input misc; do
            printf 'config EXISTING\n    bool\n' > "$dir/hw/$d/Kconfig"
        done
        mkdir -p "$dir/configs/devices/arm-softmmu"
        printf 'CONFIG_ARM_VIRT=y\n' > "$dir/configs/devices/arm-softmmu/default.mak"
        ;;
    esac
}

FAILED=0
for spec in "eraA 2.9.1" "eraB 5.1.0" "eraC 8.2.0"; do
    era="${spec% *}"; ver="${spec#* }"
    make_tree "$era" "$ver"
    for pass in 1 2; do
        echo "=================== $era (VERSION $ver) passada $pass"
        if ! sh "$WORK/goldfish-integrate.sh" "$WORK/$era" \
                "$ROOT/goldfish-patches" > "$WORK/$era.log" 2>&1; then
            echo "FALHOU -- log:"
            sed 's/^/    /' "$WORK/$era.log"
            FAILED=1
            break
        fi
    done
    [ "$FAILED" = 0 ] || continue

    d="$WORK/$era"
    echo "--- gf-version.h:"
    sed 's/^/    /' "$d/include/hw/goldfish/gf-version.h"
    echo "--- arquivos copiados: $(find "$d/hw" "$d/include/hw" -name 'android_goldfish*' | wc -l)"
    echo "--- entradas de build para a placa:"
    grep -rn "android_goldfish" "$d/hw/arm/meson.build" "$d/hw/arm/Makefile.objs" \
        2>/dev/null | sed 's/^/    /' || true
    echo "--- Kconfig agregador:"
    sed -n '/config ANDROID_GOLDFISH$/,/^$/p' "$d/hw/arm/Kconfig" 2>/dev/null |
        sed 's/^/    /' || echo "    (era sem Kconfig)"
    echo "--- defconfig:"
    grep -h "GOLDFISH\|SMC91C111" "$d/default-configs/arm-softmmu.mak" \
        "$d/configs/devices/arm-softmmu/default.mak" 2>/dev/null |
        sed 's/^/    /' || true
    echo "--- linhas duplicadas (idempotencia):"
    # so os arquivos do build system -- os .c copiados obviamente repetem
    # nomes internamente e nao interessam aqui
    dup=$(find "$d" \( -name 'Makefile.objs' -o -name 'meson.build' \
                     -o -name 'Kconfig' -o -name '*.mak' \) -exec cat {} + \
          2>/dev/null | grep -i "goldfish\|SMC91C111" |
          sort | uniq -d || true)
    if [ -n "$dup" ]; then
        echo "$dup" | sed 's/^/    DUPLICADA: /'
        FAILED=1
    else
        echo "    nenhuma"
    fi
done

echo
if [ "$FAILED" = 0 ]; then
    echo "INTEGRACAO OK NAS TRES ERAS (e idempotente)"
else
    echo "HOUVE FALHAS"
    exit 1
fi
