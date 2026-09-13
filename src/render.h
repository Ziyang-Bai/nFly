#ifndef NFLY_RENDER_H
#define NFLY_RENDER_H

#include "world.h"

#define NF_SCREEN_WIDTH 320
#define NF_SCREEN_HEIGHT 240
#define NF_NEURAL_COLUMNS 9

typedef struct {
    float cursor_x, cursor_y;
    unsigned selected_group;
    int neural_view, help, paused;
    uint32_t tick_ms;
    int tick_measured;
    const char *action;
} NFUI;

typedef struct {
    uint16_t *pixels;
    uint32_t last_time;
    float walk_phase, groom_phase, wing_spread, proboscis;
    int clip_top, clip_bottom;
} NFRender;

void nf_render_init(NFRender *render, uint16_t *pixels);
void nf_render_frame(NFRender *render, const NFBrain *brain,
                     const NFWorld *world, const NFUI *ui);
void nf_render_loading(NFRender *render, const char *stage,
                       uint32_t done, uint32_t total);
void nf_render_error(NFRender *render, const char *message);

#endif
