#include "input.h"

static u16 prev = 0;
static u16 curr = 0;

void input_update(void) {
    prev = curr;
    // REG_KEYINPUT is active-low: bit=0 means pressed.
    // Invert so bit=1 means pressed, and mask to 10 valid bits.
    curr = (~REG_KEYINPUT) & 0x03FF;
}

int key_held(u16 key) {
    return (curr & key) != 0;
}

int key_pressed(u16 key) {
    // Set this frame but NOT last frame
    return ((curr & ~prev) & key) != 0;
}
