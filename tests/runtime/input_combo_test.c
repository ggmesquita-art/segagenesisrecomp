/* Combos, autofire and pad_hotkeys: pure resolution, key-origin combo through
 * the real input_current_mask() (keyboard mocked), and settings.ini round trip. */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "input_map.h"
#include "gamepad.h"
#include "app_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Uint8 keyboard[SDL_NUM_SCANCODES];
static const Uint8 *test_keyboard_state(int *count)
{
    if (count) *count = SDL_NUM_SCANCODES;
    return keyboard;
}
#define SDL_GetKeyboardState test_keyboard_state
#include "input_map.c"
#undef SDL_GetKeyboardState
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

#define BIT(b) input_button_bit(b)

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *path = argv[1];

    /* 1. Pure resolution. */
    PlayerInput pi; memset(&pi, 0, sizeof pi);
    pi.combo[0].kind = COMBO_SRC_KEY; pi.combo[0].code = 44;
    pi.combo[0].dest = BIT(GB_X) | BIT(GB_A);
    CHECK(input_map_apply_combos(&pi, 0, 0, 0) == 0);
    CHECK(input_map_apply_combos(&pi, BIT(GB_UP), 1, 0) == (BIT(GB_UP) | BIT(GB_X) | BIT(GB_A)));
    /* autofire 15 Hz -> half period 2 frames: on,on,off,off,... */
    pi.autofire_hz[GB_B] = 15;
    int expect[8] = { 1, 1, 0, 0, 1, 1, 0, 0 };
    for (uint32_t f = 0; f < 8; f++) {
        uint16_t m = input_map_apply_combos(&pi, BIT(GB_B) | BIT(GB_C), 0, f);
        CHECK(((m & BIT(GB_B)) != 0) == expect[f]);
        CHECK(m & BIT(GB_C));                    /* unrelated button untouched */
        CHECK(input_map_apply_combos(&pi, 0, 0, f) == 0);  /* released stays released */
    }
    pi.autofire_hz[GB_B] = 60;                  /* clamps to 1-frame half period */
    CHECK(input_map_apply_combos(&pi, BIT(GB_B), 0, 0) == BIT(GB_B));
    CHECK(input_map_apply_combos(&pi, BIT(GB_B), 0, 1) == 0);

    /* 2. ini parsing: tolerant spaces, comments, invalid entries ignored. */
    FILE *f = fopen(path, "wb");
    CHECK(f);
    fputs("[input]\npad_hotkeys = 0\n"
          "[input.p1]\ndevice = 3\npad_type = 1\ndeadzone = 25\n"
          "combo.1 =  pad:button:10  ->  a + b+c   ; 3 kicks\n"
          "combo.2 = pad:axis:5:+ -> x+y+z\n"
          "combo.3 = key:44 -> x+a\n"
          "combo.4 = bogus:1 -> a\n"
          "combo.5 = key:44 -> nothing\n"
          "combo.9 = key:44 -> a\n"
          "autofire.x = 15\nautofire.A = 30\nautofire.zz = 5\n", f);
    fclose(f);
    input_map_init_defaults(); app_config_defaults();
    CHECK(g_input_map.pad_hotkeys == 1);
    CHECK(app_config_load(path));
    const PlayerInput *p1 = &g_input_map.p[0];
    CHECK(g_input_map.pad_hotkeys == 0);
    CHECK(p1->combo[0].kind == COMBO_SRC_PAD_BUTTON && p1->combo[0].code == 10);
    CHECK(p1->combo[0].dest == (BIT(GB_A) | BIT(GB_B) | BIT(GB_C)));
    CHECK(p1->combo[1].kind == COMBO_SRC_PAD_AXIS && p1->combo[1].code == 5 && p1->combo[1].axis_dir == 1);
    CHECK(p1->combo[1].dest == (BIT(GB_X) | BIT(GB_Y) | BIT(GB_Z)));
    CHECK(p1->combo[2].kind == COMBO_SRC_KEY && p1->combo[2].code == 44);
    CHECK(p1->combo[3].kind == COMBO_SRC_NONE && p1->combo[4].kind == COMBO_SRC_NONE);
    CHECK(p1->autofire_hz[GB_X] == 15 && p1->autofire_hz[GB_A] == 30);

    /* 3. Round trip: save, reload into a clean state, save again -> identical. */
    char path2[512]; snprintf(path2, sizeof path2, "%s.2", path);
    CHECK(app_config_save(path2));
    input_map_init_defaults(); app_config_defaults();
    CHECK(app_config_load(path2));
    CHECK(g_input_map.pad_hotkeys == 0 && g_input_map.p[0].combo[2].dest == (BIT(GB_X) | BIT(GB_A)));
    char path3[512]; snprintf(path3, sizeof path3, "%s.3", path);
    CHECK(app_config_save(path3));
    FILE *a = fopen(path2, "rb"), *b = fopen(path3, "rb");
    CHECK(a && b);
    int ca, cb;
    do { ca = fgetc(a); cb = fgetc(b); CHECK(ca == cb); } while (ca != EOF);
    fclose(a); fclose(b);

    /* 4. Key-origin combo through input_current_mask (keyboard mocked, no pad). */
    input_map_init_defaults();
    g_input_map.p[0].device = INPUT_DEV_KEYBOARD;
    g_input_map.p[0].combo[0].kind = COMBO_SRC_KEY;
    g_input_map.p[0].combo[0].code = SDL_SCANCODE_SPACE;
    g_input_map.p[0].combo[0].dest = BIT(GB_X) | BIT(GB_A);
    g_input_map.p[0].autofire_hz[GB_A] = 15;
    input_map_set_frame(0);
    CHECK(input_current_mask(0) == 0);
    keyboard[SDL_SCANCODE_SPACE] = 1;
    input_map_set_frame(0);
    CHECK(input_current_mask(0) == (BIT(GB_X) | BIT(GB_A)));
    input_map_set_frame(2);
    CHECK(input_current_mask(0) == BIT(GB_X));   /* A gated off by autofire */
    keyboard[SDL_SCANCODE_SPACE] = 0;
    CHECK(input_current_mask(0) == 0);
    /* Combo origin of a disabled device does not fire. */
    g_input_map.p[0].device = INPUT_DEV_GAMEPAD;
    keyboard[SDL_SCANCODE_SPACE] = 1;
    input_map_set_frame(0);
    CHECK(input_current_mask(0) == 0);

    printf("input_combo_test OK\n");
    return 0;
}
