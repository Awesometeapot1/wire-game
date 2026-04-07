#include "game.h"
#include "input.h"

// ── Backbuffer ────────────────────────────────────────────────────────────────
static u16 backbuf[SCREEN_H * SCREEN_W] __attribute__((section(".ewram")));

void game_blit(void) {
    REG_DMA3SAD   = (u32)backbuf;
    REG_DMA3DAD   = (u32)VRAM;
    REG_DMA3CNT_L = SCREEN_W * SCREEN_H / 2;
    REG_DMA3CNT_H = DMA_ENABLE | DMA_32BIT;
}

// ── Wire tables ───────────────────────────────────────────────────────────────

static const u8 wire_conns[WIRE_COUNT] = {
    [WIRE_NONE]  = 0,
    [WIRE_H]     = CONN_E|CONN_W,
    [WIRE_V]     = CONN_N|CONN_S,
    [WIRE_NE]    = CONN_N|CONN_E,
    [WIRE_NW]    = CONN_N|CONN_W,
    [WIRE_SE]    = CONN_S|CONN_E,
    [WIRE_SW]    = CONN_S|CONN_W,
    [WIRE_T_NSE] = CONN_N|CONN_S|CONN_E,
    [WIRE_T_NSW] = CONN_N|CONN_S|CONN_W,
    [WIRE_T_SEW] = CONN_S|CONN_E|CONN_W,
    [WIRE_T_NEW] = CONN_N|CONN_E|CONN_W,
    [WIRE_CROSS] = CONN_N|CONN_S|CONN_E|CONN_W,
};

static WireType rotate_cw(WireType t) {
    switch (t) {
        case WIRE_H:     return WIRE_V;
        case WIRE_V:     return WIRE_H;
        case WIRE_NE:    return WIRE_SE;
        case WIRE_SE:    return WIRE_SW;
        case WIRE_SW:    return WIRE_NW;
        case WIRE_NW:    return WIRE_NE;
        case WIRE_T_NSE: return WIRE_T_SEW;
        case WIRE_T_SEW: return WIRE_T_NSW;
        case WIRE_T_NSW: return WIRE_T_NEW;
        case WIRE_T_NEW: return WIRE_T_NSE;
        default:         return t;
    }
}

// ── RNG (xorshift32) ─────────────────────────────────────────────────────────

static u32 rng_state;
static void rng_seed(u32 s)  { rng_state = s ? s : 0xDEADBEEFu; }
static u32  rng_next(void)   {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static int rng_range(int lo, int hi) {
    return lo + (int)(rng_next() % (u32)(hi - lo + 1));
}

// ── Theme system ─────────────────────────────────────────────────────────────
typedef struct {
    u16 bg, grid_line;
    u16 wire_off, fixed_col;
    u16 t_piece, cross_col;
    u16 cursor_bg, cursor;
    u16 source, robot_off, robot_on;
    u16 status_bg, text, title;
    u16 spark, wire_on;
    u16 timer_hi, timer_mid, timer_lo;
    u16 rot_limit;
    const char *name;
} Theme;

static const Theme theme_presets[THEME_COUNT] = {
    {   // 0 — NAVY (original)
        RGB15( 2,  3,  7), RGB15( 5,  6, 10),
        RGB15( 7,  9, 15), RGB15(11, 13, 19),
        RGB15(18, 14, 20), RGB15(18, 14, 20),
        RGB15( 6,  7, 12), RGB15(20, 22, 28),
        RGB15(26, 22, 18), RGB15( 7,  8, 14), RGB15(27, 24, 20),
        RGB15( 1,  2,  5), RGB15(17, 20, 26), RGB15(22, 25, 30),
        RGB15(29, 31, 31), RGB15(21, 26, 31),
        RGB15(14, 20, 16), RGB15(20, 22, 14), RGB15(27, 20, 18),
        RGB15(24, 15,  6),
        "NAVY"
    },
    {   // 1 — EMBER (warm orange/red)
        RGB15( 5,  2,  1), RGB15( 9,  4,  2),
        RGB15(14,  7,  4), RGB15(20, 11,  6),
        RGB15(22, 12, 18), RGB15(22, 12, 18),
        RGB15( 8,  4,  2), RGB15(28, 18, 10),
        RGB15(31, 24, 10), RGB15( 8,  4,  2), RGB15(31, 28, 14),
        RGB15( 3,  1,  0), RGB15(24, 16, 10), RGB15(30, 22, 12),
        RGB15(31, 31, 20), RGB15(31, 20,  8),
        RGB15(10, 20,  6), RGB15(20, 20,  4), RGB15(30, 10,  4),
        RGB15(12, 20,  2),
        "EMBER"
    },
    {   // 2 — FOREST (deep green)
        RGB15( 1,  4,  1), RGB15( 3,  7,  3),
        RGB15( 5, 12,  5), RGB15( 8, 16,  8),
        RGB15(16, 14, 20), RGB15(16, 14, 20),
        RGB15( 3,  7,  3), RGB15(14, 26, 14),
        RGB15(20, 28, 12), RGB15( 4,  8,  4), RGB15(24, 31, 16),
        RGB15( 0,  2,  0), RGB15(12, 22, 12), RGB15(16, 28, 16),
        RGB15(28, 31, 28), RGB15(10, 28, 16),
        RGB15(10, 22,  6), RGB15(20, 24,  4), RGB15(28, 12,  4),
        RGB15(26, 18,  2),
        "FOREST"
    },
    {   // 3 — NEON (cyan/magenta on black)
        RGB15( 1,  1,  2), RGB15( 3,  3,  6),
        RGB15( 4,  8, 12), RGB15( 8, 12, 18),
        RGB15(24,  8, 24), RGB15(24,  8, 24),
        RGB15( 2,  3,  6), RGB15( 8, 28, 28),
        RGB15(31, 10, 26), RGB15( 4,  4,  8), RGB15(31, 20, 31),
        RGB15( 0,  0,  2), RGB15(10, 22, 24), RGB15(16, 28, 31),
        RGB15(31, 31, 31), RGB15( 4, 30, 30),
        RGB15( 4, 24, 10), RGB15(24, 24,  4), RGB15(30,  8, 16),
        RGB15(28, 16,  2),
        "NEON"
    },
};

static Theme g_theme;

void game_set_theme(int idx) {
    g_theme = theme_presets[idx & (THEME_COUNT-1)];
}

// Macro aliases so all existing draw code works unchanged
#define COL_BG         g_theme.bg
#define COL_GRID_LINE  g_theme.grid_line
#define COL_WIRE_OFF   g_theme.wire_off
#define COL_FIXED      g_theme.fixed_col
#define COL_T_PIECE    g_theme.t_piece
#define COL_CROSS_COL  g_theme.cross_col
#define COL_CURSOR_BG  g_theme.cursor_bg
#define COL_CURSOR     g_theme.cursor
#define COL_SOURCE     g_theme.source
#define COL_ROBOT_OFF  g_theme.robot_off
#define COL_ROBOT_ON   g_theme.robot_on
#define COL_STATUS_BG  g_theme.status_bg
#define COL_TEXT       g_theme.text
#define COL_TITLE      g_theme.title
#define COL_SPARK      g_theme.spark
#define COL_WIRE_ON    g_theme.wire_on
#define COL_TIMER_HI   g_theme.timer_hi
#define COL_TIMER_MID  g_theme.timer_mid
#define COL_TIMER_LO   g_theme.timer_lo
#define COL_ROT_LIMIT  g_theme.rot_limit



// ── Draw helpers ──────────────────────────────────────────────────────────────

static void draw_pixel(int x, int y, u16 color) {
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H)
        backbuf[y * SCREEN_W + x] = color;
}
static void fill_rect(int x, int y, int w, int h, u16 color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            draw_pixel(x+dx, y+dy, color);
}
static void draw_wire_seg(int cx, int cy, int dx, int dy, u16 color) {
    int steps = (dx != 0) ? CELL_W/2 : CELL_H/2;
    for (int t = -1; t <= 1; t++) {
        int ox = (dy!=0)?t:0, oy = (dx!=0)?t:0;
        for (int s = 0; s <= steps; s++)
            draw_pixel(cx+s*dx+ox, cy+s*dy+oy, color);
    }
}

static u16 wire_color(const Cell *c) {
    if (c->powered)                                    return COL_WIRE_ON;
    if (c->type >= WIRE_T_NSE && c->type<=WIRE_T_NEW) return COL_T_PIECE;
    if (c->type == WIRE_CROSS)                         return COL_CROSS_COL;
    if (c->fixed || c->rot_left == 0)                  return COL_FIXED;
    return COL_WIRE_OFF;
}

static void draw_cell(int col, int row, const Cell *c,
                      int is_cursor, int is_source, int is_robot, int hidden) {
    int px = col*CELL_W, py = row*CELL_H;
    int cx = px+CELL_W/2, cy = py+CELL_H/2;

    fill_rect(px+1,py+1,CELL_W-2,CELL_H-2, is_cursor?COL_CURSOR_BG:COL_BG);

    // Ghost mode: hide tiles too far from cursor
    if (hidden && !is_source && !is_robot && !is_cursor) return;

    if (is_cursor) {
        for (int x=px;x<px+CELL_W;x++){draw_pixel(x,py,COL_CURSOR);draw_pixel(x,py+CELL_H-1,COL_CURSOR);}
        for (int y=py;y<py+CELL_H;y++){draw_pixel(px,y,COL_CURSOR);draw_pixel(px+CELL_W-1,y,COL_CURSOR);}
    }
    if (is_source) {
        fill_rect(cx-5,cy-5,10,10,COL_SOURCE);
        fill_rect(cx-2,cy-2, 4, 4,COL_BG);
        fill_rect(cx-1,cy-1, 2, 2,COL_SOURCE);
        return;
    }
    if (is_robot) {
        u16 rc=c->powered?COL_ROBOT_ON:COL_ROBOT_OFF;
        u16 ey=c->powered?COL_SPARK:COL_BG;
        fill_rect(cx-4,cy-7, 8, 7,rc);
        fill_rect(cx-2,cy-5, 2, 2,ey); fill_rect(cx+1,cy-5,2,2,ey);
        fill_rect(cx-5,cy+1,10, 6,rc);
        fill_rect(cx-4,cy+7, 3, 4,rc); fill_rect(cx+1,cy+7,3,4,rc);
        return;
    }
    if (c->type==WIRE_NONE) return;

    u8  conns = wire_conns[c->type];
    u16 wc    = wire_color(c);
    fill_rect(cx-1,cy-1,3,3,wc);
    if (conns&CONN_N) draw_wire_seg(cx,cy, 0,-1,wc);
    if (conns&CONN_S) draw_wire_seg(cx,cy, 0, 1,wc);
    if (conns&CONN_E) draw_wire_seg(cx,cy, 1, 0,wc);
    if (conns&CONN_W) draw_wire_seg(cx,cy,-1, 0,wc);

    // Rotation-limit dots: amber dots in top-right corner (1 or 2 remaining)
    if (c->rot_left != 0xFF && c->rot_left > 0 && !c->powered) {
        int dx = px + CELL_W - 4, dy = py + 3;
        fill_rect(dx,   dy, 2, 2, COL_ROT_LIMIT);
        if (c->rot_left >= 2) fill_rect(dx-4, dy, 2, 2, COL_ROT_LIMIT);
    }
}

// ── Font ──────────────────────────────────────────────────────────────────────

static const u8 font3x5[128][5] = {
    [' ']={0,0,0,0,0}, ['!']={2,2,2,0,2}, ['+']=  {0,2,7,2,0},
    ['-']={0,0,7,0,0}, ['/']=  {1,1,2,4,4}, [':']= {0,2,0,2,0},
    ['0']={7,5,5,5,7}, ['1']={2,6,2,2,7}, ['2']={7,1,7,4,7},
    ['3']={7,1,3,1,7}, ['4']={5,5,7,1,1}, ['5']={7,4,7,1,7},
    ['6']={7,4,7,5,7}, ['7']={7,1,1,1,1}, ['8']={7,5,7,5,7},
    ['9']={7,5,7,1,7},
    ['A']={2,5,7,5,5}, ['B']={6,5,6,5,6}, ['C']={7,4,4,4,7},
    ['D']={6,5,5,5,6}, ['E']={7,4,6,4,7}, ['F']={7,4,6,4,4},
    ['G']={7,4,5,5,7}, ['H']={5,5,7,5,5}, ['I']={7,2,2,2,7},
    ['J']={1,1,1,5,2}, ['K']={5,6,4,6,5}, ['L']={4,4,4,4,7},
    ['M']={5,7,5,5,5}, ['N']={5,7,7,5,5}, ['O']={7,5,5,5,7},
    ['P']={7,5,7,4,4}, ['Q']={2,5,5,7,1}, ['R']={6,5,6,5,5},
    ['S']={7,4,7,1,7}, ['T']={7,2,2,2,2}, ['U']={5,5,5,5,7},
    ['V']={5,5,5,5,2}, ['W']={5,5,5,7,5}, ['X']={5,5,2,5,5},
    ['Y']={5,5,2,2,2}, ['Z']={7,1,2,4,7},
};

static void draw_char(int x, int y, char ch, u16 col) {
    if ((unsigned char)ch>=128) return;
    const u8 *g=font3x5[(unsigned char)ch];
    for (int r=0;r<5;r++){u8 b=g[r];for(int c=0;c<3;c++)if(b&(4>>c))draw_pixel(x+c,y+r,col);}
}
static void draw_char_big(int x, int y, char ch, u16 col, int sc) {
    if ((unsigned char)ch>=128) return;
    const u8 *g=font3x5[(unsigned char)ch];
    for (int r=0;r<5;r++){u8 b=g[r];for(int c=0;c<3;c++)if(b&(4>>c))fill_rect(x+c*sc,y+r*sc,sc,sc,col);}
}
static void draw_string(int x, int y, const char *s, u16 col) {
    while (*s){draw_char(x,y,*s++,col);x+=4;}
}
static void draw_string_big(int x, int y, const char *s, u16 col, int sc) {
    while (*s){draw_char_big(x,y,*s++,col,sc);x+=(3+1)*sc;}
}
static void draw_uint(int x, int y, u32 n, u16 col) {
    char buf[8]; int i=7; buf[i]='\0';
    do{buf[--i]='0'+(n%10);n/=10;}while(n&&i>0);
    draw_string(x,y,buf+i,col);
}

// ── Circuit solver ────────────────────────────────────────────────────────────

static const u8  opp[4]    = {CONN_S,CONN_N,CONN_W,CONN_E};
static const int dir_dc[4] = { 0, 0, 1,-1};
static const int dir_dr[4] = {-1, 1, 0, 0};

static void solve_circuit(Game *g) {
    for (int r=0;r<GRID_H;r++) for (int c=0;c<GRID_W;c++) g->grid[r][c].powered=0;

    int stk[GRID_H*GRID_W][2]; int top=0;
    stk[0][0]=g->sy; stk[0][1]=g->sx; top=1;
    g->grid[g->sy][g->sx].powered=1;

    while (top>0) {
        top--;
        int r=stk[top][0],c=stk[top][1];
        u8 cn=(r==g->sy&&c==g->sx)?(CONN_N|CONN_S|CONN_E|CONN_W):wire_conns[g->grid[r][c].type];
        for (int d=0;d<4;d++) {
            if (!(cn&(1<<d))) continue;
            int nr=r+dir_dr[d],nc=c+dir_dc[d];
            if ((unsigned)nr>=GRID_H||(unsigned)nc>=GRID_W) continue;
            Cell *nb=&g->grid[nr][nc];
            if (nb->powered) continue;
            u8 nc2=(nr==g->ry&&nc==g->rx)?(CONN_N|CONN_S|CONN_E|CONN_W):wire_conns[nb->type];
            if (!(nc2&opp[d])) continue;
            nb->powered=1;
            stk[top][0]=nr;stk[top][1]=nc;top++;
        }
    }
    g->solved=g->grid[g->ry][g->rx].powered;
}

// ── Level generation ──────────────────────────────────────────────────────────

typedef struct {
    int min_path, max_path;
    int fixed_pct;
    int t_pct;
    int decoys;
    int bonus_sec;    // seconds added to timer on completion
    int rot_lim_pct;  // % of eligible non-fixed path tiles that get a rotation limit
    int rot_lim_val;  // how many rotations the player is allowed (1 or 2)
} DiffParam;

static const DiffParam diff_params[DIFF_TIERS] = {
    { 8, 12, 60,  0,  0,  8,  0, 2 },  // 0 easy    +8s   no limits
    { 9, 15, 40,  0,  2, 10, 20, 2 },  // 1 medium  +10s  ~20% get 2 rotations
    {11, 18, 25,  8,  4, 12, 30, 2 },  // 2 hard    +12s  ~30% get 2 rotations
    {13, 22, 15, 18,  6, 14, 30, 1 },  // 3 harder  +14s  ~30% get 1 rotation
    {15, 28, 10, 28,  9, 16, 45, 1 },  // 4 expert  +16s  ~45% get 1 rotation
    {18, 38,  5, 38, 12, 20, 60, 1 },  // 5 brutal  +20s  ~60% get 1 rotation
};

static int  path_r[GRID_H*GRID_W], path_c[GRID_H*GRID_W], path_len;
static u8   vis[GRID_H][GRID_W];

static int find_path(int sr,int sc,int er,int ec,int min_len,int max_len) {
    static int stk_r[GRID_H*GRID_W],stk_c[GRID_H*GRID_W],stk_di[GRID_H*GRID_W];
    static int stk_dirs[GRID_H*GRID_W][4];
    int top=0; path_len=0;
    for(int r=0;r<GRID_H;r++) for(int c=0;c<GRID_W;c++) vis[r][c]=0;

    stk_r[0]=sr;stk_c[0]=sc;stk_di[0]=0;
    stk_dirs[0][0]=0;stk_dirs[0][1]=1;stk_dirs[0][2]=2;stk_dirs[0][3]=3;
    for(int i=3;i>0;i--){int j=rng_range(0,i),t=stk_dirs[0][i];stk_dirs[0][i]=stk_dirs[0][j];stk_dirs[0][j]=t;}
    vis[sr][sc]=1; path_r[0]=sr;path_c[0]=sc;path_len=1;

    while (top>=0) {
        int r=stk_r[top],c=stk_c[top];
        if (r==er&&c==ec&&path_len>=min_len) return 1;
        if ((r==er&&c==ec)||path_len>=max_len||stk_di[top]>=4) {
            vis[r][c]=0; path_len--; top--; continue;
        }
        int d=stk_dirs[top][stk_di[top]++];
        int nr=r+dir_dr[d],nc=c+dir_dc[d];
        if ((unsigned)nr<GRID_H&&(unsigned)nc<GRID_W&&!vis[nr][nc]) {
            top++;
            stk_r[top]=nr;stk_c[top]=nc;stk_di[top]=0;
            stk_dirs[top][0]=0;stk_dirs[top][1]=1;stk_dirs[top][2]=2;stk_dirs[top][3]=3;
            for(int i=3;i>0;i--){int j=rng_range(0,i),t=stk_dirs[top][i];stk_dirs[top][i]=stk_dirs[top][j];stk_dirs[top][j]=t;}
            vis[nr][nc]=1;
            path_r[path_len]=nr;path_c[path_len]=nc;path_len++;
        }
    }
    return 0;
}

static WireType type_for_cell(int i) {
    int pr=path_r[i-1],pc=path_c[i-1],r=path_r[i],c=path_c[i];
    int nr=path_r[i+1],nc=path_c[i+1];
    u8 cn=0;
    if(pr<r)cn|=CONN_N; if(pr>r)cn|=CONN_S; if(pc<c)cn|=CONN_W; if(pc>c)cn|=CONN_E;
    if(nr<r)cn|=CONN_N; if(nr>r)cn|=CONN_S; if(nc<c)cn|=CONN_W; if(nc>c)cn|=CONN_E;
    switch(cn){
        case CONN_E|CONN_W: return WIRE_H;
        case CONN_N|CONN_S: return WIRE_V;
        case CONN_N|CONN_E: return WIRE_NE;
        case CONN_N|CONN_W: return WIRE_NW;
        case CONN_S|CONN_E: return WIRE_SE;
        case CONN_S|CONN_W: return WIRE_SW;
        default:            return WIRE_H;
    }
}

static WireType try_upgrade_T(WireType base, int r, int c) {
    u8 used=wire_conns[base];
    int free[4],nf=0;
    for(int d=0;d<4;d++) if(!(used&(1<<d)))free[nf++]=d;
    if(!nf) return WIRE_CROSS;
    for(int i=nf-1;i>0;i--){int j=rng_range(0,i),t=free[i];free[i]=free[j];free[j]=t;}
    for(int i=0;i<nf;i++){
        int d=free[i],nr=r+dir_dr[d],nc=c+dir_dc[d];
        if((unsigned)nr>=GRID_H||(unsigned)nc>=GRID_W) continue;
        if(vis[nr][nc]) continue;
        u8 nc2=used|(1<<d);
        if(nc2==(CONN_N|CONN_S|CONN_E)) return WIRE_T_NSE;
        if(nc2==(CONN_N|CONN_S|CONN_W)) return WIRE_T_NSW;
        if(nc2==(CONN_S|CONN_E|CONN_W)) return WIRE_T_SEW;
        if(nc2==(CONN_N|CONN_E|CONN_W)) return WIRE_T_NEW;
    }
    return base;
}

void game_generate(Game *g, int level_num, u32 run_seed) {
    int diff = level_num / 3;
    if (diff >= DIFF_TIERS) diff = DIFF_TIERS-1;
    const DiffParam *dp = &diff_params[diff];

    rng_seed(run_seed ^ ((u32)level_num * 2654435761u));

    // ── Pick endpoints ────────────────────────────────────────────────────────
    int sr,sc,er,ec, attempts=0;
    do {
        int pair = (diff<2)?rng_range(0,1):rng_range(0,3);
        switch(pair){
            case 0: sc=0;sr=rng_range(0,GRID_H-1);ec=GRID_W-1;er=rng_range(0,GRID_H-1);break;
            case 1: sr=0;sc=rng_range(0,GRID_W-1);er=GRID_H-1;ec=rng_range(0,GRID_W-1);break;
            case 2: sc=0;sr=rng_range(0,GRID_H-1);er=GRID_H-1;ec=rng_range(0,GRID_W-1);break;
            default:sr=0;sc=rng_range(0,GRID_W-1);ec=GRID_W-1;er=rng_range(0,GRID_H-1);break;
        }
    } while(++attempts<20&&(sr==er&&sc==ec));

    // ── Generate path ─────────────────────────────────────────────────────────
    int found=0;
    for(int t=0;t<60&&!found;t++){rng_next();found=find_path(sr,sc,er,ec,dp->min_path,dp->max_path);}

    if(!found){ // fallback: L-shaped path
        int i=0; path_r[i]=sr;path_c[i]=sc;i++;
        int r=sr,c=sc;
        while(c!=ec&&i<GRID_H*GRID_W-1){c+=(ec>c)?1:-1;path_r[i]=r;path_c[i]=c;i++;}
        while(r!=er&&i<GRID_H*GRID_W-1){r+=(er>r)?1:-1;path_r[i]=r;path_c[i]=c;i++;}
        path_len=i;
        for(int j=0;j<path_len;j++) vis[path_r[j]][path_c[j]]=1;
    }

    // ── Build grid ────────────────────────────────────────────────────────────
    for(int r=0;r<GRID_H;r++) for(int c=0;c<GRID_W;c++){
        g->grid[r][c].type=WIRE_NONE;g->grid[r][c].fixed=0;g->grid[r][c].powered=0;g->grid[r][c].rot_left=0xFF;
    }

    for(int i=1;i<path_len-1;i++){
        int r=path_r[i],c=path_c[i];
        WireType wt=type_for_cell(i);
        if(rng_range(0,99)<dp->t_pct) wt=try_upgrade_T(wt,r,c);

        int is_fixed=(rng_range(0,99)<dp->fixed_pct);
        if(wt!=WIRE_H&&wt!=WIRE_V&&rng_range(0,1)) is_fixed=1; // corners more likely fixed
        if(wt>=WIRE_T_NSE) is_fixed=0; // T-pieces always scrambled

        g->grid[r][c].type=wt; g->grid[r][c].fixed=is_fixed;

        if(!is_fixed){
            // Check if this tile gets a rotation limit (not CROSS — it's symmetric)
            int is_limited = dp->rot_lim_pct > 0
                          && wt != WIRE_CROSS
                          && rng_range(0,99) < dp->rot_lim_pct;
            if(is_limited){
                // Scramble by exactly (cycle - rot_lim_val) so the tile is
                // always solvable within rot_lim_val clockwise rotations.
                int cycle = (wt==WIRE_H||wt==WIRE_V) ? 2 : 4;
                int min_s = cycle - dp->rot_lim_val;
                if(min_s < 1) min_s = 1;
                int max_s = cycle - 1;
                int rots  = (min_s < max_s) ? rng_range(min_s, max_s) : min_s;
                for(int k=0;k<rots;k++) g->grid[r][c].type=rotate_cw(g->grid[r][c].type);
                g->grid[r][c].rot_left = (u8)dp->rot_lim_val;
            } else {
                int rots=rng_range(1,3);
                for(int k=0;k<rots;k++) g->grid[r][c].type=rotate_cw(g->grid[r][c].type);
                // rot_left stays 0xFF (unlimited)
            }
        }
    }

    // ── Decoys ────────────────────────────────────────────────────────────────
    static const WireType dtypes[]={WIRE_H,WIRE_V,WIRE_NE,WIRE_NW,WIRE_SE,WIRE_SW,WIRE_T_NSE,WIRE_T_SEW};
    for(int d=0;d<dp->decoys;d++){
        for(int t=0;t<40;t++){
            int r=rng_range(0,GRID_H-1),c=rng_range(0,GRID_W-1);
            if(r==sr&&c==sc) continue; if(r==er&&c==ec) continue;
            if(g->grid[r][c].type!=WIRE_NONE) continue;
            WireType wt=dtypes[rng_range(0,7)];
            int rots=rng_range(0,3);
            for(int k=0;k<rots;k++) wt=rotate_cw(wt);
            g->grid[r][c].type=wt; g->grid[r][c].fixed=0;
            break;
        }
    }

    // ── Set state (do NOT touch timer_frames, score, or mode) ───────────────
    g->sx=sc; g->sy=sr; g->rx=ec; g->ry=er;
    g->cx=path_c[1]; g->cy=path_r[1];
    g->level_num=level_num;
    g->run_seed=run_seed;
    g->win_timer=0;
    g->solved=0;
    g->time_bonus=dp->bonus_sec*60;
    g->frenzy_timer=3*60;

    solve_circuit(g);
}

// ── Public API ────────────────────────────────────────────────────────────────

void game_update(Game *g) {
    if (!g->solved) {
        if(key_pressed(KEY_RIGHT)&&g->cx<GRID_W-1) g->cx++;
        if(key_pressed(KEY_LEFT) &&g->cx>0)         g->cx--;
        if(key_pressed(KEY_DOWN) &&g->cy<GRID_H-1) g->cy++;
        if(key_pressed(KEY_UP)   &&g->cy>0)          g->cy--;
        Cell *c=&g->grid[g->cy][g->cx];
        if(key_pressed(KEY_A)&&!c->fixed&&c->type!=WIRE_NONE&&(c->rot_left==0xFF||c->rot_left>0)){
            c->type=rotate_cw(c->type);
            if(c->rot_left!=0xFF) c->rot_left--;
        }

        // Frenzy mode: auto-rotate a random non-fixed tile every 3 seconds
        if (g->mode == MODE_FRENZY) {
            g->frenzy_timer--;
            if (g->frenzy_timer <= 0) {
                g->frenzy_timer = 3*60;
                // Collect eligible tiles
                int cands[GRID_H*GRID_W][2]; int nc=0;
                for(int r=0;r<GRID_H;r++) for(int c2=0;c2<GRID_W;c2++){
                    Cell *t=&g->grid[r][c2];
                    if(t->type!=WIRE_NONE&&!t->fixed&&
                       !(r==g->sy&&c2==g->sx)&&!(r==g->ry&&c2==g->rx))
                        { cands[nc][0]=r; cands[nc][1]=c2; nc++; }
                }
                if(nc>0){
                    int pick=rng_next()%((u32)nc);
                    Cell *t=&g->grid[cands[pick][0]][cands[pick][1]];
                    t->type=rotate_cw(t->type);
                }
            }
        }

        solve_circuit(g);
    }
    if(g->solved) g->win_timer++;
}

static const char *diff_name[DIFF_TIERS]={"EASY","MEDIUM","HARD","HARDER","EXPERT","BRUTAL"};

static void draw_timer_bar(int timer_frames) {
    // Bar: y=GRID_H*CELL_H, 4px tall, full width
    int bar_y = GRID_H*CELL_H;
    int bar_w = timer_frames * (SCREEN_W-2) / INITIAL_TIMER;
    if (bar_w > SCREEN_W-2) bar_w = SCREEN_W-2;
    if (bar_w < 0)           bar_w = 0;

    // Background track
    fill_rect(1, bar_y, SCREEN_W-2, 4, COL_STATUS_BG);

    // Colored fill — urgent when low
    u16 bar_col = (bar_w > (SCREEN_W-2)/2) ? COL_TIMER_HI  :
                  (bar_w > (SCREEN_W-2)/4) ? COL_TIMER_MID :
                                              COL_TIMER_LO;

    // Flash when under 10 seconds
    int secs = timer_frames / 60;
    if (secs < 10 && (timer_frames/15)&1) bar_col = COL_BG;

    fill_rect(1, bar_y, bar_w, 4, bar_col);
}

static void draw_status(const Game *g) {
    int sy = GRID_H*CELL_H;
    fill_rect(0, sy, SCREEN_W, SCREEN_H - sy, COL_STATUS_BG);

    draw_timer_bar(g->timer_frames);

    // Line 1: level number, difficulty, score, time remaining
    int diff = g->level_num/3; if(diff>=DIFF_TIERS)diff=DIFF_TIERS-1;
    int secs = g->timer_frames/60;

    static const char *mode_name[MODE_COUNT]={"DRONE","GHOST","FRENZY"};
    draw_string(2,  sy+6,  "LV",              COL_TEXT);
    draw_uint  (14, sy+6,  (u32)(g->level_num+1), COL_TITLE);
    draw_string(30, sy+6,  diff_name[diff],   COL_FIXED);
    draw_string(82, sy+6,  mode_name[g->mode],COL_T_PIECE);

    draw_string(130,sy+6,  "SCORE",           COL_TEXT);
    draw_uint  (162,sy+6,  (u32)g->score,     COL_TITLE);

    draw_uint  (196,sy+6,  (u32)secs,         secs<10?COL_TIMER_LO:COL_TEXT);
    draw_char  (216,sy+6,  'S',               COL_TEXT);

    // Line 2: controls or win message
    if (g->solved && g->win_timer < 80) {
        // Flash bonus amount
        if ((g->win_timer/8)&1) {
            draw_char  (2,  sy+14, '+',              COL_WIRE_ON);
            draw_uint  (8,  sy+14, (u32)(g->time_bonus/60), COL_WIRE_ON);
            draw_string(24, sy+14, "S BONUS  ROBOT AWAKE!", COL_WIRE_ON);
        }
    } else if (g->solved) {
        draw_string(2, sy+14, "ROBOT AWAKE  PRESS START",
                    (g->win_timer/20)&1 ? COL_WIRE_ON : COL_STATUS_BG);
    } else {
        draw_string(2,   sy+14, "A:ROT",  COL_TEXT);
        draw_string(44,  sy+14, "PAD:MOV",COL_TEXT);
        draw_string(104, sy+14, "SEL:QUIT",COL_TEXT);
        // Show run seed small on right
        draw_string(168, sy+14, "RUN",    COL_WIRE_OFF);
        draw_uint  (192, sy+14, g->run_seed%10000u, COL_WIRE_OFF);
    }
}

void game_draw(const Game *g) {
    for(int r=0;r<GRID_H;r++)
        for(int c=0;c<GRID_W;c++){
            int dist = (c>g->cx?c-g->cx:g->cx-c)+(r>g->cy?r-g->cy:g->cy-r);
            int hidden = (g->mode==MODE_GHOST) && !g->solved
                       && !g->grid[r][c].powered && (dist>2);
            draw_cell(c,r,&g->grid[r][c],
                      c==g->cx&&r==g->cy,
                      c==g->sx&&r==g->sy,
                      c==g->rx&&r==g->ry,
                      hidden);
        }
    for(int c=0;c<=GRID_W;c++) for(int y=0;y<GRID_H*CELL_H;y++) draw_pixel(c*CELL_W,y,COL_GRID_LINE);
    for(int r=0;r<=GRID_H;r++) for(int x=0;x<SCREEN_W;x++)      draw_pixel(x,r*CELL_H,COL_GRID_LINE);
    draw_status(g);
}

// ── Game Over screen ──────────────────────────────────────────────────────────

void game_draw_gameover(const Game *g, int high_score, int frame) {
    fill_rect(0,0,SCREEN_W,SCREEN_H,COL_BG);

    u16 c1 = (frame&16)?COL_CURSOR:COL_TIMER_LO;
    draw_string_big(52, 10, "GAME", c1, 4);
    draw_string_big(52, 46, "OVER", c1, 4);

    // Score
    draw_string(68, 88, "SCORE", COL_TEXT);
    draw_uint  (100,88, (u32)g->score, COL_TITLE);

    int diff=g->level_num/3; if(diff>=DIFF_TIERS)diff=DIFF_TIERS-1;
    draw_string(116,88, diff_name[diff], COL_FIXED);

    // High score
    if (g->score > 0 && g->score >= high_score) {
        draw_string(60, 100, "NEW BEST!", (frame/10)&1?COL_WIRE_ON:COL_TITLE);
    } else {
        draw_string(68, 100, "BEST", COL_TEXT);
        draw_uint  (100,100, (u32)high_score, COL_TEXT);
    }

    // Sharing info
    draw_string(44, 113, "RUN CODE", COL_WIRE_OFF);
    draw_uint  (100,113, g->run_seed%10000u, COL_WIRE_OFF);

    if (frame > 60 && (frame/20)&1)
        draw_string(68, 130, "PRESS START", COL_TEXT);
}

// ── Title screen ─────────────────────────────────────────────────────────────

#define TITLE_ANIM_LEN 90
#define TWX0 20
#define TWX1 210
#define TWY  68

static void draw_title_robot(int x,int y,int awake){
    u16 rc=awake?COL_ROBOT_ON:COL_ROBOT_OFF, ey=awake?COL_SPARK:COL_BG;
    fill_rect(x-4,y-7,8,7,rc);
    fill_rect(x-2,y-5,2,2,ey);fill_rect(x+1,y-5,2,2,ey);
    fill_rect(x-5,y+1,10,6,rc);
    fill_rect(x-4,y+7,3,4,rc);fill_rect(x+1,y+7,3,4,rc);
}

static const char *mode_desc[MODE_COUNT]={
    "STANDARD PLAY",
    "TILES HIDDEN NEAR CURSOR",
    "TILES AUTO-ROTATE",
};

void game_draw_title(int frame, int theme_idx, GameMode mode) {
    fill_rect(0,0,SCREEN_W,SCREEN_H,COL_BG);

    // Title (scale 4 = 20px tall each)
    draw_string_big(10, 6, "WIRE", COL_TITLE, 4);
    draw_string_big(10,30, "GAME", COL_TITLE, 4);
    draw_string   (120, 9, "RESCUE", COL_TEXT);
    draw_string   (120,18, "THE",    COL_TEXT);
    draw_string   (120,27, "ROBOTS", COL_TEXT);

    // Animated wire + robot (y=68, leaves room above and below)
    for(int t=-1;t<=1;t++) for(int x=TWX0;x<=TWX1;x++) draw_pixel(x,TWY+t,COL_WIRE_OFF);
    fill_rect(TWX0-4,TWY-4,8,8,COL_SOURCE);
    fill_rect(TWX0-1,TWY-1,2,2,COL_BG);

    int wl=TWX1-TWX0;
    int px=TWX0+(frame*wl/TITLE_ANIM_LEN); if(px>TWX1)px=TWX1;
    for(int t=-1;t<=1;t++) for(int x=TWX0;x<=px;x++) draw_pixel(x,TWY+t,COL_WIRE_ON);
    if(frame<TITLE_ANIM_LEN&&px>TWX0) for(int t=-2;t<=2;t++) draw_pixel(px,TWY+t,COL_SPARK);

    draw_title_robot(TWX1+12,TWY,frame>=TITLE_ANIM_LEN);

    // ── Menu ─────────────────────────────────────────────────────────────────
    // Divider
    for(int x=0;x<SCREEN_W;x++) draw_pixel(x,78,COL_GRID_LINE);

    // Theme row
    draw_string( 4, 83, "THEME", COL_TEXT);
    draw_string(38, 83, "< >",   COL_WIRE_OFF);
    draw_string(62, 83, theme_presets[theme_idx].name, COL_TITLE);

    // Mode row
    draw_string( 4, 95, "MODE",  COL_TEXT);
    draw_string(30, 95, "^ v",   COL_WIRE_OFF);
    static const char *mnames[MODE_COUNT]={"DRONE","GHOST","FRENZY"};
    draw_string(54, 95, mnames[mode], COL_T_PIECE);
    draw_string( 4,105, mode_desc[mode], COL_WIRE_OFF);

    // Divider
    for(int x=0;x<SCREEN_W;x++) draw_pixel(x,115,COL_GRID_LINE);

    // Press start + tagline
    if((frame/20)&1)
        draw_string(72,121,"PRESS START",COL_TEXT);
    draw_string(4,134,"ENDLESS  TIMER  BEAT AS MANY AS YOU CAN",COL_WIRE_OFF);
}

int game_update_title(int *theme_idx, GameMode *mode) {
    if(key_pressed(KEY_RIGHT)){ *theme_idx=(*theme_idx+1)%THEME_COUNT; game_set_theme(*theme_idx); }
    if(key_pressed(KEY_LEFT)) { *theme_idx=(*theme_idx+THEME_COUNT-1)%THEME_COUNT; game_set_theme(*theme_idx); }
    if(key_pressed(KEY_DOWN)) { *mode=(*mode+1)%MODE_COUNT; }
    if(key_pressed(KEY_UP))   { *mode=(*mode+MODE_COUNT-1)%MODE_COUNT; }
    return key_pressed(KEY_START);
}
