#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_5
#define BUTTON_PRESSED 0
#define DEBOUNCE_DELAY_MS 50

static volatile uint8_t ledState = 0;
static QueueHandle_t gpio_evt_queue = NULL;

// Безопасный обработчик прерывания
static void IRAM_ATTR button_isr_handler(void* arg) {
    static uint32_t last_interrupt_time = 0;
    static uint8_t last_button_state = !BUTTON_PRESSED;
    
    uint32_t current_time = xTaskGetTickCountFromISR();
    
    // Защита от дребезга
    if ((current_time - last_interrupt_time) > pdMS_TO_TICKS(DEBOUNCE_DELAY_MS)) {
        // Читаем состояние кнопки (разрешено)
        uint8_t current_button_state = gpio_get_level(BUTTON_PIN);
        
        // Обрабатываем только ИЗМЕНЕНИЕ состояния
        if (current_button_state != last_button_state) {
            // Если это нажатие (переход от отжатого к нажатому)
            if (current_button_state == BUTTON_PRESSED) {
                // Отправляем событие в очередь (безопасно)
                uint8_t data = 1;
                xQueueSendFromISR(gpio_evt_queue, &data, NULL);
            }
            
            last_button_state = current_button_state;
            last_interrupt_time = current_time;
        }
    }
}

// Задача для обработки событий из очереди
static void button_task(void* arg) {
    uint8_t data;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &data, portMAX_DELAY)) {
            // Безопасно обрабатываем вне прерывания
            ledState = !ledState;
            gpio_set_level(LED_PIN, ledState);
            ESP_LOGI("Button", "LED toggled: %s", ledState ? "ON" : "OFF");
        }
    }
}

void app_main(void) {
    // Создаем очередь для событий
    gpio_evt_queue = xQueueCreate(10, sizeof(uint8_t));
    
    // Настройка пинов
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    
    // Включаем подтяжку к питанию (для стабильности)
    gpio_pullup_en(BUTTON_PIN);
    gpio_pulldown_dis(BUTTON_PIN);
    
    // Настройка прерывания на ЛЮБОЕ изменение
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
    
    // Создаем задачу для обработки
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI("Main", "System started with improved interrupt handling");
    ESP_LOGI("Main", "Press the button to toggle LED");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}