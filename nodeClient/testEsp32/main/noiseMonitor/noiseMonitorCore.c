
#include "noiseMonitorCore.h"
#include "../commands/noiseMonitor/commands.h"
#include "driver/adc.h"
#include "../client.h"



void initNoiseMonitor(){
    //Initialized ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4,ADC_ATTEN_DB_0);

}