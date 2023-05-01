#include "spectrum_analyzer.h"
#include "spectrum_analyzer_worker.h"

#include <furry_hal.h>
#include <furry.h>

#include <lib/drivers/cc1101_regs.h>

struct SpectrumAnalyzerWorker {
    FurryThread* thread;
    bool should_work;

    SpectrumAnalyzerWorkerCallback callback;
    void* callback_context;

    uint32_t channel0_frequency;
    uint32_t spacing;
    uint8_t width;
    float max_rssi;
    uint8_t max_rssi_dec;
    uint8_t max_rssi_channel;

    uint8_t channel_ss[NUM_CHANNELS];
};

/* set the channel bandwidth */
void spectrum_analyzer_worker_set_filter(SpectrumAnalyzerWorker* instance) {
    uint8_t filter_config[2][2] = {
        {CC1101_MDMCFG4, 0},
        {0, 0},
    };

    // FURRY_LOG_D("SpectrumWorker", "spectrum_analyzer_worker_set_filter: width = %u", instance->width);

    /* channel spacing should fit within 80% of channel filter bandwidth */
    switch(instance->width) {
    case NARROW:
        filter_config[0][1] = 0xFC; /* 39.2 kHz / .8 = 49 kHz --> 58 kHz */
        break;
    case ULTRAWIDE:
        filter_config[0][1] = 0x0C; /* 784 kHz / .8 = 980 kHz --> 812 kHz */
        break;
    default:
        filter_config[0][1] = 0x6C; /* 196 kHz / .8 = 245 kHz --> 270 kHz */
        break;
    }
    furry_hal_subghz_load_registers((uint8_t*)filter_config);
}

static int32_t spectrum_analyzer_worker_thread(void* context) {
    furry_assert(context);
    SpectrumAnalyzerWorker* instance = context;

    FURRY_LOG_D("SpectrumWorker", "spectrum_analyzer_worker_thread: Start");

    // Start CC1101
    furry_hal_subghz_reset();
    furry_hal_subghz_load_preset(FurryHalSubGhzPresetOok650Async);
    furry_hal_subghz_set_frequency(433920000);
    furry_hal_subghz_flush_rx();
    furry_hal_subghz_rx();

    static const uint8_t radio_config[][2] = {
        {CC1101_FSCTRL1, 0x12},
        {CC1101_FSCTRL0, 0x00},

        {CC1101_AGCCTRL2, 0xC0},

        {CC1101_MDMCFG4, 0x6C},
        {CC1101_TEST2, 0x88},
        {CC1101_TEST1, 0x31},
        {CC1101_TEST0, 0x09},
        /* End  */
        {0, 0},
    };

    while(instance->should_work) {
        furry_delay_ms(50);

        // FURRY_LOG_T("SpectrumWorker", "spectrum_analyzer_worker_thread: Worker Loop");
        furry_hal_subghz_idle();
        furry_hal_subghz_load_registers((uint8_t*)radio_config);

        // TODO: Check filter!
        // spectrum_analyzer_worker_set_filter(instance);

        instance->max_rssi_dec = 0;

        // Visit each channel non-consecutively
        for(uint8_t ch_offset = 0, chunk = 0; ch_offset < CHUNK_SIZE;
            ++chunk >= NUM_CHUNKS && ++ch_offset && (chunk = 0)) {
            uint8_t ch = chunk * CHUNK_SIZE + ch_offset;
            furry_hal_subghz_set_frequency(instance->channel0_frequency + (ch * instance->spacing));

            furry_hal_subghz_rx();
            furry_delay_ms(3);

            //         dec      dBm
            //max_ss = 127 ->  -10.5
            //max_ss = 0   ->  -74.0
            //max_ss = 255 ->  -74.5
            //max_ss = 128 -> -138.0
            instance->channel_ss[ch] = (furry_hal_subghz_get_rssi() + 138) * 2;

            if(instance->channel_ss[ch] > instance->max_rssi_dec) {
                instance->max_rssi_dec = instance->channel_ss[ch];
                instance->max_rssi = (instance->channel_ss[ch] / 2) - 138;
                instance->max_rssi_channel = ch;
            }

            furry_hal_subghz_idle();
        }

        // FURRY_LOG_T("SpectrumWorker", "channel_ss[0]: %u", instance->channel_ss[0]);

        // Report results back to main thread
        if(instance->callback) {
            instance->callback(
                (void*)&(instance->channel_ss),
                instance->max_rssi,
                instance->max_rssi_dec,
                instance->max_rssi_channel,
                instance->callback_context);
        }
    }

    return 0;
}

SpectrumAnalyzerWorker* spectrum_analyzer_worker_alloc() {
    FURRY_LOG_D("Spectrum", "spectrum_analyzer_worker_alloc: Start");

    SpectrumAnalyzerWorker* instance = malloc(sizeof(SpectrumAnalyzerWorker));

    instance->thread = furry_thread_alloc();
    furry_thread_set_name(instance->thread, "SpectrumWorker");
    furry_thread_set_stack_size(instance->thread, 2048);
    furry_thread_set_context(instance->thread, instance);
    furry_thread_set_callback(instance->thread, spectrum_analyzer_worker_thread);

    FURRY_LOG_D("Spectrum", "spectrum_analyzer_worker_alloc: End");

    return instance;
}

void spectrum_analyzer_worker_free(SpectrumAnalyzerWorker* instance) {
    FURRY_LOG_D("Spectrum", "spectrum_analyzer_worker_free");
    furry_assert(instance);
    furry_thread_free(instance->thread);
    free(instance);
}

void spectrum_analyzer_worker_set_callback(
    SpectrumAnalyzerWorker* instance,
    SpectrumAnalyzerWorkerCallback callback,
    void* context) {
    furry_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void spectrum_analyzer_worker_set_frequencies(
    SpectrumAnalyzerWorker* instance,
    uint32_t channel0_frequency,
    uint32_t spacing,
    uint8_t width) {
    furry_assert(instance);

    FURRY_LOG_D(
        "SpectrumWorker",
        "spectrum_analyzer_worker_set_frequencies - channel0_frequency= %lu - spacing = %lu - width = %u",
        channel0_frequency,
        spacing,
        width);

    instance->channel0_frequency = channel0_frequency;
    instance->spacing = spacing;
    instance->width = width;
}

void spectrum_analyzer_worker_start(SpectrumAnalyzerWorker* instance) {
    FURRY_LOG_D("Spectrum", "spectrum_analyzer_worker_start");

    furry_assert(instance);
    furry_assert(instance->should_work == false);

    instance->should_work = true;
    furry_thread_start(instance->thread);
}

void spectrum_analyzer_worker_stop(SpectrumAnalyzerWorker* instance) {
    FURRY_LOG_D("Spectrum", "spectrum_analyzer_worker_stop");
    furry_assert(instance);
    furry_assert(instance->should_work == true);

    instance->should_work = false;
    furry_thread_join(instance->thread);
}