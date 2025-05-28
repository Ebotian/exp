#include "ldr.h"
#include "esp_adc/adc_oneshot.h"

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_6 // GPIO34

static adc_oneshot_unit_handle_t ldr_adc_handle;

void ldr_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = LDR_ADC_UNIT,
    };
    adc_oneshot_new_unit(&init_cfg, &ldr_adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(ldr_adc_handle, LDR_ADC_CHANNEL, &chan_cfg);
}

int ldr_read(void) {
    int value = 0;
    adc_oneshot_read(ldr_adc_handle, LDR_ADC_CHANNEL, &value);
    return value;
}
