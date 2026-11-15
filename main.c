#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#define WORLD 3600.0f
#define MAXSEG 300
#define NSN 8
#define NPEL 600
#define PI 3.14159265f
#define NTR 120

typedef struct {
    float x[MAXSEG], y[MAXSEG];
    int len, alive, respawn, score, kills;
    float ang, tgt;
    u32 col, glow, col2;
    char name;
} Snake;

typedef struct {
    float x, y;
    int on, val;
    u32 col, glow;
} Pellet;

typedef struct {
    float x, y;
    int life;
    u32 col;
} Trail;

static gcmContextData *ctx;
static u32 W, H;
static u32 *fb;
static u32 fboff;
static u32 *back;
static int cur = 0, first = 1;
static volatile int running = 1;

static Snake sn[NSN];
static Pellet pel[NPEL];
static Trail tr[NTR];
static int tri = 0;
static int frame = 0, dead = 0, quality = 1;
static float camx = 1800, camy = 1800, zoom = 1.0f, ax = 0, ay = 0, mag = 0;
static int bx = 0, px = 0, ptri = 0;
static int xp = 0, tprev = 0;

static u32 pal = {0x00FF2E63, 0x0000F0FF, 0x0000FF88, 0x00FFE600, 0x00B967FF, 0x00FF6BCB, 0x0000FFAA, 0x00FF7A00};
static u32 pal2 = {0x00FF6BCB, 0x0000FF88, 0x00FFE600, 0x00FF2E63, 0x0000F0FF, 0x00B967FF, 0x00FF2E63, 0x0000FF88};
static u32 glowpal = {0x00350015, 0x00002535, 0x00003518, 0x00352800, 0x00220035, 0x00351828, 0x00003522, 0x00351A00};
static const char *bnames = {"BOB", "ZE", "LIA", "TURBO", "RAFA", "NINA", "KAKA"};

static void cb(u64 s, u64 p, void *u) {
    (void)p; (void)u;
    if (s == SYSUTIL_EXIT_GAME) running = 0;
}

static void init_screen(void) {
    void *h = memalign(1024 * 1024, 32 * 1024 * 1024);
    rsxInit(&ctx, 0x100000, 32 * 1024 * 1024, h);
    videoState st;
    videoGetState(VIDEO_PRIMARY, 0, &st);
    u32 r = st.displayMode.resolution;
    if (videoGetResolutionAvailability(VIDEO_PRIMARY, VIDEO_RESOLUTION_720, VIDEO_ASPECT_AUTO, 0))
        r = VIDEO_RESOLUTION_720;
    videoResolution res;
    videoGetResolution(r, &res);
    W = res.width;
    H = res.height;
    videoConfiguration vc;
    memset(&vc, 0, sizeof(vc));
    vc.resolution = r;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.aspect = VIDEO_ASPECT_AUTO;
    vc.pitch = W * 4;
    videoConfigure(VIDEO_PRIMARY, &vc, NULL, 0);
    gcmSetFlipMode(GCM_FLIP_VSYNC);
    for (int i = 0; i < 2; i++) {
        fb[i] = (u32 *)rsxMemalign(64, W * H * 4);
        gcmAddressToOffset(fb[i], &fboff[i]);
        gcmSetDisplayBuffer(i, fboff[i], W * 4, W, H);
    }
    back = (u32 *)memalign(64, W * H * 4);
    memset(back, 0, W * H * 4);
    gcmResetFlipStatus();
}

static void present(void) {
    if (!first) {
        while (gcmGetFlipStatus() != 0) usleep(200);
        gcmResetFlipStatus();
    } else {
        gcmResetFlipStatus();
    }
    memcpy(fb[cur], back, W * H * 4);
    gcmSetFlip(ctx, cur);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur ^= 1;
    first = 0;
}

static inline u32 add_col(u32 a, u32 b) {
    u32 r = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF);
    u32 g = ((a >> 8) & 0xFF) + ((b >> 8) & 0xFF);
    u32 bl = (a & 0xFF) + (b & 0xFF);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (bl > 255) bl = 255;
    return (r << 16) | (g << 8) | bl;
}

static void rect(int x, int y, int w, int h, u32 c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w, y1 = y + h;
    if (x1 > (int)W) x1 = W;
    if (y1 > (int)H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = back + yy * W;
        for (int xx = x0; xx < x1; xx++) p[xx] = c;
    }
}

static void circ(int cx, int cy, int r, u32 c) {
    if (r < 1) r = 1;
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= (int)H) continue;
        int hw = (int)sqrtf((float)(r * r - dy * dy));
        int x0 = cx - hw, x1 = cx + hw;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int)W) x1 = W - 1;
        u32 *p = back + y * W;
        for (int x = x0; x <= x1; x++) p[x] = c;
    }
}

static void glow_circ(int cx, int cy, int r, u32 c) {
    if (r < 1) return;
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= (int)H) continue;
        int hw = (int)sqrtf((float)(r * r - dy * dy));
        int x0 = cx - hw, x1 = cx + hw;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int)W) x1 = W - 1;
        u32 *p = back + y * W;
        for (int x = x0; x <= x1; x++) p[x] = add_col(p[x], c);
    }
}
static void line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    while (1) {
        if (x0 >= 0 && x0 < (int)W && y0 >= 0 && y0 < (int)H) back[y0 * W + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

static void hex(int cx, int cy, int r, u32 c) {
    for (int k = 0; k < 6; k++) {
        float a1 = k * PI / 3.0f, a2 = (k + 1) * PI / 3.0f;
        line(cx + (int)(cosf(a1) * r), cy + (int)(sinf(a1) * r),
             cx + (int)(cosf(a2) * r), cy + (int)(sinf(a2) * r), c);
    }
}

typedef struct { char c; const char *p; } G;
static const G gl[] = {
    {'0',"###" "#.#" "#.#" "#.#" "###"}, {'1',".#." "##." ".#." ".#." "###"},
    {'2',"###" "..#" "###" "#.." "###"}, {'3',"###" "..#" "###" "..#" "###"},
    {'4',"#.#" "#.#" "###" "..#" "..#"}, {'5',"###" "#.." "###" "..#" "###"},
    {'6',"###" "#.." "###" "#.#" "###"}, {'7',"###" "..#" "..#" "..#" "..#"},
    {'8',"###" "#.#" "###" "#.#" "###"}, {'9',"###" "#.#" "###" "..#" "###"},
    {'A',".#." "#.#" "###" "#.#" "#.#"}, {'B',"##." "#.#" "##." "#.#" "##."},
    {'C',"###" "#.." "#.." "#.." "###"}, {'D',"##." "#.#" "#.#" "#.#" "##."},
    {'E',"###" "#.." "###" "#.." "###"}, {'F',"###" "#.." "##." "#.." "#.."},
    {'G',"###" "#.." "#.#" "#.#" "###"}, {'H',"#.#" "#.#" "###" "#.#" "#.#"},
    {'I',"###" ".#." ".#." ".#." "###"}, {'J',"###" "..#" "..#" "#.#" "###"},
    {'K',"#.#" "#.#" "##." "#.#" "#.#"}, {'L',"#.." "#.." "#.." "#.." "###"},
    {'M',"#.#" "###" "###" "#.#" "#.#"}, {'N',"#.#" "###" "###" "###" "#.#"},
    {'O',"###" "#.#" "#.#" "#.#" "###"}, {'P',"###" "#.#" "###" "#.." "#.."},
    {'Q',"###" "#.#" "#.#" "###" ".#."}, {'R',"##." "#.#" "##." "#.#" "#.#"},
    {'S',"###" "#.." "###" "..#" "###"}, {'T',"###" ".#." ".#." ".#." ".#."},
    {'U',"#.#" "#.#" "#.#" "#.#" "###"}, {'V',"#.#" "#.#" "#.#" "#.#" ".#."},
    {'W',"#.#" "#.#" "###" "###" "#.#"}, {'X',"#.#" "#.#" ".#." "#.#" "#.#"},
    {'Y',"#.#" "#.#" ".#." ".#." ".#."}, {'Z',"###" "..#" ".#." "#.." "###"},
    {' ',"   " "   " "   " "   " "   "}, {'-',"   " "   " "###" "   " "   "},
    {':',"   " ".#." "..." ".#." "   "}, {'.',"   " "   " "   " "   " ".#."}
};

static const char *findg(char c) {
    for (unsigned i = 0; i < sizeof(gl) / sizeof(gl); i++)
        if (gl[i].c == c) return gl[i].p;
    return 0;
}

static void txt(const char *s, int x, int y, int sc, u32 c) {
    for (; *s; s++) {
        const char *g = findg(*s);
        if (g) {
            for (int r = 0; r < 5; r++)
                for (int k = 0; k < 3; k++)
                    if (g[r * 3 + k] == '#')
                        rect(x + k * sc, y + r * sc, sc, sc, c);
        }
        x += 4 * sc;
    }
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static float rad(Snake *s) { return 11.0f + s->len * 0.045f; }

static void drop_pellet(float x, float y, u32 col, u32 glow, int val) {
    for (int k = 0; k < NPEL; k++) {
        if (!pel[k].on) {
            pel[k].x = x;
            pel[k].y = y;
            pel[k].on = 1;
            pel[k].val = val;
            pel[k].col = col;
            pel[k].glow = glow;
            return;
        }
    }
}

static void spawn(int i) {
    Snake *s = &sn[i];
    float x = 400 + frand() * (WORLD - 800), y = 400 + frand() * (WORLD - 800);
    s->ang = frand() * 2 * PI;
    s->tgt = s->ang;
    s->len = 14;
    s->alive = 1;
    s->respawn = 0;
    s->score = 0;
    s->kills = 0;
    for (int j = 0; j < s->len; j++) {
        s->x[j] = x - cosf(s->ang) * 7 * j;
        s->y[j] = y - sinf(s->ang) * 7 * j;
    }
    if (i == 0) {
        s->col = pal; s->glow = glowpal; s->col2 = pal2;
        strcpy(s->name, "VOCE");
    } else {
        s->score = rand() % 600;
        s->kills = rand() % 4;
        int c = rand() % 8;
        s->col = pal[c]; s->glow = glowpal[c]; s->col2 = pal2[c];
        strcpy(s->name, bnames[(i - 1) % 7]);
    }
}

static void kill_snake(int i, int killer) {
    Snake *s = &sn[i];
    if (i == 0) dead = 0;
    else if (killer == 0) { sn.kills++; sn.score += 150; }
    for (int j = 0; j < s->len; j += 2) {
        drop_pellet(s->x[j] + (frand() - 0.5f) * 16.0f,
                    s->y[j] + (frand() - 0.5f) * 16.0f,
                    (j % 4 == 0) ? s->col2 : s->col, s->glow, 2);
    }
    s->alive = 0;
    s->respawn = 180 + rand() % 100;
}

static void init_game(void) {
    for (int k = 0; k < NTR; k++) tr[k].life = 0;
    for (int k = 0; k < NPEL; k++) {
        if (k < 420) {
            pel[k].x = frand() * WORLD;
            pel[k].y = frand() * WORLD;
            pel[k].on = 1;
            pel[k].val = 1;
            int c = rand() % 8;
            pel[k].col = pal[c];
            pel[k].glow = glowpal[c];
        } else {
            pel[k].on = 0;
        }
    }
    for (int i = 0; i < NSN; i++) spawn(i);
    dead = 0;
}

static void readpad(void) {
    padInfo info;
    padData d;
    ax = ay = mag = 0;
    bx = px = ptri = 0;
    ioPadGetInfo(&info);
    if (info.status) {
        ioPadGetData(0, &d);
        float a = ((int)d.ANA_L_H - 128) / 128.0f;
        float b = ((int)d.ANA_L_V - 128) / 128.0f;
        if (d.BTN_LEFT) a = -1;
        if (d.BTN_RIGHT) a = 1;
        if (d.BTN_UP) b = -1;
        if (d.BTN_DOWN) b = 1;
        ax = a; ay = b;
        mag = sqrtf(a * a + b * b);
        bx = d.BTN_CROSS || d.BTN_R1;
        px = d.BTN_CROSS;
        ptri = d.BTN_TRIANGLE;
    }
}
static void update(void) {
    frame++;
    if (ptri && !tprev) quality ^= 1;
    tprev = ptri;
    for (int t = 0; t < NTR; t++) if (tr[t].life > 0) tr[t].life--;

    for (int i = 0; i < NSN; i++) {
        Snake *s = &sn[i];
        if (!s->alive) {
            if (--s->respawn <= 0) spawn(i);
            continue;
        }
        if (i == 0) {
            if (mag > 0.25f) s->tgt = atan2f(ay, ax);
        } else {
            if ((frame % 20) == 0) {
                float best = 999999;
                int bi = -1;
                for (int k = 0; k < NPEL; k += 2) {
                    if (!pel[k].on) continue;
                    float dx = pel[k].x - s->x, dy = pel[k].y - s->y, d = dx * dx + dy * dy;
                    if (d < best) { best = d; bi = k; }
                }
                if (bi >= 0) s->tgt = atan2f(pel[bi].y - s->y, pel[bi].x - s->x);
            }
            if (s->x < 300 || s->x > WORLD - 300 || s->y < 300 || s->y > WORLD - 300)
                s->tgt = atan2f(WORLD / 2 - s->y, WORLD / 2 - s->x);
        }

        float d = s->tgt - s->ang;
        while (d > PI) d -= 2 * PI;
        while (d < -PI) d += 2 * PI;
        if (d > 0.085f) d = 0.085f;
        if (d < -0.085f) d = -0.085f;
        s->ang += d;

        int turbo = 0;
        if (i == 0 && bx && s->len > 14) turbo = 1;
        if (i != 0 && rand() % 200 == 0 && s->len > 16) turbo = 1;
        float sp = turbo ? 5.4f : 3.1f;

        if (turbo && s->len > 14 && (frame % 16) == 0) {
            s->len--;
            drop_pellet(s->x[s->len], s->y[s->len], s->col, s->glow, 1);
        }

        s->x += cosf(s->ang) * sp;
        s->y += sinf(s->ang) * sp;

        if (turbo) {
            tr[tri].x = s->x - cosf(s->ang) * 18;
            tr[tri].y = s->y - sinf(s->ang) * 18;
            tr[tri].life = 16;
            tr[tri].col = s->glow;
            tri = (tri + 1) % NTR;
        }

        for (int j = 1; j < s->len; j++) {
            float dx = s->x[j - 1] - s->x[j], dy = s->y[j - 1] - s->y[j], dd = dx * dx + dy * dy;
            if (dd > 49.0f) {
                dd = sqrtf(dd);
                float f = (dd - 7.0f) / dd;
                s->x[j] += dx * f;
                s->y[j] += dy * f;
            }
        }

        if (s->x < 0 || s->x > WORLD || s->y < 0 || s->y > WORLD) {
            kill_snake(i, -1);
            continue;
        }

        float rs = rad(s), er = rs + 15.0f;
        for (int k = 0; k < NPEL; k++) {
            if (!pel[k].on) continue;
            float dx = pel[k].x - s->x, dy = pel[k].y - s->y;
            if (dx * dx + dy * dy < er * er) {
                pel[k].on = 0;
                s->score += 10 * pel[k].val;
                if (s->len < MAXSEG) {
                    s->x[s->len] = s->x[s->len - 1];
                    s->y[s->len] = s->y[s->len - 1];
                    s->len++;
                }
                if (k < 420) {
                    pel[k].x = frand() * WORLD;
                    pel[k].y = frand() * WORLD;
                    pel[k].on = 1;
                    pel[k].val = 1;
                    int c = rand() % 8;
                    pel[k].col = pal[c];
                    pel[k].glow = glowpal[c];
                }
            }
        }

        for (int o = 0; o < NSN; o++) {
            if (o == i || !sn[o].alive) continue;
            float lim = (rs + rad(&sn[o])) * 0.72f;
            lim *= lim;
            for (int j = 0; j < sn[o].len; j += 2) {
                float dx = sn[o].x[j] - s->x, dy = sn[o].y[j] - s->y;
                if (dx * dx + dy * dy < lim) {
                    kill_snake(i, o);
                    break;
                }
            }
            if (!s->alive) break;
        }
    }

    if (sn.alive) {
        camx = sn.x;
        camy = sn.y;
        float target_zoom = 1.05f - (sn.len - 14) * 0.0014f;
        if (target_zoom < 0.65f) target_zoom = 0.65f;
        zoom += (target_zoom - zoom) * 0.05f;
    } else {
        dead++;
        if (dead > 60 && px && !xp) spawn(0);
    }
    xp = px;
}

static void render(void) {
    float sc = ((float)W / 1280.0f) * zoom;
    float hw = W / 2.0f, hh = H / 2.0f;

    rect(0, 0, W, H, 0x00060714);

    float hs = (quality ? 110.0f : 190.0f) * zoom;
    int hr = (int)((quality ? 38.0f : 28.0f) * zoom);
    if (hr < 8) hr = 8;

    for (int gy = -1; gy < 28; gy++) {
        for (int gx = -1; gx < 22; gx++) {
            int ox = (int)(gx * hs * 1.5f - fmodf(camx * sc, hs * 1.5f));
            int oy = (int)(gy * hs * 1.73f - fmodf(camy * sc, hs * 1.73f));
            if (gx % 2) oy += (int)(hs * 0.86f);
            if (ox > -60 && ox < (int)W + 60 && oy > -60 && oy < (int)H + 60)
                hex(ox, oy, hr, 0x00101228);
        }
    }

    int bx0 = (int)((0 - camx) * sc + hw), by0 = (int)((0 - camy) * sc + hh);
    int bx1 = (int)((WORLD - camx) * sc + hw), by1 = (int)((WORLD - camy) * sc + hh);
    rect(bx0, by0, bx1 - bx0, 3, 0x00FF2E63);
    rect(bx0, by1, bx1 - bx0, 3, 0x00FF2E63);
    rect(bx0, by0, 3, by1 - by0, 0x00FF2E63);
    rect(bx1, by0, 3, by1 - by0, 0x00FF2E63);

    for (int t = 0; t < NTR; t++) {
        if (tr[t].life <= 0) continue;
        int px2 = (int)((tr[t].x - camx) * sc + hw), py = (int)((tr[t].y - camy) * sc + hh);
        glow_circ(px2, py, (int)(11 * zoom), tr[t].col);
    }

    for (int k = 0; k < NPEL; k++) {
        if (!pel[k].on) continue;
        int px2 = (int)((pel[k].x - camx) * sc + hw), py = (int)((pel[k].y - camy) * sc + hh);
        if (px2 < -30 || px2 > (int)W + 30 || py < -30 || py > (int)H + 30) continue;
        int base_r = (pel[k].val > 1) ? 7 : 4;
        int pulse = (int)((base_r + ((frame / 8 + k) % 3)) * zoom);
        if (pulse < 2) pulse = 2;
        if (quality == 1) glow_circ(px2, py, pulse + (int)(7 * zoom), pel[k].glow);
        circ(px2, py, pulse, pel[k].col);
        circ(px2, py, pulse / 2, 0x00FFFFFF);
    }

    for (int i = NSN - 1; i >= 0; i--) {
        Snake *s = &sn[i];
        if (!s->alive) continue;
        int r = (int)(rad(s) * sc);
        if (r < 4) r = 4;
        for (int j = s->len - 1; j >= 0; j--) {
            int px2 = (int)((s->x[j] - camx) * sc + hw), py = (int)((s->y[j] - camy) * sc + hh);
            if (px2 < -r - 20 || px2 > (int)W + r + 20 || py < -r - 20 || py > (int)H + r + 20) continue;
            u32 c = (j % 4 == 0) ? s->col2 : s->col;
            if (quality == 1 && (j % 2 == 0)) glow_circ(px2, py, r + 6, s->glow);
            circ(px2, py, r, c);
            if (quality == 1) circ(px2 - r / 5, py - r / 5, (r * 2) / 3, add_col(c, 0x00222222));
        }
        int hx = (int)((s->x - camx) * sc + hw), hy = (int)((s->y - camy) * sc + hh);
        float pxn = -sinf(s->ang), pyn = cosf(s->ang);
        int eoff = (int)(r * 0.55f);
        for (int e = -1; e <= 1; e += 2) {
            int ex = hx + (int)(cosf(s->ang) * r * 0.55f + pxn * eoff * e);
            int ey = hy + (int)(sinf(s->ang) * r * 0.55f + pyn * eoff * e);
            circ(ex, ey, r / 2 + 1, 0x00FFFFFF);
            circ(ex + (int)(cosf(s->ang) * 2), ey + (int)(sinf(s->ang) * 2), r / 4, 0x00000000);
        }
    }

    char buf;
    rect(12, 12, 220, 44, 0x000A0A1E);
    rect(12, 12, 220, 2, 0x0000F0FF);
    sprintf(buf, "SCORE %d", sn.score);
    txt(buf, 22, 20, 3, 0x00FFE600);
    sprintf(buf, "LEN %d", sn.len);
    txt(buf, 22, 38, 2, 0x00FFFFFF);

    int ord[NSN];
    for (int i = 0; i < NSN; i++) ord[i] = i;
    for (int a = 0; a < NSN; a++)
        for (int b = a + 1; b < NSN; b++)
            if (sn[ord[b]].score > sn[ord[a]].score) { int tmp = ord[a]; ord[a] = ord[b]; ord[b] = tmp; }

    rect(W - 230, 12, 218, NSN * 18 + 22, 0x000A0A1E);
    rect(W - 230, 12, 218, 2, 0x0000F0FF);
    txt("RANKING", W - 220, 18, 3, 0x0000F0FF);
    for (int k = 0; k < NSN; k++) {
        int idx = ord[k];
        u32 cc = (idx == 0) ? 0x0000FF88 : 0x00FFFFFF;
        sprintf(buf, "%d %s %d", k + 1, sn[idx].name, sn[idx].score);
        txt(buf, W - 220, 36 + k * 18, 2, cc);
    }

    txt(quality ? "TRIANGULO HD" : "TRIANGULO LOW", 14, H - 24, 2, 0x00888888);

    int mx = W - 90, my = H - 90, mr = 72;
    circ(mx, my, mr + 2, 0x0000F0FF);
    circ(mx, my, mr, 0x000A0A1E);
    for (int k = 0; k < NPEL; k += 6) {
        if (!pel[k].on) continue;
        int dx = (int)(pel[k].x / WORLD * 2 * mr - mr), dy = (int)(pel[k].y / WORLD * 2 * mr - mr);
        if (dx * dx + dy * dy < mr * mr) rect(mx + dx, my + dy, 2, 2, pel[k].col);
    }
    for (int i = 0; i < NSN; i++) {
        if (!sn[i].alive) continue;
        int dx = (int)(sn[i].x / WORLD * 2 * mr - mr), dy = (int)(sn[i].y / WORLD * 2 * mr - mr);
        int rr = (i == 0) ? 4 : 3;
        u32 cc = (i == 0) ? 0x0000FF88 : sn[i].col;
        circ(mx + dx, my + dy, rr, cc);
    }
    rect(mx - 2, my - 2, 5, 5, 0x00FFFFFF);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_screen();
    ioPadInit(7);
    sysUtilRegisterCallback(0, cb, NULL);
    srand(12345);
    init_game();
    while (running) {
        sysUtilCheckCallback();
        readpad();
        update();
        render();
        present();
    }
    ioPadEnd();
    return 0;
}
