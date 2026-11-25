/* app_config_save must keep everything the runner does not own: game sections,
 * comments, unknown keys; update runner keys; be idempotent. */
#define SDL_MAIN_HANDLED
#include "input_map.h"
#include "app_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static char *slurp(const char *p)
{
    FILE *f = fopen(p, "rb"); CHECK(f);
    char *b = (char *)calloc(1, 65536);
    size_t n = fread(b, 1, 65535, f); b[n] = 0; fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *path = argv[1];
    const char *seed =
        "# my top comment\r\n"
        "; second comment\r\n"
        "\r\n"
        "[video]\r\n"
        "window_scale = 3   ; keep my comment\r\n"
        "mystery_key = 42\r\n"
        "; a note inside video\r\n"
        "\r\n"
        "[ec.audio]\r\n"
        "remaster = hybrid ; comentario\r\n"
        "\r\n"
        "[ec.bplus]\r\n"
        "model = v3_hq\r\n"
        "\r\n"
        "[input.p1]\r\n"
        "device = 1\r\n"
        "combo.1 = key:44 -> a+x\r\n"
        "combo.2 = key:45 -> b\r\n";
    FILE *f = fopen(path, "wb"); CHECK(f); fputs(seed, f); fclose(f);

    input_map_init_defaults(); app_config_defaults();
    CHECK(app_config_load(path));
    CHECK(g_app_config.window_scale == 3 && g_input_map.p[0].device == 1);
    g_app_config.window_scale = 4;                       /* a runner change */
    g_input_map.p[0].combo[1].kind = COMBO_SRC_NONE;     /* drop combo.2 */
    g_input_map.p[0].combo[1].dest = 0;
    CHECK(app_config_save(path));
    char *a = slurp(path);

    CHECK(strstr(a, "# my top comment\r\n; second comment\r\n\r\n[video]\r\n"));
    CHECK(strstr(a, "window_scale = 4\n"));
    CHECK(strstr(a, "mystery_key = 42\r\n; a note inside video\r\n"));
    CHECK(strstr(a, "[ec.audio]\r\nremaster = hybrid ; comentario\r\n\r\n"));
    CHECK(strstr(a, "[ec.bplus]\r\nmodel = v3_hq\r\n\r\n"));
    CHECK(strstr(a, "combo.1 = key:44 -> a+x\r\n"));
    CHECK(!strstr(a, "combo.2"));
    CHECK(strstr(a, "[audio]") && strstr(a, "[input]") && strstr(a, "pad_hotkeys"));
    CHECK(strstr(a, "[input.p6]"));
    CHECK(strstr(a, "[video]") < strstr(a, "[ec.audio]"));  /* original order kept */

    /* Idempotent, and a reload sees the right values. */
    CHECK(app_config_save(path));
    char *b = slurp(path);
    CHECK(!strcmp(a, b));
    input_map_init_defaults(); app_config_defaults();
    CHECK(app_config_load(path));
    CHECK(g_app_config.window_scale == 4 && g_input_map.p[0].combo[0].kind == COMBO_SRC_KEY);

    /* No file yet -> fresh file with header comment. */
    remove(path);
    CHECK(app_config_save(path));
    char *c = slurp(path);
    CHECK(strstr(c, "# segagenesisrecomp launcher settings") && strstr(c, "[video]"));
    CHECK(app_config_save(path));
    char *d = slurp(path);
    CHECK(!strcmp(c, d));
    printf("app_config_roundtrip_test OK\n");
    return 0;
}
