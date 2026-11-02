#ifndef GESTURES_H
#define GESTURES_H

#include <Arduino.h>
#include "config.h"

struct GesturePose {
    const char* name;
    float angles[NUM_SERVOS]; // Thumb, Index, Middle, Ring, Pinky, Wrist
};

// Hand and wrist poses (Fingers: 0 = flat open, 180 = full fist. Wrist: 90 = neutral straight)
const GesturePose PRESET_POSES[] = {
    { "OPEN",         {   0,   0,   0,   0,   0,  90 } },
    { "FIST",         { 180, 180, 180, 180, 180,  90 } },
    { "POINT",        { 180,   0, 180, 180, 180,  90 } },
    { "PEACE",        { 180,   0,   0, 180, 180,  90 } },
    { "THUMBS_UP",    {   0, 180, 180, 180, 180,  90 } },
    { "HORNS",        { 180,   0, 180, 180,   0,  90 } },
    { "ROCK_ON",      {   0,   0, 180, 180,   0,  90 } },
    { "THREE_FINGER", {   0,   0,   0, 180, 180,  90 } },
    { "PINCH",        {  90,  90,   0,   0,   0,  90 } },
    { "OK_SIGN",      {  80,  80,   0,   0,   0,  90 } },
    { "WRIST_LEFT",   {   0,   0,   0,   0,   0,   0 } },
    { "WRIST_CENTER", {   0,   0,   0,   0,   0,  90 } },
    { "WRIST_RIGHT",  {   0,   0,   0,   0,   0, 180 } }
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
