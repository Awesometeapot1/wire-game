#pragma once
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

// ── Screen ───────────────────────────────────────────────────────────────────
#define SCREEN_W    240
#define SCREEN_H    160
#define VRAM        ((u16 *)0x06000000)

// ── Registers ────────────────────────────────────────────────────────────────
#define REG_DISPCNT  (*(volatile u16 *)0x04000000)
#define REG_VCOUNT   (*(volatile u16 *)0x04000006)
#define REG_KEYINPUT (*(volatile u16 *)0x04000130)

// Display control flags
#define DCNT_MODE3  0x0003
#define DCNT_BG2    0x0400

// ── Keys ─────────────────────────────────────────────────────────────────────
#define KEY_A       0x0001
#define KEY_B       0x0002
#define KEY_SELECT  0x0004
#define KEY_START   0x0008
#define KEY_RIGHT   0x0010
#define KEY_LEFT    0x0020
#define KEY_UP      0x0040
#define KEY_DOWN    0x0080
#define KEY_R       0x0100
#define KEY_L       0x0200

// ── Color ────────────────────────────────────────────────────────────────────
// 15-bit BGR: bbbbbgggggrrrrr
#define RGB15(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

// ── DMA3 (used for fast backbuffer blit) ─────────────────────────────────────
#define REG_DMA3SAD   (*(volatile u32 *)0x040000D4)
#define REG_DMA3DAD   (*(volatile u32 *)0x040000D8)
#define REG_DMA3CNT_L (*(volatile u16 *)0x040000DC)
#define REG_DMA3CNT_H (*(volatile u16 *)0x040000DE)
#define DMA_ENABLE    (1 << 15)
#define DMA_32BIT     (1 << 10)

// ── VSync ────────────────────────────────────────────────────────────────────
static inline void vsync(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}
