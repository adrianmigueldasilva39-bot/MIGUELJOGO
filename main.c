#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <tiny3d.h>
#include <io/pad.h>

#define WORLD 3600.0f
#define MAXSEG 300
#define NSN 8
#define NPEL 900
#define PI 3.14159265f
#define NTR 140
#define TEX 64

typedef struct { float x[MAXSEG], y[MAXSEG]; int len, alive, respawn, score, kills; float ang, tgt; u32 col, col2; char name[10]; } Snake;
typedef struct { float x, y; int on; u32 col; float size; } Pellet;
typedef struct { float x, y; int life; u32 col; } Trail;

static volatile int running = 1;
static Snake sn[NSN];
static Pellet pel[NPEL];
static Trail tr[NTR];
static int tri = 0;
static int frame = 0, dead = 0, quality = 1;
static float camx = 1800, camy = 1800, zoom = 1.0f, zoomT = 1.0f;
static float ax = 0, ay = 0, mag = 0;
static int bx = 0, px = 0, ptri = 0, xp = 0, tprev = 0;
static int W = 1280, H = 720;

static u32 pal[8] = {0xFF2E63FF, 0x00F0FFFF, 0x00FF88FF, 0xFFE600FF, 0xB967FFFF, 0xFF6BCBFF, 0x00FFAAFF, 0xFF7A00FF};
static u32 pal2[8] = {0xFF6BCBFF, 0x00FF88FF, 0xFFE600FF, 0xFF2E63FF, 0x00F0FFFF, 0xB967FFFF, 0xFF2E63FF, 0x00FF88FF};
static char *bnames[7] = {"BOB","ZE","LIA","TURBO","RAFA","NINA","KAKA"};

static u32 *texGlow = NULL;
static u32 *texBody = NULL;
static u32 *texEye = NULL;

static void cb(u64 s, u64 p, void *u) { (void)p; (void)u; if (s == SYSUTIL_EXIT_GAME) running = 0; }

/* ---------- TEXTURAS PROCEDURAIS ---------- */
/* ARGB: A no byte alto. Tiny3D espera A8R8G8B8 */
static u32 make_argb(int a, int r, int g, int b) { return ((u32)(a & 255) << 24) | ((u32)(r & 255) << 16) | ((u32)(g & 255) << 8) | (u32)(b & 255); }

static void gen_textures(void) {
    int x, y;
    texGlow = (u32*)malloc(TEX * TEX * 4);
    texBody = (u32*)malloc(TEX * TEX * 4);
    texEye  = (u32*)malloc(TEX * TEX * 4);
    for (y = 0; y < TEX; y++) {
        for (x = 0; x < TEX; x++) {
            float dx = (x - 32.0f) / 32.0f;
            float dy = (y - 32.0f) / 32.0f;
            float d = sqrtf(dx*dx + dy*dy);
            int idx = y * TEX + x;
            /* GLOW: gradiente radial suave, alpha cai pra 0 na borda */
            if (d <= 1.0f) {
                float a = 1.0f - d;
                a = a * a;
                texGlow[idx] = make_argb((int)(a * 255.0f), 255, 255, 255);
            } else texGlow[idx] = 0;
            /* BODY: esfera com volume, centro claro, borda escura */
            if (d <= 1.0f) {
                float shade = 1.0f - d * 0.55f;
                float hi = 1.0f - d;
                hi = hi * hi;
                int v = (int)(200.0f * shade + 55.0f * hi);
                if (v > 255) v = 255;
                texBody[idx] = make_argb(255, v, v, v);
            } else texBody[idx] = 0;
            /* EYE: branco com pupila */
            if (d <= 1.0f) {
                if (d < 0.45f) texEye[idx] = make_argb(255, 10, 10, 10);
                else if (d < 0.55f) texEye[idx] = make_argb(255, 40, 40, 40);
                else texEye[idx] = make_argb(255, 255, 255, 255);
            } else texEye[idx] = 0;
        }
    }
}

/* ---------- DRAW HELPERS TINY3D ---------- */
static void draw_quad(float x0, float y0, float x1, float y1, u32 color) {
    tiny3d_DrawQuad(x0, y0, x1, y0, x1, y1, x0, y1, color);
}

static void draw_sprite(float cx, float cy, float size, u32 *tex, u32 tint) {
    float h = size * 0.5f;
    tiny3d_SetTexture(0, tex, TEX, TEX, TEX * 4, TINY3D_TEX_FORMAT_A8R8G8B8, TEXTURE_LINEAR);
    tiny3d_DrawQuadTexture(cx - h, cy - h, 0.0f, 0.0f,
                           cx + h, cy - h, 1.0f, 0.0f,
                           cx + h, cy + h, 1.0f, 1.0f,
                           cx - h, cy + h, 0.0f, 1.0f, tint);
    tiny3d_SetTexture(0, NULL, 0, 0, 0, 0, 0);
}

static void draw_line_quad(float x0, float y0, float x1, float y1, float th, u32 color) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.01f) return;
    float nx = -dy / len * th * 0.5f;
    float ny = dx / len * th * 0.5f;
    tiny3d_DrawQuad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, color);
}

static void draw_hex(float cx, float cy, float r, u32 color) {
    int k;
    for (k = 0; k < 6; k++) {
        float a1 = k * PI / 3.0f;
        float a2 = (k + 1) * PI / 3.0f;
        draw_line_quad(cx + cosf(a1) * r, cy + sinf(a1) * r, cx + cosf(a2) * r, cy + sinf(a2) * r, 1.0f, color);
    }
}

/* Fonte 3x5 com quads */
typedef struct { char c; const char *p; } G;
static const G gl[] = {{'0',"###" "#.#" "#.#" "#.#" "###"},{'1',".#." "##." ".#." ".#." "###"},{'2',"###" "..#" "###" "#.." "###"},{'3',"###" "..#" "###" "..#" "###"},{'4',"#.#" "#.#" "###" "..#" "..#"},{'5',"###" "#.." "###" "..#" "###"},{'6',"###" "#.." "###" "#.#" "###"},{'7',"###" "..#" "..#" "..#" "..#"},{'8',"###" "#.#" "###" "#.#" "###"},{'9',"###" "#.#" "###" "..#" "###"},{'A',".#." "#.#" "###" "#.#" "#.#"},{'B',"##." "#.#" "##." "#.#" "##."},{'C',"###" "#.." "#.." "#.." "###"},{'D',"##." "#.#" "#.#" "#.#" "##."},{'E',"###" "#.." "###" "#.." "###"},{'F',"###" "#.." "##." "#.." "#.."},{'G',"###" "#.." "#.#" "#.#" "###"},{'H',"#.#" "#.#" "###" "#.#" "#.#"},{'I',"###" ".#." ".#." ".#." "###"},{'J',"###" "..#" "..#" "#.#" "###"},{'K',"#.#" "#.#" "##." "#.#" "#.#"},{'L',"#.." "#.." "#.." "#.." "###"},{'M',"#.#" "###" "###" "#.#" "#.#"},{'N',"#.#" "###" "###" "###" "#.#"},{'O',"###" "#.#" "#.#" "#.#" "###"},{'P',"###" "#.#" "###" "#.." "#.."},{'Q',"###" "#.#" "#.#" "###" ".#."},{'R',"##." "#.#" "##." "#.#" "#.#"},{'S',"###" "#.." "###" "..#" "###"},{'T',"###" ".#." ".#." ".#." ".#."},{'U',"#.#" "#.#" "#.#" "#.#" "###"},{'V',"#.#" "#.#" "#.#" "#.#" ".#."},{'W',"#.#" "#.#" "###" "###" "#.#"},{'X',"#.#" "#.#" ".#." "#.#" "#.#"},{'Y',"#.#" "#.#" ".#." ".#." ".#."},{'Z',"###" "..#" ".#." "#.." "###"},{' ',"   " "   " "   " "   " "   "},{'-',"   " "   " "###" "   " "   "},{':',"   " ".#." "..." ".#." "   "},{'.',"   " "   " "   " "   " ".#."}};
static const char *findg(char c) { unsigned i; for (i = 0; i < sizeof(gl)/sizeof(gl[0]); i++) if (gl[i].c == c) return gl[i].p; return 0; }
static void txt(const char *s, int x, int y, int sc, u32 c) {
    for (; *s; s++) {
        const char *g = findg(*s);
        int r, k;
        if (g) for (r = 0; r < 5; r++) for (k = 0; k < 3; k++) if (g[r*3+k] == '#') draw_quad((float)(x + k*sc), (float)(y + r*sc), (float)(x + k*sc + sc), (float)(y + r*sc + sc), c);
        x += 4 * sc;
    }
}

/* ---------- LOGICA ---------- */
static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static float rad(Snake *s) { return 10.0f + s->len * 0.04f; }

static int free_pellet(void) {
    int k;
    for (k = 0; k < NPEL; k++) if (!pel[k].on) return k;
    return rand() % NPEL;
}

static void drop_orbs(Snake *s) {
    int j;
    for (j = 0; j < s->len; j += 2) {
        int k = free_pellet();
        pel[k].x = s->x[j];
        pel[k].y = s->y[j];
        pel[k].on = 1;
        pel[k].size = 7.0f;
        pel[k].col = s->col;
    }
}

static void spawn(int i) {
    Snake *s = &sn[i];
    float x = 400 + frand() * (WORLD - 800);
    float y = 400 + frand() * (WORLD - 800);
    int j, c;
    s->ang = frand() * 2.0f * PI;
    s->tgt = s->ang;
    s->len = 12;
    s->alive = 1;
    s->respawn = 0;
    s->score = 0;
    s->kills = 0;
    for (j = 0; j < s->len; j++) { s->x[j] = x - cosf(s->ang) * 7.0f * j; s->y[j] = y - sinf(s->ang) * 7.0f * j; }
    c = rand() % 8;
    if (i == 0) { s->col = pal[1]; s->col2 = pal2[1]; strcpy(s->name, "VOCE"); }
    else { s->score = rand() % 800; s->kills = rand() % 5; s->col = pal[c]; s->col2 = pal2[c]; strcpy(s->name, bnames[(i-1)%7]); }
}

static void kill_snake(int i, int killer) {
    Snake *s = &sn[i];
    drop_orbs(s);
    if (i == 0) dead = 0;
    else if (killer == 0) { sn[0].kills++; sn[0].score += 150; }
    s->alive = 0;
    s->respawn = 180 + rand() % 100;
}

static void init_game(void) {
    int k, i;
    for (k = 0; k < NTR; k++) tr[k].life = 0;
    for (k = 0; k < NPEL; k++) {
        pel[k].x = frand() * WORLD;
        pel[k].y = frand() * WORLD;
        pel[k].on = 1;
        pel[k].size = 5.0f + frand() * 3.0f;
        pel[k].col = pal[rand() % 8];
    }
    for (i = 0; i < NSN; i++) spawn(i);
    dead = 0;
}

static void readpad(void) {
    padInfo info; padData d;
    ax = ay = mag = 0; bx = px = ptri = 0;
    ioPadGetInfo(&info);
    if (info.status[0]) {
        ioPadGetData(0, &d);
        float a = ((int)d.ANA_L_H - 128) / 128.0f;
        float b = ((int)d.ANA_L_V - 128) / 128.0f;
        if (d.BTN_LEFT) a = -1; if (d.BTN_RIGHT) a = 1;
        if (d.BTN_UP) b = -1; if (d.BTN_DOWN) b = 1;
        ax = a; ay = b; mag = sqrtf(a*a + b*b);
        bx = d.BTN_CROSS || d.BTN_R1;
        px = d.BTN_CROSS;
        ptri = d.BTN_TRIANGLE;
    }
}

static void update(void) {
    int i, j, k, o, t;
    frame++;
    if (ptri && !tprev) { quality = quality ? 0 : 1; }
    tprev = ptri;
    for (t = 0; t < NTR; t++) if (tr[t].life > 0) tr[t].life--;

    /* Zoom dinamico: cresce -> afasta */
    zoomT = 1.0f - fminf(0.35f, sn[0].len * 0.0012f);
    zoom += (zoomT - zoom) * 0.05f;

    for (i = 0; i < NSN; i++) {
        Snake *s = &sn[i];
        if (!s->alive) { if (--s->respawn <= 0) spawn(i); continue; }
        if (i == 0) { if (mag > 0.3f) s->tgt = atan2f(ay, ax); }
        else {
            if ((frame % 20) == 0) {
                float best = 99999999.0f; int bi = -1;
                for (k = 0; k < NPEL; k += 3) {
                    if (!pel[k].on) continue;
                    float dx = pel[k].x - s->x[0], dy = pel[k].y - s->y[0], d = dx*dx + dy*dy;
                    if (d < best) { best = d; bi = k; }
                }
                if (bi >= 0) s->tgt = atan2f(pel[bi].y - s->y[0], pel[bi].x - s->x[0]);
            }
            if (s->x[0] < 300 || s->x[0] > WORLD-300 || s->y[0] < 300 || s->y[0] > WORLD-300) s->tgt = atan2f(WORLD/2 - s->y[0], WORLD/2 - s->x[0]);
        }
        float d = s->tgt - s->ang;
        d = fmodf(d + PI, 2.0f * PI);
        if (d < 0) d += 2.0f * PI;
        d -= PI;
        if (d > 0.08f) d = 0.08f;
        if (d < -0.08f) d = -0.08f;
        s->ang += d;

        int turbo = 0;
        if (i == 0 && bx && s->len > 12) turbo = 1;
        if (i != 0 && rand() % 200 == 0) turbo = 1;
        float sp = turbo ? 5.2f : 3.1f;
        if (turbo && i == 0 && (frame % 15) == 0 && s->len > 12) s->len--;
        s->x[0] += cosf(s->ang) * sp;
        s->y[0] += sinf(s->ang) * sp;
        if (turbo) {
            tr[tri].x = s->x[0] - cosf(s->ang) * 22.0f;
            tr[tri].y = s->y[0] - sinf(s->ang) * 22.0f;
            tr[tri].life = 18;
            tr[tri].col = s->col;
            tri = (tri + 1) % NTR;
        }
        for (j = 1; j < s->len; j++) {
            float dx = s->x[j-1] - s->x[j], dy = s->y[j-1] - s->y[j], dd = dx*dx + dy*dy;
            if (dd > 49.0f) { dd = sqrtf(dd); float f = (dd - 7.0f) / dd; s->x[j] += dx * f; s->y[j] += dy * f; }
        }
        if (s->x[0] < 0 || s->x[0] > WORLD || s->y[0] < 0 || s->y[0] > WORLD) { kill_snake(i, -1); continue; }
        float rs = rad(s), er = rs + 14.0f;
        for (k = 0; k < NPEL; k++) {
            if (!pel[k].on) continue;
            float dx = pel[k].x - s->x[0], dy = pel[k].y - s->y[0];
            if (dx*dx + dy*dy < er*er) {
                pel[k].on = 0;
                s->score += 10;
                if (s->len < MAXSEG) { s->x[s->len] = s->x[s->len-1]; s->y[s->len] = s->y[s->len-1]; s->len++; }
                if (rand() % 3 == 0) {
                    int idx = free_pellet();
                    pel[idx].x = frand() * WORLD;
                    pel[idx].y = frand() * WORLD;
                    pel[idx].on = 1;
                    pel[idx].size = 5.0f + frand() * 3.0f;
                    pel[idx].col = pal[rand() % 8];
                }
            }
        }
        for (o = 0; o < NSN; o++) {
            if (o == i || !sn[o].alive) continue;
            float lim = (rs + rad(&sn[o])) * 0.75f; lim *= lim;
            for (j = 0; j < sn[o].len; j += 3) {
                float dx = sn[o].x[j] - s->x[0], dy = sn[o].y[j] - s->y[0];
                if (dx*dx + dy*dy < lim) { kill_snake(i, o); break; }
            }
            if (!s->alive) break;
        }
    }
    if (sn[0].alive) { camx = sn[0].x[0]; camy = sn[0].y[0]; }
    else { dead++; if (dead > 60 && px && !xp) spawn(0); }
    xp = px;
}

/* ---------- RENDER GPU ---------- */
static float world_to_screen_x(float wx) { return (wx - camx) * zoom + (W * 0.5f); }
static float world_to_screen_y(float wy) { return (wy - camy) * zoom + (H * 0.5f); }

static void render(void) {
    int gx, gy, k, i, j, t;
    tiny3d_Clear(0xFF050512, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();

    /* Fundo + grade hexagonal */
    draw_quad(0, 0, W, H, 0xFF050512);
    {
        int hs = quality ? 110 : 200;
        float stepX = hs * 1.5f * zoom;
        float stepY = hs * 1.73f * zoom;
        float offX = fmodf(camx * zoom, stepX);
        float offY = fmodf(camy * zoom, stepY);
        for (gy = -1; gy < (H / (int)stepY) + 2; gy++) {
            for (gx = -1; gx < (W / (int)stepX) + 2; gx++) {
                float ox = gx * stepX - offX;
                float oy = gy * stepY - offY + ((gx % 2) ? hs * 0.86f * zoom : 0);
                draw_hex(ox, oy, hs * 0.35f * zoom, 0xFF0D0D22);
            }
        }
    }

    /* Borda arena */
    {
        float bx0 = world_to_screen_x(0), by0 = world_to_screen_y(0);
        float bx1 = world_to_screen_x(WORLD), by1 = world_to_screen_y(WORLD);
        draw_quad(bx0, by0, bx1, by0 + 3, 0xFF00F0FF);
        draw_quad(bx0, by1, bx1, by1 + 3, 0xFF00F0FF);
        draw_quad(bx0, by0, bx0 + 3, by1, 0xFF00F0FF);
        draw_quad(bx1, by0, bx1 + 3, by1, 0xFF00F0FF);
    }

    /* Rastro turbo - glow */
    for (t = 0; t < NTR; t++) {
        if (tr[t].life <= 0) continue;
        float px2 = world_to_screen_x(tr[t].x), py = world_to_screen_y(tr[t].y);
        float a = tr[t].life / 18.0f;
        u32 tint = ((u32)(a * 180) << 24) | tr[t].col;
        if (quality) draw_sprite(px2, py, 34.0f * zoom, texGlow, tint);
        draw_sprite(px2, py, 14.0f * zoom, texGlow, tint);
    }

    /* Comida - orbe com glow */
    for (k = 0; k < NPEL; k++) {
        if (!pel[k].on) continue;
        float px2 = world_to_screen_x(pel[k].x), py = world_to_screen_y(pel[k].y);
        if (px2 < -40 || px2 > W + 40 || py < -40 || py > H + 40) continue;
        float pulse = 1.0f + 0.15f * sinf(frame * 0.15f + k);
        float sz = pel[k].size * 2.0f * pulse * zoom;
        if (quality) draw_sprite(px2, py, sz * 3.2f, texGlow, 0x80000000 | pel[k].col);
        draw_sprite(px2, py, sz * 2.2f, texGlow, 0xCC000000 | pel[k].col);
        draw_sprite(px2, py, sz, texBody, 0xFFFFFFFF | (pel[k].col & 0x00FFFFFF));
        /* brilho central */
        draw_sprite(px2, py, sz * 0.4f, texGlow, 0xFFFFFFFF);
    }

    /* Cobras - de tras pra frente, esferas sobrepostas */
    for (i = NSN - 1; i >= 0; i--) {
        Snake *s = &sn[i];
        if (!s->alive) continue;
        float r = rad(s) * zoom;
        for (j = s->len - 1; j >= 0; j--) {
            float px2 = world_to_screen_x(s->x[j]), py = world_to_screen_y(s->y[j]);
            if (px2 < -r - 30 || px2 > W + r + 30 || py < -r - 30 || py > H + r + 30) continue;
            u32 c = (j % 4 == 0) ? s->col2 : s->col;
            if (quality) draw_sprite(px2, py, r * 2.8f, texGlow, 0x55000000 | c);
            draw_sprite(px2, py, r * 2.0f, texBody, 0xFF000000 | c);
        }
        /* Cabeca + olhos grandes que giram */
        float hx = world_to_screen_x(s->x[0]), hy = world_to_screen_y(s->y[0]);
        float pxn = -sinf(s->ang), pyn = cosf(s->ang);
        float eoff = r * 0.55f;
        int e;
        draw_sprite(hx, hy, r * 2.2f, texBody, 0xFF000000 | s->col);
        for (e = -1; e <= 1; e += 2) {
            float ex = hx + cosf(s->ang) * r * 0.6f + pxn * eoff * e;
            float ey = hy + sinf(s->ang) * r * 0.6f + pyn * eoff * e;
            draw_sprite(ex, ey, r * 1.15f, texEye, 0xFFFFFFFF);
        }
    }

    /* HUD - cantos */
    draw_quad(12, 12, 232, 54, 0xCC0A0A1E);
    draw_quad(12, 12, 232, 14, 0xFF00F0FF);
    {
        char buf[64];
        sprintf(buf, "SCORE %d", sn[0].score);
        txt(buf, 22, 22, 3, 0xFFFFE600);
        sprintf(buf, "LEN %d", sn[0].len);
        txt(buf, 22, 38, 2, 0xFFFFFFFF);
    }

    /* Ranking */
    {
        int ord[NSN]; int a, b;
        for (a = 0; a < NSN; a++) ord[a] = a;
        for (a = 0; a < NSN; a++) for (b = a + 1; b < NSN; b++) if (sn[ord[b]].score > sn[ord[a]].score) { int tmp = ord[a]; ord[a] = ord[b]; ord[b] = tmp; }
        draw_quad(W - 230, 12, W - 12, 12 + NSN * 18 + 22, 0xCC0A0A1E);
        draw_quad(W - 230, 12, W - 12, 14, 0xFF00F0FF);
        txt("RANKING", W - 220, 18, 3, 0xFF00F0FF);
        for (k = 0; k < NSN; k++) {
            int idx = ord[k]; char buf[64]; u32 cc = 0xFFFFFFFF;
            if (idx == 0) cc = 0xFF00FF88;
            sprintf(buf, "%d %s %d", k + 1, sn[idx].name, sn[idx].score);
            txt(buf, W - 220, 36 + k * 18, 2, cc);
        }
    }

    if (quality) txt("TRIANGULO HD", 14, H - 24, 2, 0xFF888888);
    else txt("TRIANGULO LOW", 14, H - 24, 2, 0xFF888888);

    /* Minimapa circular */
    {
        int mx = W - 90, my = H - 90, mr = 72;
        draw_sprite((float)mx, (float)my, (float)(mr * 2 + 8), texGlow, 0xFF00F0FF);
        draw_sprite((float)mx, (float)my, (float)(mr * 2), texBody, 0xFF0A0A1E);
        for (k = 0; k < NPEL; k += 8) {
            if (!pel[k].on) continue;
            int dx = (int)(pel[k].x / WORLD * 2 * mr - mr);
            int dy = (int)(pel[k].y / WORLD * 2 * mr - mr);
            if (dx*dx + dy*dy < mr*mr) draw_quad(mx + dx, my + dy, mx + dx + 2, my + dy + 2, 0xFF000000 | pel[k].col);
        }
        for (i = 0; i < NSN; i++) {
            if (!sn[i].alive) continue;
            int dx = (int)(sn[i].x[0] / WORLD * 2 * mr - mr);
            int dy = (int)(sn[i].y[0] / WORLD * 2 * mr - mr);
            float sz = (i == 0) ? 8.0f : 6.0f;
            u32 cc = (i == 0) ? 0xFF00FF88 : (0xFF000000 | sn[i].col);
            draw_sprite((float)(mx + dx), (float)(my + dy), sz, texBody, cc);
        }
        draw_quad(mx - 2, my - 2, mx + 3, my + 3, 0xFFFFFFFF);
    }

    tiny3d_Flip();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    videoState st;
    videoGetState(VIDEO_PRIMARY, 0, &st);
    videoResolution res;
    videoGetResolution(VIDEO_RESOLUTION_720, &res);
    W = res.width; H = res.height;

    ioPadInit(7);
    sysUtilRegisterCallback(0, cb, NULL);
    tiny3d_Init(1024 * 1024);
    gen_textures();
    srand(12345);
    init_game();

    while (running) {
        sysUtilCheckCallback();
        readpad();
        update();
        render();
    }
    return 0;
}