#include <esp_random.h>
#include <leaf.h>
#include <leaf-filters.h>
#include <leaf-math.h>
#include <leaf-envelopes.h>

#include "sd.h"

typedef struct {
    // tCycle *osc;
    tSawtooth *osc;
    // tTwoPole *filter;
    // tEnvelope *ampEnv; // env for amp
    // tEnvelope *filtEnv; // AD env for filter freq
} synth_t;

static LEAF leaf;
static char leafMempool[LEAF_MEM_POOL_SIZE];

static float rando() {return (float)rand()/RAND_MAX;}

static synth_t synth;

void stop_leaf(void) {
  // do something...
}

void start_leaf(void) {
    LEAF_init(&leaf, SAMPLE_RATE, leafMempool, LEAF_MEM_POOL_SIZE, &rando);

    // tCycle_init(&synth.osc, &leaf);
    // tCycle_setFreq(synth.osc, 440.0);
    tSawtooth_init(&synth.osc, &leaf);
    tSawtooth_setSampleRate(synth.osc, SAMPLE_RATE);

    assert(&synth.osc);

    // tTwoPole_init(&synth.filter, &leaf);
    // assert(&synth.filter);

    // tEnvelope_init(&synth.ampEnv, 0.05f, 0.9999f, 0, &leaf);
    // tEnvelope_on(synth.ampEnv, 60);
    // assert(&synth.ampEnv);

    // tEnvelope_init(&synth.filtEnv, 0.05f, 0.9999f, 0, &leaf);
    // tEnvelope_on(synth.filtEnv, 60);
    // assert(&synth.filtEnv);
}

void noteOn(uint8_t midipitch, uint8_t velocity) {
    Lfloat freq;

    // if note_is_on, noteOff()...

    freq = LEAF_midiToFrequency(midipitch);
    // tCycle_setFreq(synth.osc, freq);
    tSawtooth_setFreq(synth.osc, freq);

    // tEnvelope_on(synth.ampEnv, velocity);
    // tEnvelope_on(synth.filtEnv, velocity);
    // tCycle_setFreq(synth.osc, 0.0);
}

void noteOff(uint8_t midipitch) {
    // tCycle_setFreq(synth.osc, 0.0);
    tSawtooth_setFreq(synth.osc, 0.0);

    // stop amp env -- no need with the simple A/D tEnvelope
    // stop filter env -- no need
}

void synthTick(int16_t *buf, int bufsiz) {
   Lfloat samp = 0.00f;

   for (int i = 0; i < bufsiz; i++) {
       // samp = tCycle_tick(synth.osc);
       samp = tSawtooth_tick(synth.osc);
       // samp *= tEnvelope_tick(synth.ampEnv);
       // tTwoPole_setB0(synth.filter, tEnvelope_tick(synth.filtEnv));
       // samp = tTwoPole_tick(synth.filter, samp);
       buf[i] = (int16_t)(samp * 32767.0); // convert to 16-bit integer
   }
}
