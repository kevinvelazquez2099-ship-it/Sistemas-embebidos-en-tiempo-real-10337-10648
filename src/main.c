#include "stdio.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_freertos_hooks.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/timers.h"

// Integrantes: Kevin Veláquez, Eduardo González

#define Led 2
#define bot 4

// Configuración del ADC
#define ADC_RESOLUTION_BITS 12U
#define ADC_MAX_VALUE       ((1U << ADC_RESOLUTION_BITS) - 1U)
#define ADC_CH_Sensor       ADC_CHANNEL_6
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static const char *TAG = "ADC_READER";

//volatile bool g_botonPres = false; FREERTOS_USE_IDLE_HOOK

uint16_t adc;
bool bandera = true;
float voltaje = 0.0f;


TaskHandle_t Task_1 = NULL;
TaskHandle_t Task_2 = NULL;
TaskHandle_t Task_3 = NULL;
TimerHandle_t TiempoBloqueo = NULL;

bool Hook(void)
{
    if(bandera == false)
    {
        if(TiempoBloqueo != NULL)
        {
            if(xTimerIsTimerActive(TiempoBloqueo) == pdFALSE)
            {
                xTimerStart(TiempoBloqueo, 0);
                printf("Inicia cuenta de 5 segundos ->\n");
            }
        }
        if(gpio_get_level(bot) == false)
        {
            xTimerStop(TiempoBloqueo, 0);
            printf("Boton presionado, Mostrando adc ->\n");
            vTaskResume(Task_3);
            bandera = true;
        }
    }
   return true;
}

void vTimerCallback(TimerHandle_t xTimer)
{
    xTimerStop(TiempoBloqueo, 0);
    printf("Tiempo de bloqueo expirado, reanudando tarea 1\n");
    bandera = true;
    vTaskResume(Task_1);
}


uint16_t adc_reader_get_raw(adc_channel_t channel)
{
    if (s_adc_handle == NULL) {
        return 0U;
    }
    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, channel, &raw) == ESP_OK) {
        return (uint16_t)raw;
    }

    return 0U;
}

void adc_reader_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,     
        .bitwidth = ADC_BITWIDTH_DEFAULT,  
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CH_Sensor, &chan_cfg));

    ESP_LOGI(TAG, "ADC Inicializado con éxito");
}



void vTaskLedLento(void *pvParameters)
{
    while(1)
    {
        int Tiempo=0;
        printf("Tarea 2 led lento ->\n");
        while (Tiempo < 5) {
            gpio_set_level(Led, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);

            gpio_set_level(Led, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            Tiempo++;
        }
        bandera = false;
        vTaskSuspend(NULL);
    }
}
    
void vTaskLedRapido(void *pvParameters)
{
    while(1)
    {
        int Tiempo=0;
        printf("Tarea 1 led rapido ->\n");
        while (Tiempo < 25) {
            gpio_set_level(Led, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            gpio_set_level(Led, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            Tiempo++;
        }
        vTaskResume(Task_2);
        vTaskSuspend(NULL);
    }
}

void vTaskSensor(void *pvParameters)
{
    while(1)
    {
        printf("Tarea 3 sensor ->\n");
        adc = adc_reader_get_raw(ADC_CH_Sensor);
        voltaje = (float)adc * 3.3f / (float)ADC_MAX_VALUE;
        ESP_LOGI("Sensor", "Valor ADC: %u, Voltaje: %.2f V", adc, voltaje);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        vTaskResume(Task_1);
        vTaskSuspend(NULL);
    }
}



void app_main() 
{
    gpio_reset_pin(bot);
    gpio_set_direction(bot,GPIO_MODE_INPUT);
    gpio_set_pull_mode(bot, GPIO_PULLUP_ONLY);

    gpio_reset_pin(Led);
    gpio_set_direction(Led,GPIO_MODE_OUTPUT);

    adc_reader_init();
    xTaskCreate(vTaskLedRapido,"Led_Rapido",2048,NULL,2,&Task_1);
    xTaskCreate(vTaskLedLento,"Led_Lento",2048,NULL,2,&Task_2);
    xTaskCreate(vTaskSensor,"Sensor",2048,NULL,2,&Task_3);

    TiempoBloqueo = xTimerCreate("TiempoBloqueo", pdMS_TO_TICKS(5000), pdFALSE, NULL, vTimerCallback);

    esp_register_freertos_idle_hook(Hook);

    vTaskResume(Task_1);
    vTaskSuspend(Task_2);
    vTaskSuspend(Task_3);
}