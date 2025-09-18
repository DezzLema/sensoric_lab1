#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// Определение пинов
#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_5

// Параметры для подавления дребезга
#define DEBOUNCE_DELAY_MS 50
#define BUTTON_PRESSED 0 // Кнопка нажата (активный низкий уровень)

static bool ledState = false;
static bool buttonPressed = false;

// Функция для обработки нажатия кнопки с подавлением дребезга
static bool readButtonDebounced() {
    static uint32_t lastDebounceTime = 0;
    static bool lastButtonState = !BUTTON_PRESSED;
    static bool stableButtonState = !BUTTON_PRESSED;
    
    bool currentButtonState = gpio_get_level(BUTTON_PIN);
    
    // Если состояние кнопки изменилось
    if (currentButtonState != lastButtonState) {
        lastDebounceTime = xTaskGetTickCount();
    }
    
    // Если прошло достаточно времени после последнего изменения
    if ((xTaskGetTickCount() - lastDebounceTime) > pdMS_TO_TICKS(DEBOUNCE_DELAY_MS)) {
        // Если текущее состояние стабильно и отличается от предыдущего стабильного состояния
        if (currentButtonState != stableButtonState) {
            stableButtonState = currentButtonState;
            
            // Если кнопка нажата (учет активного низкого уровня)
            if (stableButtonState == BUTTON_PRESSED) {
                return true;
            }
        }
    }
    
    lastButtonState = currentButtonState;
    return false;
}

// Задача для управления светодиодом
static void ledTask(void* args) {
    while (true) {
        if (buttonPressed) {
            // Меняем состояние светодиода
            ledState = !ledState;
            gpio_set_level(LED_PIN, ledState);
            
            ESP_LOGI("LED", "LED state changed to: %s", ledState ? "ON" : "OFF");
            
            // Сбрасываем флаг нажатия
            buttonPressed = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    // Настройка пина светодиода как выхода
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0); // Выключить светодиод
    
    // Настройка пина кнопки как входа
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    
    ESP_LOGI("Main", "System started. Press button to toggle LED.");
    
    // Создаем задачу для управления светодиодом
    xTaskCreate(ledTask, "ledTask", 4096, NULL, 4, NULL);
    
    while (true) {
        // Проверяем нажатие кнопки с подавлением дребезга
        if (readButtonDebounced()) {
            buttonPressed = true;
            ESP_LOGI("Button", "Button pressed (debounced)");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}