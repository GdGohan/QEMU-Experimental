/*
 * Goldfish platform device bus (Android Emulator)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Espelha o driver drivers/platform/goldfish/pdev_bus.c do kernel Linux.
 * E por aqui que o kernel do Android descobre TODOS os dispositivos
 * goldfish (fb, events, battery, tty, ...): a placa registra cada um com
 * android_goldfish_pdev_bus_add_device() e o kernel enumera a lista.
 */
#ifndef HW_ARM_ANDROID_GOLDFISH_PDEV_BUS_H
#define HW_ARM_ANDROID_GOLDFISH_PDEV_BUS_H

#define TYPE_ANDROID_GOLDFISH_PDEV_BUS "android-goldfish-pdev-bus"

#define ANDROID_GOLDFISH_PDEV_BUS_MAX_DEVS 32
#define ANDROID_GOLDFISH_PDEV_BUS_NAME_MAX 32

/*
 * Registra um dispositivo na lista enumerada pelo kernel.
 * 'dev' e o DeviceState do pdev_bus (retornado por sysbus_create_simple).
 * Deve ser chamado pela placa, depois de criar o barramento e antes do
 * convidado comecar a rodar.
 */
void android_goldfish_pdev_bus_add_device(DeviceState *dev,
                                          const char *name,
                                          uint32_t io_base,
                                          uint32_t io_size,
                                          uint32_t irq,
                                          uint32_t irq_count);

#endif /* HW_ARM_ANDROID_GOLDFISH_PDEV_BUS_H */
