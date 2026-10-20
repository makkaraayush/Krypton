#ifndef GESTURES_H
#define GESTURES_H

#include <Arduino.h>

struct GesturePose {
    const char* name;
    float angles[5]; // Thumb, Index, Middle, Ring, Pinky
};

// Hand poses (0 = flat open, 180 = full contracted fist)
const GesturePose PRESET_POSES[] = {
    { "OPEN",      {   0,   0,   0,   0,   0 } },
    { "FIST",      { 180, 180, 180, 180, 180 } },
    { "POINT",     { 180,   0, 180, 180, 180 } },
    { "PEACE",     { 180,   0,   0, 180, 180 } },
    { "THUMBS_UP", {   0, 180, 180, 180, 180 } },
    { "HORNS",     { 180,   0, 180, 180,   0 } },
    { "PINCH",     {  90,  90,   0,   0,   0 } },
    { "OK_SIGN",   {  80,  80,   0,   0,   0 } }
};

const int NUM_PRESET_POSES = sizeof(PRESET_POSES) / sizeof(PRESET_POSES[0]);

inline const GesturePose* findGesture(const String& name) {
    String upper = name;
    upper.toUpperCase();
    for (int i = 0; i < NUM_PRESET_POSES; i++) {
        if (upper == PRESET_POSES[i].name) {
            return &PRESET_POSES[i];
        }
    }
    return nullptr;
}

#endif // GESTURES_H
