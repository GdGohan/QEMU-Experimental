/*
 * Framebuffer goldfish para Android.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_DISPLAY_ANDROID_GOLDFISH_FB_H
#define HW_DISPLAY_ANDROID_GOLDFISH_FB_H

#define TYPE_ANDROID_GOLDFISH_FB "android-goldfish-fb"
#define ANDROID_GOLDFISH_FB(obj) \
    OBJECT_CHECK(AndroidGoldfishFbState, (obj), TYPE_ANDROID_GOLDFISH_FB)

typedef struct AndroidGoldfishFbState AndroidGoldfishFbState;

#endif /* HW_DISPLAY_ANDROID_GOLDFISH_FB_H */
