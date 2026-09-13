/* FlyBrain bridge, drive and movement model; upstream MIT notice in THIRD_PARTY_NOTICES. */
#include "world.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define G(name) NF_G_##name

const char *const nf_behavior_names[NF_STATE_COUNT] = {
    "Idle", "Walk", "Explore", "Phototaxis", "Rest", "Groom", "Feed", "Fly", "Startle", "Brace"
};
static const uint32_t minimum_ms[NF_STATE_COUNT] = {0,500,1000,1000,3000,2000,2000,1500,800,500};
static const uint32_t cooldown_ms[NF_STATE_COUNT] = {0,0,0,0,0,3000,1000,1000,2000,1000};

static float clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float angle(float a)
{
    a = fmodf(a, 2 * PI);
    if (a > PI) a -= 2 * PI;
    if (a < -PI) a += 2 * PI;
    return a;
}
static float random_unit(NFWorld *w)
{
    uint32_t x = w->random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    w->random_state = x;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}
static int moving(const NFWorld *w)
{
    return w->behavior == NF_WALK || w->behavior == NF_EXPLORE ||
           w->behavior == NF_PHOTOTAXIS || w->behavior == NF_FLY ||
           (w->behavior == NF_STARTLE && w->startle_burst);
}
static int nearest(const NFWorld *w, float *distance)
{
    unsigned i;
    int best = -1;
    float d2 = 1e30f;
    for (i = 0; i < w->food_count; ++i) {
        float dx = w->x - w->food[i].x, dy = w->y - w->food[i].y;
        float candidate = dx * dx + dy * dy;
        if (candidate < d2) { best = (int)i; d2 = candidate; }
    }
    *distance = sqrtf(d2);
    return best;
}
static int touch_active(const NFWorld *w) { return (int32_t)(w->touch_until - w->time_ms) > 0; }
static int wind_active(const NFWorld *w) { return (int32_t)(w->wind_until - w->time_ms) > 0; }

void nf_world_init(NFWorld *w, uint32_t seed)
{
    memset(w, 0, sizeof(*w));
    w->x = NF_WORLD_WIDTH * 0.5f;
    w->y = NF_WORLD_HEIGHT * 0.5f;
    w->drives.hunger = 0.3f;
    w->drives.curiosity = 0.5f;
    w->drives.groom = 0.1f;
    w->light = 1;
    w->temperature = 0.5f;
    w->touch_part = NF_NO_TOUCH;
    w->random_state = seed ? seed : 1;
}

int nf_world_food(NFWorld *w, float x, float y)
{
    NFFood *food;
    if (w->food_count >= NF_FOOD_MAX || !isfinite(x) || !isfinite(y)) return 0;
    food = &w->food[w->food_count++];
    food->x = clamp(x, 0, NF_WORLD_WIDTH);
    food->y = clamp(y, 0, NF_WORLD_HEIGHT);
    food->eaten = food->duration = 0;
    return 1;
}
void nf_world_touch(NFWorld *w, NFTouchPart part)
{
    unsigned i, count = 0;
    if (part < NF_HEAD || part > NF_LEG) return;
    w->touch_part = part;
    w->touch_until = w->time_ms + 2000;
    for (i = 0; i < w->touch_count; ++i)
        if (w->time_ms - w->touch_history[i] <= 4000)
            w->touch_history[count++] = w->touch_history[i];
    w->touch_history[count++] = w->time_ms;
    if (count == 3) { w->nociception = 1; count = 0; }
    w->touch_count = count;
}
void nf_world_wind(NFWorld *w, float direction, float strength)
{
    if (!isfinite(direction) || !isfinite(strength)) return;
    w->wind_direction = angle(direction);
    w->wind_strength = clamp(strength, 0, 1);
    w->wind_until = w->time_ms + 2000;
}

static void update_drives(NFWorld *w)
{
    NFDrives *d = &w->drives;
    d->hunger += 0.005f;
    if (w->behavior == NF_FEED) d->hunger -= 0.3f;
    if (touch_active(w)) d->fear += 0.3f;
    if (wind_active(w) && w->wind_strength > 0.5f) d->fear += 0.2f * w->wind_strength;
    if (w->danger) d->fear += 0.25f;
    d->fear *= 0.85f;
    d->fatigue += moving(w) ? (w->light < 0.3f ? 0.006f : 0.003f) : -0.01f;
    d->curiosity += (random_unit(w) - 0.5f) * (w->light < 0.3f ? 0.02f : 0.06f);
    d->groom += 0.008f;
    if (touch_active(w)) d->groom += 0.2f;
    if (w->behavior == NF_GROOM) d->groom -= 0.5f;
    d->hunger = clamp(d->hunger, 0, 1); d->fear = clamp(d->fear, 0, 1);
    d->fatigue = clamp(d->fatigue, 0, 1); d->curiosity = clamp(d->curiosity, 0, 1);
    d->groom = clamp(d->groom, 0, 1);
}
static float drive_pulses(float value) { return value > 0.6f ? 3 : value > 0.4f ? 2 : 1; }
static void stimulation(NFWorld *w)
{
    const NFDrives *d = &w->drives;
    float *s = w->stimulus;
    float tonic = w->light == 0 ? 0.03f : 0.08f;
    memset(s, 0, sizeof(w->stimulus));
    if (d->hunger > 0.2f) s[G(DRIVE_HUNGER)] = 0.15f * d->hunger * drive_pulses(d->hunger);
    if (d->fear > 0.05f) s[G(DRIVE_FEAR)] = 0.15f * d->fear * (d->fear > 0.5f ? 3 : d->fear > 0.2f ? 2 : 1);
    if (d->fatigue > 0.3f) s[G(DRIVE_FATIGUE)] = 0.15f * d->fatigue;
    if (d->curiosity > 0.2f) s[G(DRIVE_CURIOSITY)] = 0.15f * d->curiosity * (d->curiosity > 0.5f ? 2 : 1);
    if (d->groom > 0.3f) s[G(DRIVE_GROOM)] = 0.15f * d->groom;
    if (touch_active(w)) s[G(MECH_BRISTLE)] = (w->touch_part == NF_HEAD || w->touch_part == NF_THORAX) ? 0.30f : 0.15f;
    if (w->food_nearby) s[G(OLF_ORN_FOOD)] = 0.15f;
    if (w->food_contact) s[G(GUS_GRN_SWEET)] = 0.15f;
    if (w->danger) s[G(OLF_ORN_DANGER)] = 0.15f;
    if (wind_active(w)) s[G(MECH_JO)] = 0.15f * w->wind_strength;
    if (w->light > 0.2f) { s[G(VIS_R1R6)] = 0.15f * w->light; s[G(VIS_R7R8)] = 0.15f * w->light * 0.7f; }
    if (w->temperature > 0.65f) s[G(THERMO_WARM)] = 0.15f * (w->temperature - 0.5f) * 2;
    else if (w->temperature < 0.35f) s[G(THERMO_COOL)] = 0.15f * (0.5f - w->temperature) * 2;
    if (moving(w)) {
        s[G(MECH_CHORD)] = 0.15f;
        if (w->light > 0.1f) s[G(VIS_LPTC)] = 0.15f * 0.3f;
    }
    s[G(CX_FC)] = s[G(CX_EPG)] = s[G(CX_PFN)] = tonic;
}

/* FAFB maps the brain. FlyBrain's virtual VNC maps descending activity to limbs. */
static void synthesize_motors(NFWorld *w)
{
    float *a = w->activity;
    float desc = a[G(GNG_DESC)], vcpg = a[G(VNC_CPG)];
    float walk = (a[G(CX_PFN)] + a[G(CX_FC)] + a[G(CX_EPG)]) * 0.3f +
                 (a[G(MB_MBON_APP)] + a[G(LH_APP)]) * 0.5f + (desc + vcpg) * 0.2f;
    float flight = a[G(DRIVE_FEAR)] * 2 + (a[G(MB_MBON_AV)] + a[G(LH_AV)]) * 0.8f +
                   a[G(DN_STARTLE)] * 1.5f + a[G(NOCI)];
    float groom = a[G(DRIVE_GROOM)] * 1.5f + a[G(SEZ_GROOM)];
    float feed = a[G(SEZ_FEED)] + a[G(MN_PROBOSCIS)] * 0.5f;
    float proxy = fmaxf(fmaxf(walk * 0.45f, flight * 0.35f), fmaxf(groom * 0.3f, feed * 0.25f));
    float total, drive, jitter, left, right;
    if (proxy > desc) a[G(GNG_DESC)] = desc = proxy;
    total = desc + vcpg;
    if (total < 0.5f) return;
    drive = total * 0.6f * (1 + walk * 0.1f);
    jitter = (random_unit(w) - 0.5f) * 0.04f;
    left = drive * (1 + jitter) / 3;
    right = drive * (1 - jitter) / 3;
    a[G(MN_LEG_L1)] += left; a[G(MN_LEG_L2)] += left; a[G(MN_LEG_L3)] += left;
    a[G(MN_LEG_R1)] += right; a[G(MN_LEG_R2)] += right; a[G(MN_LEG_R3)] += right;
    if (flight > 1) { a[G(MN_WING_L)] += flight * 0.6f * 0.7f; a[G(MN_WING_R)] += flight * 0.6f * 0.7f; }
    if (a[G(DRIVE_FEAR)] > 3) a[G(DN_STARTLE)] += a[G(DRIVE_FEAR)] * 0.6f;
    if (groom > 1) a[G(MN_ABDOMEN)] += groom * 0.6f * 0.3f;
    if (feed > 0.5f) a[G(MN_PROBOSCIS)] += feed * 0.6f * 0.3f;
}
static float drain(NFWorld *w, unsigned g)
{
    float value = w->activity[g];
    w->activity[g] = 0;
    return value;
}
static void motor_control(NFWorld *w)
{
    NFMotor *m = &w->motor;
    float l1 = drain(w, G(MN_LEG_L1)), r1 = drain(w, G(MN_LEG_R1));
    float abdomen;
    m->walk_left = fmaxf(0, l1 + drain(w, G(MN_LEG_L2)) + drain(w, G(MN_LEG_L3)));
    m->walk_right = fmaxf(0, r1 + drain(w, G(MN_LEG_R2)) + drain(w, G(MN_LEG_R3)));
    m->flight = fmaxf(0, drain(w, G(MN_WING_L)) + drain(w, G(MN_WING_R)));
    m->feed = fmaxf(0, drain(w, G(MN_PROBOSCIS)));
    abdomen = drain(w, G(MN_ABDOMEN));
    m->head = drain(w, G(MN_HEAD));
    m->groom = fmaxf(0, abdomen + (abdomen > 0 ? m->head : 0) + fminf(l1, r1));
    m->head = fmaxf(0, m->head);
    m->startle = fmaxf(0, w->activity[G(DN_STARTLE)]);
    m->left = m->walk_left; m->right = m->walk_right;
    if (m->flight > 10) { m->left += m->flight * 0.5f; m->right += m->flight * 0.5f; }
}
static int available(const NFWorld *w, NFBehavior state)
{
    return (int32_t)(w->time_ms - w->cooldown[state]) >= 0;
}
static NFBehavior evaluate(const NFWorld *w)
{
    float distance, walk = w->motor.walk_left + w->motor.walk_right;
    nearest(w, &distance);
    if (w->motor.startle > 30 && available(w, NF_STARTLE)) return NF_STARTLE;
    if (w->motor.flight > 15 && available(w, NF_FLY)) return NF_FLY;
    if ((w->motor.feed > 8 || (w->drives.hunger > 0.7f && w->food_nearby)) &&
        distance <= 50 && available(w, NF_FEED)) return NF_FEED;
    if (w->motor.groom > 8 && available(w, NF_GROOM)) return NF_GROOM;
    if (wind_active(w) && w->wind_strength < 0.5f && w->motor.startle < 30 && available(w, NF_BRACE)) return NF_BRACE;
    if (w->drives.fatigue > (w->light == 0 ? 0.4f : 0.7f)) return NF_REST;
    if (w->light > 0.5f && w->drives.curiosity > 0.2f && walk > 3) return NF_PHOTOTAXIS;
    if (walk > 5 && w->drives.curiosity > 0.4f) return NF_EXPLORE;
    if (walk > 5) return NF_WALK;
    return NF_IDLE;
}
static void transition(NFWorld *w)
{
    NFBehavior next;
    if (w->time_ms - w->entered_ms < minimum_ms[w->behavior]) return;
    next = evaluate(w);
    if (next == w->behavior) return;
    if (cooldown_ms[w->behavior]) w->cooldown[w->behavior] = w->time_ms + cooldown_ms[w->behavior];
    w->behavior = next;
    w->entered_ms = w->time_ms;
    w->startle_burst = 0;
    if (next == NF_STARTLE) w->activity[G(DN_STARTLE)] = 0;
}
static void movement_target(NFWorld *w)
{
    NFMotor *m = &w->motor;
    float distance;
    int food = nearest(w, &distance);
    float walk_speed = (fabsf(m->left) + fabsf(m->right)) / 100;
    switch (w->behavior) {
    case NF_WALK: case NF_EXPLORE:
        w->target_direction = w->direction + clamp((m->left - m->right) / 20, -0.05f, 0.05f) * PI;
        w->target_speed = walk_speed;
        if (w->behavior == NF_EXPLORE) w->target_direction += (random_unit(w) - 0.5f) * 0.3f;
        if (w->food_nearby && w->drives.hunger > 0.3f && food >= 0) {
            float a = atan2f(w->y - w->food[food].y, w->food[food].x - w->x);
            w->target_direction = w->direction + angle(a - w->direction) * fminf(1, w->drives.hunger);
            w->target_speed = fmaxf(w->target_speed, 0.3f);
        }
        if (m->head > 3) w->target_direction += fminf(m->head / 40 * 0.15f, 0.08f) * (m->walk_left > m->walk_right ? 1 : -1);
        w->speed_delta = (w->target_speed - w->speed) / 30;
        break;
    case NF_PHOTOTAXIS:
        w->target_direction = atan2f(w->y - NF_WORLD_HEIGHT / 2, NF_WORLD_WIDTH / 2 - w->x);
        w->target_speed = fmaxf(walk_speed, 0.3f);
        w->speed_delta = (w->target_speed - w->speed) / 30;
        break;
    case NF_FLY:
        w->target_direction = w->direction + (m->left - m->right) / 20 * PI + (random_unit(w) - 0.5f) * 0.2f;
        w->target_speed = fmaxf(1.5f, walk_speed * 2.5f);
        w->speed_delta = (w->target_speed - w->speed) / 10;
        break;
    case NF_STARTLE:
        w->target_speed = w->startle_burst ? 0.5f : 0;
        if (w->startle_burst) w->target_direction = w->burst_direction;
        w->speed_delta = w->startle_burst ? (w->target_speed - w->speed) / 30 : -w->speed * 0.5f;
        break;
    case NF_FEED:
        if (food >= 0 && distance > 20) {
            w->target_direction = atan2f(w->y - w->food[food].y, w->food[food].x - w->x);
            w->target_speed = 0.25f;
            w->speed_delta = (w->target_speed - w->speed) / 30;
        } else { w->target_speed = 0; w->speed_delta = -w->speed * 0.1f; }
        break;
    case NF_BRACE:
        w->target_direction = angle(w->target_direction + angle(w->wind_direction + PI - w->target_direction) * 0.8f);
        w->target_speed = 0; w->speed_delta = -w->speed * 0.1f;
        break;
    default:
        w->target_speed = 0;
        w->speed_delta = -w->speed * (w->behavior == NF_IDLE ? 0.05f : 0.1f);
        break;
    }
}
static void movement_frame(NFWorld *w)
{
    float distance, bx = 0, by = 0, retention = 0.9f;
    int food = nearest(w, &distance);
    unsigned i;
    if (w->behavior == NF_STARTLE && !w->startle_burst) {
        w->speed = w->speed_delta = 0;
        if (w->time_ms - w->entered_ms >= 200) {
            w->startle_burst = 1;
            w->speed = 3;
            w->burst_direction = angle(w->direction + PI + (random_unit(w) - 0.5f) * 0.5f);
            w->direction = w->target_direction = w->burst_direction;
            w->target_speed = 0.5f;
            w->speed_delta = (w->target_speed - w->speed) / 30;
        }
    }
    if (w->behavior == NF_GROOM || w->behavior == NF_REST || w->behavior == NF_IDLE || w->behavior == NF_BRACE)
        w->speed = w->speed > 0.05f ? w->speed * 0.92f : 0;
    if (w->behavior == NF_FEED) {
        if (food >= 0 && distance > 20) { if (w->speed > 0.2f) w->speed *= 0.92f; }
        else w->speed = w->speed > 0.05f ? w->speed * 0.92f : 0;
    }
    w->speed = fmaxf(0, w->speed + w->speed_delta);
    if (w->x < 50) bx = (50 - w->x) / 50;
    else if (w->x > NF_WORLD_WIDTH - 50) bx = -(50 - (NF_WORLD_WIDTH - w->x)) / 50;
    if (w->y < 50) by = -(50 - w->y) / 50;
    else if (w->y > NF_WORLD_HEIGHT - 50) by = (50 - (NF_WORLD_HEIGHT - w->y)) / 50;
    if (bx != 0 || by != 0) w->target_direction += angle(atan2f(by, bx) - w->target_direction) * fminf(1, sqrtf(bx * bx + by * by)) * 0.3f;
    if (w->behavior == NF_STARTLE && w->startle_burst) retention = 0.3f;
    else if (w->behavior == NF_FLY) retention = 0.4f;
    w->direction = angle(w->direction + angle(w->target_direction - w->direction) * (1 - retention));
    w->target_direction = angle(w->target_direction);
    w->x += cosf(w->direction) * w->speed;
    w->y -= sinf(w->direction) * w->speed;
    if (w->x < 0 || w->x > NF_WORLD_WIDTH || w->y < 0 || w->y > NF_WORLD_HEIGHT) {
        w->x = clamp(w->x, 0, NF_WORLD_WIDTH); w->y = clamp(w->y, 0, NF_WORLD_HEIGHT);
        w->touch_until = w->time_ms + 2000;
    }
    w->food_nearby = w->food_contact = 0;
    for (i = 0; i < w->food_count;) {
        NFFood *f = &w->food[i];
        float dx = w->x - f->x, dy = w->y - f->y, d2 = dx * dx + dy * dy;
        if (d2 <= 2500) w->food_nearby = 1;
        if (d2 <= 400) {
            w->food_contact = 1;
            if (w->behavior == NF_FEED) {
                if (f->duration == 0) f->duration = 2000 + random_unit(w) * 3000;
                f->eaten += (1000.0f / 60) / f->duration;
                if (f->eaten >= 1) {
                    memmove(f, f + 1, (w->food_count - i - 1) * sizeof(*f));
                    --w->food_count;
                    continue;
                }
            }
        }
        ++i;
    }
    if (!touch_active(w)) { w->touch_until = 0; w->touch_part = NF_NO_TOUCH; }
    if (!wind_active(w)) { w->wind_until = 0; w->wind_strength = w->wind_direction = 0; }
}

void nf_world_step(NFWorld *w, NFBrain *b)
{
    unsigned g, frame;
    uint32_t start = w->time_ms;
    nf_brain_step(b, w->stimulus);
    for (g = 0; g < NF_GROUP_COUNT; ++g) w->pending_spikes[g] += b->spikes[g];
    ++w->pending_ticks;
    if (w->pending_ticks == 5) {
        if (w->nociception) { nf_brain_pulse(b, G(NOCI), 0.75f); w->nociception = 0; }
        update_drives(w);
        stimulation(w);
        for (g = 0; g < NF_GROUP_COUNT; ++g) {
            uint32_t size = b->offset[g + 1] - b->offset[g];
            float activation = size ? (float)w->pending_spikes[g] / ((float)size * w->pending_ticks) * 100 : 0;
            w->activity[g] = fmaxf(activation, w->activity[g] * 0.75f);
            w->pending_spikes[g] = 0;
        }
        w->pending_ticks = 0;
        w->activity[G(DRIVE_FEAR)] = w->drives.fear * 100;
        w->activity[G(DRIVE_CURIOSITY)] = w->drives.curiosity * 100;
        w->activity[G(DRIVE_GROOM)] = w->drives.groom * 100;
        synthesize_motors(w);
        motor_control(w);
        transition(w);
        movement_target(w);
    }
    for (frame = 0; frame < 6; ++frame) {
        w->time_ms = start + (frame + 1) * 100 / 6;
        movement_frame(w);
    }
}
