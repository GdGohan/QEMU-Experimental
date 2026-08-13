/*
 * Bateria goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_MISC_ANDROID_GOLDFISH_BATTERY_H
#define HW_MISC_ANDROID_GOLDFISH_BATTERY_H

#define TYPE_ANDROID_GOLDFISH_BATTERY "android-goldfish-battery"
#define ANDROID_GOLDFISH_BATTERY(obj) \
    OBJECT_CHECK(AndroidGoldfishBatteryState, (obj), \
                 TYPE_ANDROID_GOLDFISH_BATTERY)

typedef struct AndroidGoldfishBatteryState AndroidGoldfishBatteryState;

#endif /* HW_MISC_ANDROID_GOLDFISH_BATTERY_H */
