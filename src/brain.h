#ifndef NFLY_BRAIN_H
#define NFLY_BRAIN_H

#include <stddef.h>
#include <stdint.h>
#include "groups.h"

typedef struct {
    uint32_t neurons, edges;
    uint32_t offset[NF_GROUP_COUNT + 1];
    uint32_t *row, *target;
    float *weight, *voltage;
    uint8_t *group, *fired, *refractory;
    uint8_t active[NF_GROUP_COUNT], cooldown[NF_GROUP_COUNT];
    uint32_t spikes[NF_GROUP_COUNT];
    uint32_t ticks, fired_count;
} NFBrain;

typedef void (*NFLoadProgress)(const char *stage, uint32_t done, uint32_t total);

/* Zero-initialize before loading; failed loads release every allocation. */
int nf_brain_load(NFBrain *brain, const char *path, NFLoadProgress progress,
                  char *error, size_t error_size);
void nf_brain_free(NFBrain *brain);
void nf_brain_reset(NFBrain *brain);
/* One upstream LIF tick: decay, sustained input, prior spikes, threshold. */
void nf_brain_step(NFBrain *brain, const float stimulus[NF_GROUP_COUNT]);
void nf_brain_pulse(NFBrain *brain, unsigned group, float intensity);

#endif
