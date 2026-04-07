#pragma once
#include "gba.h"

// ── Grid ─────────────────────────────────────────────────────────────────────
#define GRID_W       8
#define GRID_H       5
#define CELL_W       30
#define CELL_H       28
#define DIFF_TIERS   6    // number of difficulty tiers (levels 0-2=easy, etc.)

// Timer constants
#define INITIAL_TIMER_SEC  15
#define INITIAL_TIMER      (INITIAL_TIMER_SEC * 60)  // in frames
#define TIME_BONUS_PER_SEC 2 * 60                    // frames added per second remaining

// ── Themes ────────────────────────────────────────────────────────────────────
#define THEME_COUNT 4
void game_set_theme(int idx);

// ── Game mode ─────────────────────────────────────────────────────────────────
typedef enum {
    MODE_DRONE = 0,   // standard
    MODE_GHOST,       // unpowered tiles >2 away from cursor are hidden
    MODE_FRENZY,      // a random tile auto-rotates every 3 seconds
    MODE_COUNT
} GameMode;

// ── Wire connection flags ─────────────────────────────────────────────────────
#define CONN_N  0x1
#define CONN_S  0x2
#define CONN_E  0x4
#define CONN_W  0x8

// ── Wire piece types ──────────────────────────────────────────────────────────
typedef enum {
    WIRE_NONE = 0,
    WIRE_H,       // ─   E+W
    WIRE_V,       // │   N+S
    WIRE_NE,      // └   N+E
    WIRE_NW,      // ┘   N+W
    WIRE_SE,      // ┌   S+E
    WIRE_SW,      // ┐   S+W
    WIRE_T_NSE,   // ├   N+S+E
    WIRE_T_NSW,   // ┤   N+S+W
    WIRE_T_SEW,   // ┬   S+E+W
    WIRE_T_NEW,   // ┴   N+E+W
    WIRE_CROSS,   // +   all four
    WIRE_COUNT
} WireType;

// ── Grid cell ─────────────────────────────────────────────────────────────────
typedef struct {
    WireType type;
    u8 fixed;
    u8 powered;
    u8 rot_left;  // 0xFF = unlimited, 0 = exhausted (locked), 1/2 = remaining rotations
} Cell;

// ── Game state ────────────────────────────────────────────────────────────────
typedef struct {
    Cell grid[GRID_H][GRID_W];
    int  cx, cy;
    int  sx, sy;
    int  rx, ry;
    int  solved;
    int  level_num;     // 0-based, increases forever
    int  win_timer;     // frames since this level was solved
    u32  run_seed;      // seed for this run (for sharing)
    // ── Managed by main, preserved across game_generate ──────────────────
    int      timer_frames;  // countdown timer
    int      score;         // levels beaten so far
    int      time_bonus;    // frames added on beating this level (set by game_generate)
    GameMode mode;          // selected game mode
    int      frenzy_timer;  // frames until next frenzy rotation
} Game;

// Generate a random level. level_num drives difficulty (0-2=easy, 3-5=medium…)
// and the per-level seed. Does NOT reset timer_frames or score.
void game_generate(Game *g, int level_num, u32 run_seed);

void game_blit(void);
void game_update(Game *g);
void game_draw(const Game *g);
void game_draw_gameover(const Game *g, int high_score, int frame);
void game_draw_title(int frame, int theme_idx, GameMode mode);
int  game_update_title(int *theme_idx, GameMode *mode);  // returns 1 when START pressed
