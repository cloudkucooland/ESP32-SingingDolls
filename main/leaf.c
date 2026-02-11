#include <esp_random.h>

#include <leaf.h>

static LEAF leaf;
static char leafMempool[2048];

static float rando() {return (float)rand()/RAND_MAX;}

void start_leaf(void) {
    // float audioBuffer[128];
    LEAF_init(&leaf, 48000, leafMempool, sizeof(leafMempool), &rando);
    // tCycle_init(&mySine, &leaf);
    // tCycle_setFreq(&mySine, 440.0);
}
