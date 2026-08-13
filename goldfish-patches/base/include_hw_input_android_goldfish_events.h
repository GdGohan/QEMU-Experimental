/*
 * Dispositivo de eventos goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_INPUT_ANDROID_GOLDFISH_EVENTS_H
#define HW_INPUT_ANDROID_GOLDFISH_EVENTS_H

#define TYPE_ANDROID_GOLDFISH_EVENTS "android-goldfish-events"
#define ANDROID_GOLDFISH_EVENTS(obj) \
    OBJECT_CHECK(AndroidGoldfishEventsState, (obj), \
                 TYPE_ANDROID_GOLDFISH_EVENTS)

typedef struct AndroidGoldfishEventsState AndroidGoldfishEventsState;

#endif /* HW_INPUT_ANDROID_GOLDFISH_EVENTS_H */
