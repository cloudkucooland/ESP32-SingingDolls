#define I2S_BCLK_IO        GPIO_NUM_2      // I2S bit clock io number
#define I2S_WS_IO          GPIO_NUM_3      // I2S word select io number
#define I2S_DOUT_IO        GPIO_NUM_4      // I2S data out io number

#define SAMPLE_RATE 44100
#define AUDIO_BUFFSIZE 8192
#define LEAF_MEM_POOL_SIZE 4096

void start_leaf(void);
void stop_leaf(void);

void start_audio(void);
void stop_audio(void);

void noteOn(uint8_t, uint8_t);
void noteOff(uint8_t);
void synthTick(int16_t *, int);
