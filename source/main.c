#include "gba.h"
#include "input.h"
#include "game.h"

typedef enum {
    SCREEN_TITLE,
    SCREEN_PLAY,
    SCREEN_GAMEOVER,
} Screen;

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    Screen   screen      = SCREEN_TITLE;
    int      title_frame = 0;
    int      go_frame    = 0;
    u32      run_seed    = 0;
    int      high_score  = 0;
    int      theme_idx   = 0;
    GameMode sel_mode    = MODE_DRONE;
    Game     g;

    game_set_theme(theme_idx);

    // Pre-generate level 0 so first blit has something to show
    run_seed = 42;
    g.score = 0;
    g.timer_frames = INITIAL_TIMER;
    g.mode = sel_mode;
    game_generate(&g, 0, run_seed);

    while (1) {
        // ── Draw ─────────────────────────────────────────────────────────────
        switch (screen) {
            case SCREEN_TITLE:
                game_draw_title(title_frame, theme_idx, sel_mode);
                break;
            case SCREEN_PLAY:
                game_draw(&g);
                break;
            case SCREEN_GAMEOVER:
                game_draw_gameover(&g, high_score, go_frame);
                break;
        }

        // ── Blit during vblank (no tearing) ──────────────────────────────────
        vsync();
        game_blit();

        // ── Input + logic ─────────────────────────────────────────────────────
        input_update();

        switch (screen) {
            case SCREEN_TITLE:
                title_frame++;
                if (game_update_title(&theme_idx, &sel_mode)) {
                    run_seed = (u32)title_frame;
                    g.score        = 0;
                    g.timer_frames = INITIAL_TIMER;
                    g.mode         = sel_mode;
                    game_generate(&g, 0, run_seed);
                    screen = SCREEN_PLAY;
                }
                break;

            case SCREEN_PLAY:
                // Tick timer only while unsolved
                if (!g.solved) {
                    g.timer_frames--;
                    if (g.timer_frames <= 0) {
                        g.timer_frames = 0;
                        if (g.score > high_score) high_score = g.score;
                        go_frame = 0;
                        screen   = SCREEN_GAMEOVER;
                        break;
                    }
                }

                game_update(&g);

                // Auto-advance to next level after a short celebration delay
                if (g.solved && g.win_timer > 90) {
                    g.timer_frames += g.time_bonus;
                    g.score++;
                    game_generate(&g, g.score, run_seed);
                    // timer_frames and score are preserved by game_generate
                }

                if (key_pressed(KEY_SELECT)) {
                    title_frame = 0;
                    screen      = SCREEN_TITLE;
                }
                break;

            case SCREEN_GAMEOVER:
                go_frame++;
                if (go_frame > 60 && key_pressed(KEY_START)) {
                    title_frame = 0;
                    screen      = SCREEN_TITLE;
                }
                break;
        }
    }

    return 0;
}
