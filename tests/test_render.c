#include "render.h"
#include <assert.h>
#include <stdio.h>

static void selection_layout(void)
{
    uint16_t pixels[NF_SCREEN_WIDTH * NF_SCREEN_HEIGHT];
    NFBrain brain = {0};
    NFWorld world;
    NFUI ui = {0};
    NFRender render;
    const unsigned groups[] = {0, 9, 54};
    const unsigned top[] = {39, 54, 129};
    unsigned selected, candidate;
    nf_world_init(&world, UINT32_C(0x4e464c59));
    ui.neural_view = 1;
    ui.paused = 1;
    for (selected = 0; selected < 3; ++selected) {
        ui.selected_group = groups[selected];
        nf_render_init(&render, pixels);
        nf_render_frame(&render, &brain, &world, &ui);
        for (candidate = 0; candidate < 3; ++candidate) {
            unsigned row = top[candidate] * NF_SCREEN_WIDTH;
            if (candidate == selected) assert(pixels[row + 2] != pixels[row + 1]);
            else assert(pixels[row + 2] == pixels[row + 1]);
        }
    }
}

static void estimated_tick(void)
{
    uint16_t estimated[NF_SCREEN_WIDTH * NF_SCREEN_HEIGHT];
    uint16_t measured[NF_SCREEN_WIDTH * NF_SCREEN_HEIGHT];
    NFBrain brain = {0};
    NFWorld world;
    NFUI ui = {0};
    NFRender render;
    unsigned x, y;
    int marker_visible = 0;
    nf_world_init(&world, UINT32_C(0x4e464c59));
    ui.neural_view = 1;
    ui.paused = 1;
    ui.tick_ms = 42;
    nf_render_init(&render, estimated);
    nf_render_frame(&render, &brain, &world, &ui);
    ui.tick_measured = 1;
    nf_render_init(&render, measured);
    nf_render_frame(&render, &brain, &world, &ui);
    for (y = 188; y <= 194; ++y) {
        unsigned row = y * NF_SCREEN_WIDTH;
        for (x = 113; x <= 118; ++x)
            if (estimated[row + x] != estimated[row + 112]) marker_visible = 1;
        for (x = 0; x < 24; ++x)
            assert(estimated[row + 119 + x] == measured[row + 113 + x]);
    }
    assert(marker_visible);
}

int main(void)
{
    selection_layout();
    estimated_tick();
    puts("PASS: neural selection rows, visible estimate marker, intact tick duration");
    return 0;
}
