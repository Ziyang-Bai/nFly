#ifndef NFLY_WORLD_H
#define NFLY_WORLD_H

#include "brain.h"

#define NF_FOOD_MAX 24
#define NF_WORLD_WIDTH 960.0f
#define NF_WORLD_HEIGHT 480.0f

typedef enum {
    NF_IDLE, NF_WALK, NF_EXPLORE, NF_PHOTOTAXIS, NF_REST,
    NF_GROOM, NF_FEED, NF_FLY, NF_STARTLE, NF_BRACE, NF_STATE_COUNT
} NFBehavior;
typedef enum { NF_NO_TOUCH = -1, NF_HEAD, NF_THORAX, NF_ABDOMEN, NF_LEG } NFTouchPart;
typedef struct { float x, y, eaten, duration; } NFFood;
typedef struct { float hunger, fear, fatigue, curiosity, groom; } NFDrives;
typedef struct {
    float walk_left, walk_right, flight, feed, groom, startle, head;
    float left, right;
} NFMotor;
typedef struct {
    float x, y, direction, target_direction, speed, target_speed, speed_delta;
    NFDrives drives;
    NFMotor motor;
    NFBehavior behavior;
    NFTouchPart touch_part;
    uint32_t time_ms, entered_ms, cooldown[NF_STATE_COUNT];
    uint32_t touch_until, wind_until, touch_history[3];
    unsigned touch_count;
    float wind_strength, wind_direction, light, temperature;
    int food_nearby, food_contact, danger, nociception, startle_burst;
    float burst_direction;
    NFFood food[NF_FOOD_MAX];
    unsigned food_count;
    float activity[NF_GROUP_COUNT], stimulus[NF_GROUP_COUNT];
    uint32_t pending_spikes[NF_GROUP_COUNT], pending_ticks, random_state;
} NFWorld;

extern const char *const nf_behavior_names[NF_STATE_COUNT];
void nf_world_init(NFWorld *world, uint32_t seed);
/* Advance 100 ms of simulation time, including one full-connectome tick. */
void nf_world_step(NFWorld *world, NFBrain *brain);
int nf_world_food(NFWorld *world, float x, float y);
void nf_world_touch(NFWorld *world, NFTouchPart part);
void nf_world_wind(NFWorld *world, float direction, float strength);

#endif
