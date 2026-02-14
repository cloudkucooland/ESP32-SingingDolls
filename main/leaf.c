#include <esp_random.h>
#include <leaf.h>
#include <leaf-filters.h>
#include <leaf-math.h>
#include <leaf-envelopes.h>

#include "sd.h"

// ideal situation: 2 osc - square & saw + sub, mixable.
// amp and filter env
// filter with reso, because aciiiieeeeeed

typedef struct {
    tCycle *subosc;
    Lfloat submix;
    tSawtooth *saw;
    Lfloat sawmix;
    tSquare *square;
    Lfloat squaremix;
    // tTwoPole *filter;
    tEnvelope *ampEnv; // env for amp
    // tEnvelope *filtEnv; // env for filter freq
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

    tCycle_init(&synth.subosc, &leaf);
    tCycle_setSampleRate(synth.subosc, SAMPLE_RATE);
    synth.submix = 0.25;

    tSawtooth_init(&synth.saw, &leaf);
    tSawtooth_setSampleRate(synth.saw, SAMPLE_RATE);
    synth.sawmix = 0.5;

    tSquare_init(&synth.square, &leaf);
    tSquare_setSampleRate(synth.square, SAMPLE_RATE);
    synth.squaremix = 0.5;

    assert(&synth.subosc);
    assert(&synth.saw);
    assert(&synth.square);

    tCycle_setFreq(synth.subosc, 0.0);
    tSawtooth_setFreq(synth.saw, 0.0);
    tSquare_setFreq(synth.square, 0.0);

    // tTwoPole_init(&synth.filter, &leaf);
    // assert(&synth.filter);

    tEnvelope_init(&synth.ampEnv, 0.05f, 0.9999f, 0, &leaf);
    assert(&synth.ampEnv);

    // tEnvelope_init(&synth.filtEnv, 0.05f, 0.9999f, 0, &leaf);
    // tEnvelope_on(synth.filtEnv, 60);
    // assert(&synth.filtEnv);
}

void noteOn(uint8_t midipitch, uint8_t velocity) {
    Lfloat freq;

    // if note_is_on, noteOff()...

    freq = LEAF_midiToFrequency(midipitch);
    tCycle_setFreq(synth.subosc, freq / 2);
    tSawtooth_setFreq(synth.saw, freq);
    tSquare_setFreq(synth.square, freq);

    tEnvelope_on(synth.ampEnv, velocity);
    // tEnvelope_on(synth.filtEnv, velocity);
}

void noteOff(uint8_t midipitch) {
    tCycle_setFreq(synth.subosc, 0.0);
    tSawtooth_setFreq(synth.saw, 0.0);
    tSquare_setFreq(synth.square, 0.0);

    // stop amp env -- no need with the simple A/D tEnvelope
    // stop filter env -- no need
}

void synthTick(int16_t *buf, int bufsiz) {
   Lfloat samp = 0.00f;

   for (int i = 0; i < bufsiz; i++) {
      samp = (synth.squaremix * tSquare_tick(synth.square)) + 
               (synth.sawmix * tSawtooth_tick(synth.saw)) +
               (synth.submix * tCycle_tick(synth.subosc));
      samp *= (0.9f * tEnvelope_tick(synth.ampEnv));

//     tSVF_setFreq(cowbell->bandpassOsc, cowbell->filterCutoff + 1000.0f * tEnvelope_tick(cowbell->envFilter));
//     sample = tSVF_tick(cowbell->bandpassOsc,sample);

       // tTwoPole_setB0(synth.filter, tEnvelope_tick(synth.filtEnv));
       // samp = tTwoPole_tick(synth.filter, samp);

      buf[i] = (int16_t)(samp * 32765.0); // convert to 16-bit integer
   }
}
