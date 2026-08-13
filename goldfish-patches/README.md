# goldfish-patches

Placa `goldfish` (Android Emulator) portada para QEMU **2.9.1 a 9.x**, incluindo
o fork **qemu-wasm** (que reporta `VERSION` 8.2.0).

Alvo: kernels goldfish do **Android 2.3.6 (Gingerbread)** ate **4.4 (KitKat)**.

## Como usar

Os arquivos aqui nao vao direto na arvore do QEMU: quem os instala e
`ci/goldfish-integrate.sh`, gerado pelo workflow
`.github/workflows/build-qemu-goldfish.yml`. Ele le o `manifest.tsv`, copia cada
arquivo para o caminho de destino e registra o simbolo `CONFIG_` no build system
da era detectada.

```
ci/goldfish-integrate.sh <arvore-do-qemu> <este-diretorio> [overlay]
```

## Layout

```
manifest.tsv        o que copiar, para onde, e com qual simbolo CONFIG_
extra-selects.txt   simbolos Kconfig de terceiros que a placa precisa
CONTRATO.md         regras de portabilidade que todo arquivo aqui segue
base/               os fontes (valem para todas as versoes)
qemu-2.9/           overlay opcional: substitui arquivos de base/ nessa era
qemu-3.1/  qemu-4.2/  qemu-5.1/  qemu-modern/
```

O overlay tem prioridade sobre `base/`. Hoje todos os overlays estao vazios: um
unico conjunto de fontes compila nas cinco eras. Se algum dia uma versao exigir
codigo realmente divergente, coloque o arquivo inteiro no overlay dela em vez de
encher o `base/` de `#if`.

Correcoes pontuais de compilacao que nao valem um arquivo inteiro vao como
patch em `qemu-<era>/diffs/*.patch`.

## Como a compatibilidade funciona

Duas tecnicas, nesta ordem de preferencia:

1. **Header que mudou de lugar** -> `__has_include`, sem depender de versao.
   Ex.: `chardev/char-fe.h` (>=2.12) vs `sysemu/char.h` (<=2.11).
2. **Assinatura de API que mudou** -> `GF_ATLEAST(maj, min)` em
   `include/hw/goldfish/compat.h`.

A versao **nao e adivinhada**. O script de integracao le o arquivo `VERSION` da
raiz da arvore e gera `include/hw/goldfish/gf-version.h`. Se esse header faltar,
o `compat.h` para o build com `#error` de proposito: assumir "versao mais nova"
para um fork daria bug silencioso -- o qemu-wasm diz 8.2.0, e coisas como o
`class_init` com `const void *` so existem no QEMU 10.

Mudancas de API ja tratadas:

| mudanca | versao |
|---|---|
| `sysemu/char.h` -> `chardev/char-fe.h`, `CharDriverState` -> `Chardev` | 2.12 |
| `cpu_generic_init()` -> `object_new(machine->cpu_type)` | 2.12 |
| `serial_hds[]` -> `serial_hd()` | 4.0 |
| `hw/arm/arm.h` -> `hw/arm/boot.h` | 4.1 |
| `arm_load_kernel()` ganha parametro `MachineState *` | 4.2 |
| `dc->props` -> `device_class_set_props()` | 5.1 |
| `memory_region_allocate_system_memory()` -> `machine->ram` + `default_ram_id` | 5.2 |
| `smc91c111_init()` sai de `hw/devices.h` para `hw/net/smc91c111.h` | 5.2 |
| `qdev_init_nofail()` -> `qdev_realize()` | 6.0 |
| sourceset `softmmu_ss` -> `system_ss` | 8.x |
| `class_init` recebe `const void *` | 10.0 |

## Por que os nomes tem prefixo `android_`

O QEMU upstream **ja tem** dispositivos goldfish para a maquina `virt` do m68k:
`goldfish_rtc` (desde a 5.1) e `goldfish_pic`/`goldfish_tty` (desde a 5.2) --
o mesmo hardware, drivers diferentes. Usar os nomes sem prefixo causaria colisao
de tipo QOM e de simbolo `CONFIG_` nas versoes novas, obrigando a ter codigo
diferente por era. Com `android_goldfish_*` / `android-goldfish-*` /
`CONFIG_ANDROID_GOLDFISH_*`, o mesmo codigo compila nas cinco.

O nome da **maquina** continua `goldfish` (`-M goldfish`), e os nomes que o
kernel enxerga pelo `pdev_bus` continuam sendo os originais (`goldfish_fb`,
`goldfish_tty`, ...) -- esses tem que bater exatamente com o driver do kernel.

## Mapa de hardware

De `arch/arm/mach-goldfish/include/mach/hardware.h` e `irqs.h` do kernel
goldfish (identicos entre goldfish 2.6.29 e 3.4):

| endereco | dispositivo | IRQ |
|---|---|---|
| 0xff000000 | interrupt controller | -> linha IRQ da CPU |
| 0xff001000 | pdev_bus | 1 |
| 0xff002000 | tty | 4 |
| 0xff003000 | timer | 3 |
| 0xff010000+ (passo 0x1000) | fb, events, battery, smc91x | 5+ |

RAM em 0x00000000, boot por ATAGS (goldfish nao usa device tree),
`MACH_TYPE_GOLDFISH` = 1441.

Fora do PIC, do tty e do timer, o kernel descobre o hardware **enumerando o
pdev_bus** -- por isso cada dispositivo dinamico e registrado com
`android_goldfish_pdev_bus_add_device()` usando o nome exato que o driver espera.

## Verificacao

`tools/check-syntax.sh` monta uma arvore de headers-stub por era (2.9, 3.1, 4.2,
5.1, wasm 8.2, 9.1, 10.0), aplica o `manifest.tsv` e roda `gcc -fsyntax-only`
com `-Werror` em cada `.c`. Sao 8 arquivos x 7 eras. Isso **nao** substitui um
build de verdade -- e o que o workflow faz -- mas pega erro de digitacao, tipo
errado e uso de API inexistente na era em segundos.

`tools/test-integrate.sh` roda o script de integracao contra arvores QEMU
simuladas das tres eras de build system, duas vezes cada, e confere que o
resultado esta correto e que nao duplica linhas.

## Limitacoes conhecidas

- **Sem entrada `goldfish_rtc` no pdev_bus.** Registrar `goldfish_rtc` como um
  segundo apelido do mesmo MMIO faria o driver de timer e o de rtc disputarem os
  registradores de ALARM. O Android sobe sem ele.
- **fb e events ficam nos tamanhos padrao (320x480).** A placa cria os
  dispositivos com `sysbus_create_simple()`, que realiza o dispositivo na hora e
  portanto nao permite setar propriedade antes do realize. Foi uma escolha
  consciente: e a unica API de create+realize+map+irq que existe igual da 2.9 a
  9.x (`qdev_create`/`qdev_init_nofail` sairam na 6.0, e
  `qdev_realize(dev, NULL, errp)` dispara assert em `SysBusDevice`).
  Por isso tambem o tty pega o chardev padrao dentro do proprio `realize`.
- **Sem handler de reset**, de proposito: `dc->reset` foi removido na 9.1. Todo
  o estado e inicializado no `realize`.
- **Rotacao do framebuffer e aceita mas ignorada** na renderizacao.
- **Faltam dispositivos** para um Android completo: audio, mmc, nand, switch e
  o `goldfish_pipe` (que o Android 4.x usa para varias coisas). Espere um boot
  ate a UI com limitacoes, nao um emulador completo.
- **2.9.1 a 5.1.0 compilam apenas nativo.** O backend TCG para wasm existe so
  no fork moderno do ktock; nao ha como gerar wasm a partir dessas versoes.
