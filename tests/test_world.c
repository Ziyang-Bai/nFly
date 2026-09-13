#include "world.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_centered(const NFWorld *world)
{
    assert(fabsf(world->x - 480.0f) < 1e-6f);
    assert(fabsf(world->y - 240.0f) < 1e-6f);
    assert(fabsf(world->speed) < 1e-6f);
}

static void begin_grooming(NFWorld *world, NFBrain *brain)
{
    unsigned i;
    nf_brain_reset(brain);
    nf_world_init(world, UINT32_C(0x4e464c59));
    world->drives.groom = 0;
    world->drives.curiosity = 0;
    world->light = 0;
    nf_brain_pulse(brain, NF_G_MN_ABDOMEN, 1.1f);
    for (i = 0; i < 4; ++i) {
        nf_world_step(world, brain);
        assert(world->behavior == NF_IDLE);
        assert_centered(world);
    }
    assert(world->time_ms == 400);
    nf_world_step(world, brain);
    assert(world->time_ms == 500);
    assert(world->behavior == NF_GROOM && world->entered_ms == 400);
    assert_centered(world);
}

int main(void)
{
    uint32_t rows[3] = {0, 0, 0};
    float voltage[2] = {0, 0};
    uint8_t groups[2] = {NF_G_MECH_BRISTLE, NF_G_MN_ABDOMEN};
    uint8_t fired[2] = {0, 0}, refractory[2] = {0, 0};
    NFBrain brain = {0};
    NFWorld world;
    const NFTouchPart parts[] = {NF_HEAD, NF_LEG};
    unsigned g, i, scenario;
    brain.neurons = 2;
    brain.row = rows;
    brain.voltage = voltage;
    brain.group = groups;
    brain.fired = fired;
    brain.refractory = refractory;
    for (g = 0; g <= NF_GROUP_COUNT; ++g)
        brain.offset[g] = g <= NF_G_MECH_BRISTLE ? 0 :
                          g <= NF_G_MN_ABDOMEN ? 1 : 2;

    begin_grooming(&world, &brain);
    for (i = 0; i < 5; ++i) nf_world_step(&world, &brain);
    assert(world.time_ms == 1000 && world.behavior == NF_GROOM);
    assert(world.motor.groom == 0);
    while (world.time_ms < 2400) nf_world_step(&world, &brain);
    assert(world.behavior == NF_GROOM);
    nf_world_step(&world, &brain);
    assert(world.time_ms == 2500 && world.behavior == NF_IDLE);
    assert(world.entered_ms == 2400 && world.cooldown[NF_GROOM] == 5400);
    assert_centered(&world);

    for (scenario = 0; scenario < 2; ++scenario) {
        begin_grooming(&world, &brain);
        nf_world_touch(&world, parts[scenario]);
        for (i = 0; i < 5; ++i) nf_world_step(&world, &brain);
        assert(world.time_ms == 1000 && world.behavior == NF_GROOM);
        for (i = 0; i < 4; ++i) nf_world_step(&world, &brain);
        assert(world.time_ms == 1400 && world.behavior == NF_GROOM);
        if (parts[scenario] == NF_HEAD) {
            assert(fired[0] == 1 && brain.spikes[NF_G_MECH_BRISTLE] == 1);
            assert(nf_world_food(&world, world.x, world.y));
            nf_brain_reset(&brain);
            nf_world_init(&world, UINT32_C(0x4e464c59));
            assert(world.time_ms == 0 && brain.ticks == 0);
            assert(world.behavior == NF_IDLE);
            assert_centered(&world);
            assert(world.food_count == 0);
            assert(world.touch_part == NF_NO_TOUCH && world.touch_until == 0);
            nf_world_step(&world, &brain);
            assert(world.time_ms == 100 && world.behavior == NF_IDLE);
            assert_centered(&world);
            assert(!fired[0] && !fired[1]);
        } else {
            assert(fired[0] == 0 && brain.spikes[NF_G_MECH_BRISTLE] == 0);
            assert(voltage[0] > 0 && voltage[0] < 1);
        }
    }
    puts("PASS: grooming entry, minimum duration, exit, touch intensity, active reset");
    return 0;
}
