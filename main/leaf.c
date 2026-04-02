#include <esp_random.h>
#include <leaf.h>

#include "sd.h"

typedef struct {
	tCycle *sine;
	tSawtooth *saw;
	tTwoPole *filter;
	tADSR *ampEnv; // env for amp
	tADSR *filtEnv; // env for filter freq

	float reso;
	float filterFreq;
} synth_t;

static LEAF leaf;
static __attribute__((aligned(8))) char leafMempool[LEAF_MEM_POOL_SIZE];

static float rando() {
	return (float)esp_random() / (float)UINT32_MAX;
};

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

	tTwoPole_init(&synth.filter, &leaf);
	assert(synth.filter);

	synth.reso = 0.25f;
	synth.filterFreq = 1000.0f;
	tTwoPole_setResonance(synth.filter, synth.filterFreq, synth.reso, 0); // 1kHz, mild reso

	tADSR_init(&synth.ampEnv,
		50.0f, // attack (ms)
		100.0f,	 // decay (ms)
		0.7f,	 // sustain (0–1)
		300.0f,	 // release (ms)
	   	&leaf);
	assert(synth.ampEnv);

	tADSR_init(&synth.filtEnv,
		40.0f,
		90.0f,
		0.01f,
		300.0f,
		&leaf);
	assert(synth.filtEnv);
}

void noteOn(uint8_t midipitch, uint8_t velocity) {
	Lfloat freq;

	if (leaf.sampleRate == 0) return;

	freq = LEAF_midiToFrequency(midipitch);
	tCycle_setFreq(synth.sine, freq * 0.5f);
	tSawtooth_setFreq(synth.saw, freq);

	float vel = velocity / 127.0f;
	tADSR_on(synth.ampEnv, vel);
	tADSR_on(synth.filtEnv, vel);
}

void noteOff(uint8_t midipitch) {
	if (leaf.sampleRate == 0) return;

	tADSR_off(synth.ampEnv);
	tADSR_off(synth.filtEnv);
}

void synth_tick(int32_t *buf) {
	static int controlStepCounter  = 0;
	static float smoothCutoff = 1000.0f;

	const int CONTROL_RATE = 1000;
	const int CONTROL_STEP = SAMPLE_RATE / CONTROL_RATE;
	
	for (int i = 0; i < AUDIO_BUFFSIZE; i++) {
	    float sub = tCycle_tick(synth.sine);
	    float saw = tSawtooth_tick(synth.saw);
	
	    float samp = (0.8f * saw) + (0.2f * sub);
	    samp *= 0.5f; // headroom
	
	    float filtEnv = tADSR_tick(synth.filtEnv);
	    if (++controlStepCounter >= CONTROL_STEP) {
	        controlStepCounter = 0;
	
	        float target = 100.0f + (synth.filterFreq * filtEnv);
	        smoothCutoff += 0.05f * (target - smoothCutoff);
	
	        tTwoPole_setResonance(synth.filter, smoothCutoff, synth.reso, 0);
	    }
	    samp = tTwoPole_tick(synth.filter, samp);

            float normCutoff = smoothCutoff / 10000.0f;
            float gainComp = 0.5f + 0.5f * normCutoff;
            samp *= gainComp;
	
	    float env = tADSR_tick(synth.ampEnv);
	    samp *= env;
	
	    int16_t s16 = (int16_t)(samp * 32767.0f);
	    int32_t s32 = ((int32_t)s16) << 16;
	
	    buf[2*i]     = s32;
	    buf[2*i + 1] = s32;
	}
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
