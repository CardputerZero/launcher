// Web (Emscripten) version of the emulator
// Differences from native:
// - No dlopen — app is statically linked
// - Uses emscripten_set_main_loop instead of while(true)
// - SDL2 provided by Emscripten's port

#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <emscripten.h>

extern "C" { void ui_init(void); }

// APPLaunch's main.cpp defines LV_EVENT_BATTERY, but the web target
// uses this main_web.cpp instead, so the symbol would be undefined.
volatile uint32_t LV_EVENT_BATTERY;

// ── Layout (same as native) ─────────────────────────────────────
static constexpr int SKIN_W = 1280;
static constexpr int SKIN_H = 840;
static constexpr int LCD_SX = 323;
static constexpr int LCD_SY = 60;
static constexpr int LCD_SW = 639;
static constexpr int LCD_SH = 339;
static constexpr int LCD_W  = 320;
static constexpr int LCD_H  = 170;

struct KeyRect { int x, y, w, h; SDL_Keycode key; };
static KeyRect g_keys[4][11] = {
    {{49,461,70,41,SDLK_1},{160,461,71,42,SDLK_2},{272,461,71,41,SDLK_3},{384,461,71,41,SDLK_4},
     {495,461,71,41,SDLK_5},{608,461,69,41,SDLK_6},{718,461,71,42,SDLK_7},{830,461,71,42,SDLK_8},
     {942,461,70,42,SDLK_9},{1054,461,71,41,SDLK_0},{1166,461,70,42,SDLK_BACKSPACE}},
    {{49,558,71,42,SDLK_TAB},{160,558,71,42,SDLK_q},{272,558,71,42,SDLK_w},{384,558,70,42,SDLK_e},
     {495,558,71,42,SDLK_r},{608,558,69,42,SDLK_t},{718,558,71,42,SDLK_y},{830,558,71,42,SDLK_u},
     {942,558,70,42,SDLK_i},{1054,558,71,42,SDLK_o},{1166,558,70,42,SDLK_p}},
    {{49,655,70,41,SDLK_LSHIFT},{160,655,71,41,SDLK_a},{272,655,71,41,SDLK_s},{384,655,70,41,SDLK_d},
     {495,655,71,41,SDLK_f},{608,655,69,41,SDLK_g},{718,655,71,41,SDLK_h},{830,655,71,41,SDLK_j},
     {942,655,70,41,SDLK_k},{1054,655,71,42,SDLK_l},{1166,655,70,41,SDLK_RETURN}},
    {{49,752,70,41,SDLK_ESCAPE},{160,752,71,41,SDLK_LCTRL},{272,752,71,41,SDLK_LALT},
     {384,752,70,41,SDLK_z},{495,752,71,41,SDLK_x},{608,752,69,41,SDLK_c},
     {718,752,71,41,SDLK_v},{830,752,71,41,SDLK_b},{942,752,70,41,SDLK_n},
     {1054,752,71,41,SDLK_m},{1166,752,70,41,SDLK_SPACE}},
};

// Side buttons + POWER
static constexpr int SIDE_POWER = 4;
static constexpr int NUM_SIDE_KEYS = 5;
static KeyRect g_side_keys[NUM_SIDE_KEYS] = {
    {49,  365, 75, 45, SDLK_ESCAPE},
    {158, 365, 75, 45, SDLK_HOME},
    {1058,365, 75, 45, SDLK_F3},
    {1166,365, 75, 45, SDLK_TAB},
    {1080, 40, 100, 60, SDLK_POWER},
};
static int g_side_pr = -1;

static int hit_side(int mx, int my) {
    for(int i=0;i<NUM_SIDE_KEYS;i++){
        auto &k=g_side_keys[i];
        if(mx>=k.x&&mx<k.x+k.w&&my>=k.y&&my<k.y+k.h) return i;
    }
    return -1;
}

// Modifiers
#define MOD_SYM_R 1
#define MOD_SYM_C 0
#define MOD_AA_R  2
#define MOD_AA_C  0
#define MOD_FN_R  3
#define MOD_FN_C  0
#define MOD_CTRL_R 3
#define MOD_CTRL_C 1
#define MOD_ALT_R  3
#define MOD_ALT_C  2

static bool g_mod_sym=false, g_mod_aa=false, g_mod_fn=false, g_mod_ctrl=false, g_mod_alt=false;
static int g_pr = -1, g_pc = -1;
static SDL_Window   *g_win = nullptr;
static SDL_Renderer *g_ren = nullptr;
static SDL_Texture  *g_skin_tex = nullptr;
static SDL_Texture  *g_lcd_tex = nullptr;
static uint32_t     *g_lcd_buf = nullptr;

typedef void (*sdl_kbd_handler_fn)(SDL_Event *);
static sdl_kbd_handler_fn g_kbd_handler = nullptr;

static bool is_modifier(int r, int c) {
    return (r==MOD_SYM_R&&c==MOD_SYM_C)||(r==MOD_AA_R&&c==MOD_AA_C)||
           (r==MOD_FN_R&&c==MOD_FN_C)||(r==MOD_CTRL_R&&c==MOD_CTRL_C)||
           (r==MOD_ALT_R&&c==MOD_ALT_C);
}
static void toggle_modifier(int r, int c) {
    if(r==MOD_SYM_R&&c==MOD_SYM_C) g_mod_sym=!g_mod_sym;
    if(r==MOD_AA_R&&c==MOD_AA_C) g_mod_aa=!g_mod_aa;
    if(r==MOD_FN_R&&c==MOD_FN_C) g_mod_fn=!g_mod_fn;
    if(r==MOD_CTRL_R&&c==MOD_CTRL_C) g_mod_ctrl=!g_mod_ctrl;
    if(r==MOD_ALT_R&&c==MOD_ALT_C) g_mod_alt=!g_mod_alt;
}

static bool hit_key(int mx, int my, int *r, int *c) {
    for(int i=0;i<4;i++) for(int j=0;j<11;j++) {
        auto &k=g_keys[i][j];
        if(mx>=k.x&&mx<k.x+k.w&&my>=k.y&&my<k.y+k.h){*r=i;*c=j;return true;}
    }
    return false;
}

static void flush_cb(lv_display_t*, const lv_area_t *area, uint8_t *px) {
    int32_t w=lv_area_get_width(area), h=lv_area_get_height(area);
    uint16_t *src=(uint16_t*)px;
    for(int32_t y=0;y<h;y++) for(int32_t x=0;x<w;x++) {
        uint16_t c=src[y*w+x];
        g_lcd_buf[(area->y1+y)*LCD_W+area->x1+x]=0xFF000000
            |(((c>>11&0x1F)<<3|(c>>11&0x1F)>>2)<<16)
            |(((c>>5&0x3F)<<2|(c>>5&0x3F)>>4)<<8)
            |((c&0x1F)<<3|(c&0x1F)>>2);
    }
    lv_display_flush_ready(lv_display_get_default());
}

static void inject_sdl_key(SDL_Keycode key, bool down) {
    SDL_Event ev={};
    ev.type=down?SDL_KEYDOWN:SDL_KEYUP;
    ev.key.keysym.sym=key;
    ev.key.keysym.scancode=SDL_GetScancodeFromKey(key);
    ev.key.state=down?SDL_PRESSED:SDL_RELEASED;
    if(g_kbd_handler) g_kbd_handler(&ev);
    if(down&&key>=0x20&&key<0x7f&&g_kbd_handler){
        SDL_Event te={};te.type=SDL_TEXTINPUT;
        te.text.text[0]=(char)key;te.text.text[1]='\0';
        g_kbd_handler(&te);
    }
}

static void draw_key_hl(int r,int c,uint8_t cr,uint8_t cg,uint8_t cb,uint8_t ca){
    auto &k=g_keys[r][c];
    SDL_SetRenderDrawBlendMode(g_ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_ren,cr,cg,cb,ca);
    SDL_Rect kr={k.x,k.y,k.w,k.h};
    SDL_RenderFillRect(g_ren,&kr);
}

static void render() {
    // Match web page background (#1a1a2e) so skin corners blend
    SDL_SetRenderDrawColor(g_ren, 0x1a, 0x1a, 0x2e, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren,g_skin_tex,nullptr,nullptr);
    SDL_UpdateTexture(g_lcd_tex,nullptr,g_lcd_buf,LCD_W*4);
    SDL_Rect lcd_dst={LCD_SX,LCD_SY,LCD_SW,LCD_SH};
    SDL_RenderCopy(g_ren,g_lcd_tex,nullptr,&lcd_dst);
    SDL_RenderCopy(g_ren,g_skin_tex,nullptr,nullptr);
    if(g_mod_sym)  draw_key_hl(MOD_SYM_R,MOD_SYM_C,0,170,85,120);
    if(g_mod_aa)   draw_key_hl(MOD_AA_R,MOD_AA_C,180,50,220,120);
    if(g_mod_fn)   draw_key_hl(MOD_FN_R,MOD_FN_C,238,153,0,120);
    if(g_mod_ctrl) draw_key_hl(MOD_CTRL_R,MOD_CTRL_C,50,120,255,120);
    if(g_mod_alt)  draw_key_hl(MOD_ALT_R,MOD_ALT_C,220,220,0,120);
    if(g_pr>=0&&!is_modifier(g_pr,g_pc)) draw_key_hl(g_pr,g_pc,255,50,50,100);
    if(g_side_pr>=0){
        auto &k=g_side_keys[g_side_pr];
        SDL_SetRenderDrawBlendMode(g_ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_ren,255,50,50,100);
        SDL_Rect kr={k.x,k.y,k.w,k.h};
        SDL_RenderFillRect(g_ren,&kr);
    }
    SDL_RenderPresent(g_ren);
}

static void main_loop() {
    SDL_Event ev;
    while(SDL_PollEvent(&ev)) {
        if(ev.type==SDL_MOUSEBUTTONDOWN) {
            int side=hit_side(ev.button.x,ev.button.y);
            if(side==SIDE_POWER){
                g_side_pr=side;
                int confirmed = EM_ASM_INT({ return confirm("Reset the emulator?") ? 1 : 0; });
                if(confirmed){
                    printf("[EMU] POWER — reset\n");
                    memset(g_lcd_buf,0,LCD_W*LCD_H*sizeof(uint32_t));
                    ui_init();
                }
                g_side_pr=-1;
            } else if(side>=0){g_side_pr=side;inject_sdl_key(g_side_keys[side].key,true);}
            else{int r,c;
            if(hit_key(ev.button.x,ev.button.y,&r,&c)){
                if(is_modifier(r,c)) toggle_modifier(r,c);
                else{g_pr=r;g_pc=c;inject_sdl_key(g_keys[r][c].key,true);}
            }}
        } else if(ev.type==SDL_MOUSEBUTTONUP) {
            if(g_side_pr>=0){inject_sdl_key(g_side_keys[g_side_pr].key,false);g_side_pr=-1;}
            else if(g_pr>=0){inject_sdl_key(g_keys[g_pr][g_pc].key,false);g_pr=-1;}
        } else if(ev.type==SDL_KEYDOWN||ev.type==SDL_KEYUP||ev.type==SDL_TEXTINPUT) {
            if(g_kbd_handler) g_kbd_handler(&ev);
        }
    }
    lv_tick_inc(16);
    lv_timer_handler();
    render();
}

// These are provided by the statically linked app
extern "C" {
    lv_indev_t *lv_sdl_keyboard_create(void);
    void lv_sdl_keyboard_handler(SDL_Event *event);
}

int main(int, char*[]) {
    printf("M5CardputerZero Emulator (Web)\n");

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // Use skin native resolution for crisp rendering
    g_win=SDL_CreateWindow("M5CardputerZero",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        SKIN_W,SKIN_H,SDL_WINDOW_SHOWN);
    g_ren=SDL_CreateRenderer(g_win,-1,SDL_RENDERER_ACCELERATED);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    SDL_Surface *surf=IMG_Load("assets/device_skin.png");
    if(!surf){printf("skin load failed: %s\n",IMG_GetError());return 1;}
    g_skin_tex=SDL_CreateTextureFromSurface(g_ren,surf);
    SDL_SetTextureBlendMode(g_skin_tex,SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surf);

    g_lcd_tex=SDL_CreateTexture(g_ren,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,LCD_W,LCD_H);
    g_lcd_buf=(uint32_t*)calloc(LCD_W*LCD_H,sizeof(uint32_t));

    lv_init();
    static uint8_t draw_buf[LCD_W*LCD_H*2];
    lv_display_t *disp=lv_display_create(LCD_W,LCD_H);
    lv_display_set_flush_cb(disp,flush_cb);
    lv_display_set_buffers(disp,draw_buf,nullptr,sizeof(draw_buf),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp,LV_COLOR_FORMAT_RGB565);

    // Static link — call directly
    g_kbd_handler = lv_sdl_keyboard_handler;
    lv_sdl_keyboard_create();

    ui_init();
    printf("[EMU] Running in browser.\n");

    emscripten_set_main_loop(main_loop, 0, 1);
    return 0;
}
