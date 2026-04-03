#include <esp_random.h>
#include <leaf.h>

#include "sd.h"

typedef struct {
	tCycle *sine;
	tSawtooth *saw;

	float subAmt;
        float sawAmt;

	tBiQuad *filter;
	tADSR *ampEnv; // env for amp
	tADSR *filtEnv; // env for filter freq

	float reso;
	float filterFreq;
	float oscFreq;
} synth_t;

static LEAF leaf;
static __attribute__((aligned(8))) char leafMempool[LEAF_MEM_POOL_SIZE];

static float rando() {
	return (float)esp_random() / (float)UINT32_MAX;
}

static synth_t synth;

void stop_leaf(void) {
	leaf_free(&leaf, leafMempool);
}

void start_leaf(void) {
	LEAF_init(&leaf, SAMPLE_RATE, leafMempool, LEAF_MEM_POOL_SIZE, &rando);

	tCycle_init(&synth.sine, &leaf);
	assert(synth.sine);
	tCycle_setFreq(synth.sine, 0.0);

	tSawtooth_init(&synth.saw, &leaf);
	tSawtooth_setSampleRate(synth.saw, SAMPLE_RATE);
	assert(synth.saw);
	tSawtooth_setFreq(synth.saw, 0.0);

	tBiQuad_init(&synth.filter, &leaf);
	assert(synth.filter);
	// tBiQuad_setEqualGainZeros(synth.filter); 
	tBiQuad_setSampleRate(synth.filter, SAMPLE_RATE);

	synth.reso = 0.5f;
	synth.filterFreq = SAMPLE_RATE * 0.45f; // start wide open
	tBiQuad_setResonance(synth.filter, synth.filterFreq, synth.reso, 1); 

	tADSR_init(&synth.ampEnv,
		20.0f, // attack (ms)
		200.0f,	 // decay (ms)
		0.9f,	 // sustain (0–1)
		200.0f,	 // release (ms)
	   	&leaf);
	assert(synth.ampEnv);

	tADSR_init(&synth.filtEnv,
		10.0f,
		150.0f,
		0.50f,
		100.0f,
		&leaf);
	assert(synth.filtEnv);

        synth.subAmt = 0.2;
        synth.sawAmt = 0.7;

        synth.oscFreq = 0.0f;
}

void noteOn(uint8_t midipitch, uint8_t velocity) {
	if (leaf.sampleRate == 0) return;
	
	synth.oscFreq = LEAF_midiToFrequency(midipitch);
	tCycle_setFreq(synth.sine, synth.oscFreq * 0.5f);
	tSawtooth_setFreq(synth.saw, synth.oscFreq);

	float vel = velocity / 127.0f;
	tADSR_on(synth.ampEnv, vel);
	tADSR_on(synth.filtEnv, vel);
}

void noteOff(uint8_t midipitch) {
	if (leaf.sampleRate == 0) return;

	tADSR_off(synth.ampEnv);
	tADSR_off(synth.filtEnv);
}

void setFilterFreq(float freq) {
	synth.filterFreq = freq;
}

void setFilterAttack(float a) {
	tADSR_setAttack(synth.filtEnv, a);
}

void setFilterDecay(float d) {
	tADSR_setDecay(synth.filtEnv, d);
}

void setFilterSustain(float s) {
	tADSR_setSustain(synth.filtEnv, s);
}

void setFilterRelease(float r) {
	tADSR_setRelease(synth.filtEnv, r);
}

void setFilterResonance(float r) {
	synth.reso = r;
}

void setAmpAttack(float a) {
	tADSR_setAttack(synth.ampEnv, a);
}

void setAmpDecay(float d) {
	tADSR_setDecay(synth.ampEnv, d);
}

void setAmpSustain(float s) {
	tADSR_setSustain(synth.ampEnv, s);
}

void setAmpRelease(float r) {
	tADSR_setRelease(synth.ampEnv, r);
}

void setSubMix(float m) {
	synth.subAmt = m;
}

void setSawMix(float m) {
	synth.sawAmt = m;
}

void synth_tick(int32_t *buf) {
    static int controlStepCounter  = 0;
    static float smoothCutoff = 1000.0f;

    const int CONTROL_RATE = 1000;
    const int CONTROL_STEP = SAMPLE_RATE / CONTROL_RATE;

    for (int i = 0; i < AUDIO_BUFFSIZE; i++) {
        // Oscillators
        float sub = tCycle_tick(synth.sine);
        float saw = tSawtooth_tick(synth.saw);

	// mix
        float samp = (synth.sawAmt * saw) + (synth.subAmt * sub);
        samp *= 0.7f; // headroom

        // Filter envelope
        float filtEnv = tADSR_tick(synth.filtEnv);

        if (++controlStepCounter >= CONTROL_STEP) {
            controlStepCounter = 0;

            // Pitch-based scaling
            float pitchFactor = synth.oscFreq / 1000.0f; 
            if (pitchFactor > 1.0f) pitchFactor = 1.0f; // clamp

            // Target cutoff = base + envelope contribution + pitch factor
            float targetCutoff = 50.0f + (synth.filterFreq * (0.2f + 0.8f * filtEnv) * (0.5f + 0.5f * pitchFactor));

            // Smooth cutoff to avoid zipper noise
            smoothCutoff += 0.1f * (targetCutoff - smoothCutoff);

            // Clamp cutoff to reasonable range
            if (smoothCutoff < 20.0f) smoothCutoff = 20.0f;
            if (smoothCutoff > SAMPLE_RATE * 0.45f) smoothCutoff = SAMPLE_RATE * 0.45f;

            // Clamp resonance
            float radius = fminf(synth.reso, 0.99f);
	    // float radius = synth.reso;

            tBiQuad_setResonance(synth.filter, smoothCutoff, radius, 1);
        }

        // Apply filter
        samp = tBiQuad_tick(synth.filter, samp);

        // Optional: gain compensation (boost higher cutoff)
        /* float normCutoff = smoothCutoff / 10000.0f;
        float gainComp = 0.5f + 0.5f * normCutoff;
        samp *= gainComp; */

        // Apply amplitude envelope
        float env = tADSR_tick(synth.ampEnv);
        samp *= env;

        // Convert to interleaved 32-bit buffer
        int16_t s16 = (int16_t)(samp * 32767.0f);
        int32_t s32 = ((int32_t)s16) << 16;

        buf[2*i]     = s32;
        buf[2*i + 1] = s32;
    }
}

