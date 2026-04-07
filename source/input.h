#pragma once
#include "gba.h"

// Call once per frame at the top of your game loop.
void input_update(void);

// True every frame the key is held.
int key_held(u16 key);

// True only on the first frame the key is pressed (not held).
int key_pressed(u16 key);
