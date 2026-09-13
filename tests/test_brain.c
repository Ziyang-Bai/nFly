#define _POSIX_C_SOURCE 200809L
#include "brain.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

static void u32(gzFile file, uint32_t value)
{
    unsigned char bytes[4] = {value, value >> 8, value >> 16, value >> 24};
    assert(gzwrite(file, bytes, 4) == 4);
}

static void f32(gzFile file, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    u32(file, bits);
}

static void fixture(const char *path, int invalid_target)
{
    gzFile file = gzopen(path, "wb");
    unsigned g;
    const uint32_t rows[] = {0, 2, 3, 3, 3};
    assert(file);
    assert(gzwrite(file, "NFLYCSR1", 8) == 8);
    u32(file, 4);
    u32(file, 3);
    u32(file, NF_GROUP_COUNT);
    for (g = 0; g <= NF_GROUP_COUNT; ++g) u32(file, g < 4 ? g : 4);
    for (g = 0; g < 5; ++g) u32(file, rows[g]);
    u32(file, invalid_target ? 4 : 1);
    u32(file, 2);
    u32(file, 3);
    f32(file, 1.1f);
    f32(file, -0.5f);
    f32(file, 1.1f);
    assert(gzclose(file) == Z_OK);
}

int main(void)
{
    char path[] = "/tmp/nfly-brain-XXXXXX";
    char error[160];
    float stimulus[NF_GROUP_COUNT] = {0};
    NFBrain brain = {0};
    int fd = mkstemp(path);
    unsigned i;
    FILE *file;
    int byte;
    assert(fd >= 0);
    close(fd);
    fixture(path, 0);
    assert(nf_brain_load(&brain, path, NULL, error, sizeof(error)));

    nf_brain_pulse(&brain, 0, 1.1f);
    nf_brain_step(&brain, stimulus);
    assert(brain.fired[0] && !brain.fired[1]);
    nf_brain_step(&brain, stimulus);
    assert(!brain.fired[0] && brain.fired[1]);
    assert(!brain.fired[2] && brain.voltage[2] < 0);
    nf_brain_step(&brain, stimulus);
    assert(brain.fired[3]);

    nf_brain_reset(&brain);
    stimulus[0] = 1.1f;
    nf_brain_step(&brain, stimulus);
    assert(brain.fired[0]);
    nf_brain_step(&brain, stimulus);
    assert(!brain.fired[0]);
    nf_brain_step(&brain, stimulus);
    assert(!brain.fired[0]);
    nf_brain_step(&brain, stimulus);
    assert(brain.fired[0]);

    nf_brain_reset(&brain);
    stimulus[0] = 0.08f;
    {
        float expected = 0;
        for (i = 0; i < 12; ++i) {
            expected = (float)(expected * 0.95);
            expected += stimulus[0];
            nf_brain_step(&brain, stimulus);
            assert(brain.voltage[0] == expected);
        }
    }

    nf_brain_reset(&brain);
    stimulus[0] = 0;
    nf_brain_pulse(&brain, 0, 0.1f);
    for (i = 0; i < 20; ++i) nf_brain_step(&brain, stimulus);
    assert(!brain.active[0] && brain.voltage[0] == 0);
    nf_brain_reset(&brain);
    assert(brain.ticks == 0 && brain.fired_count == 0);
    nf_brain_free(&brain);

    fixture(path, 1);
    assert(!nf_brain_load(&brain, path, NULL, error, sizeof(error)));
    assert(error[0] && !brain.row && !brain.voltage);

    fixture(path, 0);
    file = fopen(path, "r+b");
    assert(file && fseek(file, -8, SEEK_END) == 0);
    byte = fgetc(file);
    assert(byte != EOF && fseek(file, -1, SEEK_CUR) == 0);
    assert(fputc(byte ^ 0x80, file) != EOF);
    assert(fclose(file) == 0);
    assert(!nf_brain_load(&brain, path, NULL, error, sizeof(error)));
    assert(error[0] && !brain.target && !brain.weight);

    file = fopen(path, "wb");
    assert(file && fwrite("NFLY", 1, 4, file) == 4);
    assert(fclose(file) == 0);
    assert(!nf_brain_load(&brain, path, NULL, error, sizeof(error)));
    nf_brain_free(&brain);
    assert(unlink(path) == 0);
    puts("PASS: causal propagation, inhibition, refractory, cooldown, reset, invalid graph, gzip CRC, truncation");
    return 0;
}
