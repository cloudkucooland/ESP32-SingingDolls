#define I2S_MCLK_IO	   I2S_GPIO_UNUSED // not needed
#define I2S_BCLK_IO        GPIO_NUM_2      // I2S bit clock io number
#define I2S_WS_IO          GPIO_NUM_3      // I2S word select io number
#define I2S_DOUT_IO        GPIO_NUM_10     // I2S data out io number

#define SAMPLE_RATE 24000
#define AUDIO_BUFFSIZE 512 // in int32_t samples
#define LEAF_MEM_POOL_SIZE 4096

void start_leaf(void);
void stop_leaf(void);

void start_audio(void);
void stop_audio(void);

void initTables(void);

void noteOn(uint8_t, uint8_t);
void noteOff(uint8_t);
void synth_tick(int32_t *);
void setFilterFreq(float);
void setFilterAttack(float);
void setFilterDecay(float);
void setFilterSustain(float);
void setFilterRelease(float);
void setFilterResonance(float);
void setAmpAttack(float);
void setAmpDecay(float);
void setAmpSustain(float);
void setAmpRelease(float);
void setSubMix(float);
void setSawMix(float);
