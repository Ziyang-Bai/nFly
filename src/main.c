#include <libndls.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "render.h"

static NFRender renderer;

/* The SDK's clock() is a stub and gettimeofday() resolves only seconds.
 * SP804 channel 2 runs at 32768 Hz; channel 1 remains available to msleep(). */
typedef struct {
    uint32_t load, control, previous, milliseconds, fraction;
} NFClock;

static volatile uint32_t *const timer = (volatile uint32_t *)0x900D0020;

static void clock_start(NFClock *clock)
{
    clock->load = timer[0];
    clock->control = timer[2];
    timer[2] = 0;
    timer[3] = 1;
    timer[0] = UINT32_MAX;
    timer[2] = 0x82; /* Enabled, 32-bit free running, no interrupt, no prescale. */
    msleep(1);
    clock->previous = timer[1];
    clock->milliseconds = 0;
    clock->fraction = 0;
}

static uint32_t clock_ms(NFClock *clock)
{
    uint32_t value = timer[1];
    uint32_t elapsed = clock->previous - value;
    uint64_t scaled = (uint64_t)elapsed * 125 + clock->fraction;
    clock->previous = value;
    clock->milliseconds += (uint32_t)(scaled >> 12);
    clock->fraction = (uint32_t)scaled & 4095;
    return clock->milliseconds;
}

static void clock_stop(const NFClock *clock)
{
    timer[2] = 0;
    timer[3] = 1;
    timer[0] = clock->load;
    timer[2] = clock->control;
}

static void present(void)
{
    lcd_blit(renderer.pixels, SCR_320x240_565);
}

static void load_progress(const char *stage, uint32_t done, uint32_t total)
{
    nf_render_loading(&renderer, stage, done, total);
    present();
}

static void restore_os(void)
{
    wait_no_key_pressed();
    lcd_init(SCR_TYPE_INVALID);
    refresh_osscr();
}

static int os_error(const char *message)
{
    wait_no_key_pressed();
    show_msgbox("nFly", message);
    restore_os();
    return 1;
}

static int pressed(const t_key *key)
{
    return (isKeyPressed)(key) != 0;
}

enum {
    K_EXIT, K_FOOD, K_SNAP, K_HEAD, K_THORAX, K_ABDOMEN, K_LEG,
    K_WIND, K_LIGHT, K_TEMP, K_DANGER, K_CLEAR, K_PAUSE,
    K_BRAIN, K_HELP, K_RESET, K_COUNT
};

static const t_key *const keys[K_COUNT] = {
    &KEY_NSPIRE_ESC, &KEY_NSPIRE_ENTER, &KEY_NSPIRE_F,
    &KEY_NSPIRE_1, &KEY_NSPIRE_2, &KEY_NSPIRE_3, &KEY_NSPIRE_4,
    &KEY_NSPIRE_A, &KEY_NSPIRE_L, &KEY_NSPIRE_T, &KEY_NSPIRE_D,
    &KEY_NSPIRE_C, &KEY_NSPIRE_P, &KEY_NSPIRE_B, &KEY_NSPIRE_H,
    &KEY_NSPIRE_R
};

static uint32_t read_keys(void)
{
    uint32_t bits = 0;
    unsigned i;
    for (i = 0; i < K_COUNT; ++i)
        if (pressed(keys[i])) bits |= UINT32_C(1) << i;
    return bits;
}

static unsigned read_arrows(void)
{
    unsigned bits = 0;
    if (isKeyPressed(KEY_NSPIRE_UP)) bits |= 1;
    if (isKeyPressed(KEY_NSPIRE_DOWN)) bits |= 2;
    if (isKeyPressed(KEY_NSPIRE_LEFT)) bits |= 4;
    if (isKeyPressed(KEY_NSPIRE_RIGHT)) bits |= 8;
    if (isKeyPressed(KEY_NSPIRE_UPRIGHT)) bits |= 1 | 8;
    if (isKeyPressed(KEY_NSPIRE_RIGHTDOWN)) bits |= 2 | 8;
    if (isKeyPressed(KEY_NSPIRE_DOWNLEFT)) bits |= 2 | 4;
    if (isKeyPressed(KEY_NSPIRE_LEFTUP)) bits |= 1 | 4;
    return bits;
}

static void move_cursor(NFUI *ui, unsigned arrows)
{
    int dx = ((arrows & 8) != 0) - ((arrows & 4) != 0);
    int dy = ((arrows & 2) != 0) - ((arrows & 1) != 0);
    if (ui->neural_view) {
        int group = (int)ui->selected_group + dx + dy * NF_NEURAL_COLUMNS;
        if (group < 0) group += NF_GROUP_COUNT;
        if (group >= NF_GROUP_COUNT) group -= NF_GROUP_COUNT;
        ui->selected_group = (unsigned)group;
    } else {
        ui->cursor_x += dx * 12.0f;
        ui->cursor_y += dy * 12.0f;
        if (ui->cursor_x < 0) ui->cursor_x = 0;
        if (ui->cursor_x > 957) ui->cursor_x = 957;
        if (ui->cursor_y < 0) ui->cursor_y = 0;
        if (ui->cursor_y > 477) ui->cursor_y = 477;
    }
}

static void reset_simulation(NFBrain *brain, NFWorld *world, NFUI *ui)
{
    nf_brain_reset(brain);
    nf_world_init(world, UINT32_C(0x4e464c59));
    nf_render_init(&renderer, renderer.pixels);
    ui->cursor_x = world->x;
    ui->cursor_y = world->y;
    ui->tick_measured = 0;
    ui->action = "Simulation reset";
}

static void apply_key(unsigned key, NFBrain *brain, NFWorld *world, NFUI *ui)
{
    static const char *const touches[] = {
        "Touch head", "Touch thorax", "Touch abdomen", "Touch legs"
    };
    switch (key) {
    case K_FOOD:
        ui->action = nf_world_food(world, ui->cursor_x, ui->cursor_y)
            ? "Food placed" : "Food limit: C to clear";
        break;
    case K_SNAP:
        ui->cursor_x = world->x;
        ui->cursor_y = world->y;
        ui->action = "Cursor on fly: Enter to feed";
        break;
    case K_HEAD: case K_THORAX: case K_ABDOMEN: case K_LEG:
        nf_world_touch(world, (NFTouchPart)(key - K_HEAD));
        ui->action = touches[key - K_HEAD];
        break;
    case K_WIND: {
        float direction = atan2f(ui->cursor_y - world->y,
                                 world->x - ui->cursor_x);
        int strong = isKeyPressed(KEY_NSPIRE_SHIFT);
        nf_world_wind(world, direction, strong ? 1.0f : 0.3f);
        ui->action = strong ? "Strong wind" : "Gentle wind";
        break;
    }
    case K_LIGHT:
        world->light = world->light >= 1.0f ? 0.5f :
                       world->light > 0.0f ? 0.0f : 1.0f;
        ui->action = "Light changed";
        break;
    case K_TEMP:
        world->temperature = world->temperature == 0.5f ? 0.75f :
                             world->temperature > 0.5f ? 0.25f : 0.5f;
        ui->action = "Temperature changed";
        break;
    case K_DANGER:
        world->danger = !world->danger;
        ui->action = world->danger ? "Danger odor on" : "Danger odor off";
        break;
    case K_CLEAR:
        world->food_count = 0;
        ui->action = "Food cleared";
        break;
    case K_PAUSE:
        ui->paused = !ui->paused;
        ui->action = ui->paused ? "Simulation paused" : "Simulation running";
        break;
    case K_BRAIN:
        ui->neural_view = !ui->neural_view;
        break;
    case K_RESET:
        reset_simulation(brain, world, ui);
        break;
    default:
        break;
    }
}

int main(int argc, char **argv)
{
    NFBrain brain = {0};
    NFWorld world;
    NFUI ui = {0};
    NFClock clock;
    uint32_t previous_keys, last_step, next_frame, arrow_deadline;
    unsigned previous_arrows = 0;
    char error[192];
    int dirty = 1;

    if (!has_colors || is_cm)
        return os_error("The full model needs a color CX or CX II with at least 64 MB RAM. Classic and 32 MB devices are not supported.");
    if (argc < 1 || !argv || !argv[0] || enable_relative_paths(argv) != 0)
        return os_error("Cannot locate the application folder. Launch nFly.tns from My Documents; keep connectome.tns beside it.");

    renderer.pixels = malloc(NF_SCREEN_WIDTH * NF_SCREEN_HEIGHT * sizeof(uint16_t));
    if (!renderer.pixels)
        return os_error("Not enough free RAM for the display. Close other Ndless applications or restart the calculator, then try again.");
    if (!lcd_init(SCR_320x240_565)) {
        free(renderer.pixels);
        return os_error("Cannot initialize the color LCD. Install the current Ndless release for this calculator.");
    }
    nf_render_init(&renderer, renderer.pixels);
    load_progress("Opening connectome.tns", 0, 0);
    if (!nf_brain_load(&brain, "connectome.tns", load_progress, error, sizeof(error))) {
        nf_render_error(&renderer, error);
        present();
        wait_no_key_pressed();
        while (!isKeyPressed(KEY_NSPIRE_ESC) && !isKeyPressed(KEY_NSPIRE_ENTER))
            msleep(20);
        nf_brain_free(&brain);
        free(renderer.pixels);
        restore_os();
        return 1;
    }

    reset_simulation(&brain, &world, &ui);
    ui.action = "F then Enter: feed at fly";
    wait_no_key_pressed();
    previous_keys = read_keys();
    clock_start(&clock);
    last_step = next_frame = arrow_deadline = clock_ms(&clock);

    for (;;) {
        uint32_t now = clock_ms(&clock);
        uint32_t held = read_keys();
        uint32_t down = held & ~previous_keys;
        unsigned arrows = read_arrows();
        unsigned key;
        int was_frozen = ui.paused || ui.help;
        previous_keys = held;

        if (down & (UINT32_C(1) << K_EXIT)) break;
        if (down & (UINT32_C(1) << K_HELP)) {
            ui.help = !ui.help;
            dirty = 1;
        }
        if (!ui.help) {
            for (key = K_FOOD; key < K_COUNT; ++key) {
                if (!(down & (UINT32_C(1) << key))) continue;
                apply_key(key, &brain, &world, &ui);
                dirty = 1;
                if (key == K_RESET) last_step = now;
            }
            if (arrows && (arrows != previous_arrows ||
                           (int32_t)(now - arrow_deadline) >= 0)) {
                move_cursor(&ui, arrows);
                arrow_deadline = now + (arrows != previous_arrows ? 260 : 75);
                dirty = 1;
            }
        }
        previous_arrows = arrows;
        if (was_frozen || ui.paused || ui.help) last_step = now;

        /* Each step advances 100 simulated ms. Slower hardware stretches wall
         * time, with input polling between every pair of neural ticks. */
        if (!ui.paused && !ui.help && (uint32_t)(now - last_step) >= 100) {
            uint32_t begin = clock_ms(&clock);
            last_step = begin;
            nf_world_step(&world, &brain);
            ui.tick_ms = clock_ms(&clock) - begin;
            ui.tick_measured = 1;
            dirty = 1;
        }
        now = clock_ms(&clock);
        if (dirty || (int32_t)(now - next_frame) >= 0) {
            nf_render_frame(&renderer, &brain, &world, &ui);
            present();
            next_frame = clock_ms(&clock) + 50;
            dirty = 0;
        }
        msleep(5);
    }

    clock_stop(&clock);
    nf_brain_free(&brain);
    free(renderer.pixels);
    restore_os();
    return 0;
}
