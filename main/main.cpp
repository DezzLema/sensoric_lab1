#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_5
#define BUTTON_PRESSED 0
#define DEBOUNCE_DELAY_MS 25

static volatile uint32_t lastButtonChangeTime = 0;
static bool ledState = false;

// Простой обработчик прерывания - только запоминаем время изменения
static void IRAM_ATTR button_isr_handler(void* arg) {
    lastButtonChangeTime = xTaskGetTickCountFromISR();
}

// Задача для опроса состояния кнопки
static void buttonPollTask(void* args) {
    bool lastStableState = !BUTTON_PRESSED;
    uint32_t lastProcessedTime = 0;
    
    while (true) {
        uint32_t currentTime = xTaskGetTickCount();
        
        // Если было изменение состояния и прошло достаточно времени
        if (lastButtonChangeTime > lastProcessedTime && 
            (currentTime - lastButtonChangeTime) > pdMS_TO_TICKS(DEBOUNCE_DELAY_MS)) {
            
            bool currentState = gpio_get_level(BUTTON_PIN);
            
            // Обрабатываем только нажатие (переход от отжатого к нажатому)
            if (currentState == BUTTON_PRESSED && lastStableState != BUTTON_PRESSED) {
                ledState = !ledState;
                gpio_set_level(LED_PIN, ledState);
                ESP_LOGI("Button", "LED toggled: %s", ledState ? "ON" : "OFF");
            }
            
            lastStableState = currentState;
            lastProcessedTime = lastButtonChangeTime;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    
    // Прерывание на любом изменении состояния
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
    
    ESP_LOGI("Main", "System started with improved button handling");
    
    xTaskCreate(buttonPollTask, "buttonPoll", 4096, NULL, 5, NULL);
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}