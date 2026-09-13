/* Native port of FlyBrain sim-worker.js; upstream MIT notice in LICENSE. */
#include "brain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t little32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int read_words(gzFile file, void *output, uint32_t count,
                      const char *stage, NFLoadProgress progress)
{
    uint32_t done = 0;
    unsigned char *bytes = output;
    while (done < count) {
        uint32_t chunk = count - done;
        if (chunk > 65536) chunk = 65536;
        if (gzread(file, bytes + done * 4, chunk * 4) != (int)(chunk * 4)) return 0;
        done += chunk;
        if (progress) progress(stage, done, count);
    }
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (done = 0; done < count; ++done) {
        uint32_t value = little32(bytes + done * 4);
        memcpy(bytes + done * 4, &value, 4);
    }
#endif
    return 1;
}

void nf_brain_free(NFBrain *b)
{
    free(b->row); free(b->target); free(b->weight); free(b->voltage);
    free(b->group); free(b->fired); free(b->refractory);
    memset(b, 0, sizeof(*b));
}

void nf_brain_reset(NFBrain *b)
{
    if (b->voltage) memset(b->voltage, 0, b->neurons * sizeof(float));
    if (b->fired) memset(b->fired, 0, b->neurons);
    if (b->refractory) memset(b->refractory, 0, b->neurons);
    memset(b->active, 0, sizeof(b->active));
    memset(b->cooldown, 0, sizeof(b->cooldown));
    memset(b->spikes, 0, sizeof(b->spikes));
    b->ticks = b->fired_count = 0;
}

int nf_brain_load(NFBrain *b, const char *path, NFLoadProgress progress,
                  char *error, size_t error_size)
{
    unsigned char header[20], extra;
    uint32_t g, i;
    const char *failure = "Damaged connectome.tns. Copy the full data file again.";
    gzFile file = gzopen(path, "rb");
    int zerror;
    if (error_size) error[0] = 0;
    if (!file) {
        snprintf(error, error_size, "Cannot open connectome.tns. Place it beside nFly.tns.");
        return 0;
    }
    if (gzread(file, header, sizeof(header)) != (int)sizeof(header) ||
        memcmp(header, "NFLYCSR1", 8) != 0) goto fail;
    b->neurons = little32(header + 8);
    b->edges = little32(header + 12);
    if (little32(header + 16) != NF_GROUP_COUNT || b->neurons == 0 ||
        b->neurons > 200000 || b->edges > 4000000) goto fail;
    if (!read_words(file, b->offset, NF_GROUP_COUNT + 1, "Group index", progress)) goto fail;
    if (b->offset[0] != 0 || b->offset[NF_GROUP_COUNT] != b->neurons) goto fail;
    for (g = 0; g < NF_GROUP_COUNT; ++g)
        if (b->offset[g] > b->offset[g + 1]) goto fail;
    b->row = malloc((b->neurons + 1) * sizeof(uint32_t));
    b->target = malloc((b->edges ? b->edges : 1) * sizeof(uint32_t));
    b->weight = malloc((b->edges ? b->edges : 1) * sizeof(float));
    b->voltage = calloc(b->neurons, sizeof(float));
    b->group = malloc(b->neurons);
    b->fired = calloc(b->neurons, 1);
    b->refractory = calloc(b->neurons, 1);
    if (!b->row || !b->target || !b->weight || !b->voltage ||
        !b->group || !b->fired || !b->refractory) {
        failure = "Not enough free RAM. The full graph needs 23 MB. Restart the calculator and reopen nFly.";
        goto fail;
    }
    if (!read_words(file, b->row, b->neurons + 1, "Neuron rows", progress) ||
        !read_words(file, b->target, b->edges, "Synapse targets", progress) ||
        !read_words(file, b->weight, b->edges, "Synapse weights", progress)) goto fail;
    if (b->row[0] != 0 || b->row[b->neurons] != b->edges) goto fail;
    for (i = 0; i < b->neurons; ++i)
        if (b->row[i] > b->row[i + 1]) goto fail;
    for (i = 0; i < b->edges; ++i)
        if (b->target[i] >= b->neurons || !isfinite(b->weight[i])) goto fail;
    if (gzread(file, &extra, 1) != 0) goto fail;
    gzerror(file, &zerror);
    if (zerror != Z_OK || !gzeof(file)) goto fail;
    if (gzclose(file) != Z_OK) {
        file = NULL;
        goto fail;
    }
    for (g = 0; g < NF_GROUP_COUNT; ++g)
        memset(b->group + b->offset[g], (int)g, b->offset[g + 1] - b->offset[g]);
    nf_brain_reset(b);
    return 1;
fail:
    if (file) gzclose(file);
    nf_brain_free(b);
    snprintf(error, error_size, "%s", failure);
    return 0;
}

void nf_brain_pulse(NFBrain *b, unsigned group, float intensity)
{
    uint32_t i;
    if (group >= NF_GROUP_COUNT || !isfinite(intensity) ||
        b->offset[group] == b->offset[group + 1]) return;
    for (i = b->offset[group]; i < b->offset[group + 1]; ++i)
        b->voltage[i] += intensity;
    if (!b->active[group]) {
        b->active[group] = 1;
        b->cooldown[group] = 20;
    }
}

void nf_brain_step(NFBrain *b, const float stimulus[NF_GROUP_COUNT])
{
    uint8_t received[NF_GROUP_COUNT] = {0};
    unsigned g;
    uint32_t i, j;
    memset(b->spikes, 0, sizeof(b->spikes));
    b->fired_count = 0;
    for (g = 0; g < NF_GROUP_COUNT; ++g) {
        if (stimulus[g] != 0 && b->offset[g] != b->offset[g + 1] && !b->active[g]) {
            b->active[g] = 1;
            b->cooldown[g] = 20;
        }
    }
    for (g = 0; g < NF_GROUP_COUNT; ++g) {
        if (!b->active[g]) continue;
        for (i = b->offset[g]; i < b->offset[g + 1]; ++i) {
            if (b->refractory[i]) {
                --b->refractory[i];
                b->voltage[i] = 0;
            } else {
                /* JavaScript multiplies in binary64, then stores Float32Array. */
                b->voltage[i] *= 0.95;
            }
        }
    }
    for (g = 0; g < NF_GROUP_COUNT; ++g) {
        if (stimulus[g] == 0) continue;
        for (i = b->offset[g]; i < b->offset[g + 1]; ++i)
            if (!b->refractory[i]) b->voltage[i] += stimulus[g];
    }
    for (g = 0; g < NF_GROUP_COUNT; ++g) {
        if (!b->active[g]) continue;
        for (i = b->offset[g]; i < b->offset[g + 1]; ++i) {
            if (!b->fired[i]) continue;
            for (j = b->row[i]; j < b->row[i + 1]; ++j) {
                uint32_t target = b->target[j];
                b->voltage[target] += b->weight[j];
                received[b->group[target]] = 1;
            }
        }
    }
    for (g = 0; g < NF_GROUP_COUNT; ++g) {
        if (received[g] && !b->active[g]) {
            b->active[g] = 1;
            b->cooldown[g] = 20;
        }
        if (!b->active[g]) continue;
        for (i = b->offset[g]; i < b->offset[g + 1]; ++i) {
            b->fired[i] = 0;
            if (!b->refractory[i] && b->voltage[i] >= 1.0f) {
                b->fired[i] = 1;
                b->voltage[i] = 0;
                b->refractory[i] = 3;
                ++b->spikes[g];
                ++b->fired_count;
            }
        }
        if (b->spikes[g] || received[g] || stimulus[g] != 0) {
            b->cooldown[g] = 20;
        } else if (--b->cooldown[g] == 0) {
            uint32_t start = b->offset[g], count = b->offset[g + 1] - start;
            b->active[g] = 0;
            memset(b->voltage + start, 0, count * sizeof(float));
            memset(b->fired + start, 0, count);
            memset(b->refractory + start, 0, count);
        }
    }
    ++b->ticks;
}
