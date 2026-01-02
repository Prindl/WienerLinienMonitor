#include "user_button.h"

Button::Button(ButtonTaskConfig& button_cfg, uint8_t mode) : config(button_cfg), pin(button_cfg.pin), state(HIGH)
{
    pinMode(pin, mode);
    if(button_cfg.isr != nullptr){
        attachInterrupt(digitalPinToInterrupt(pin), button_cfg.isr, FALLING);
    }
}

bool Button::is_pressed() {
    return read() == LOW;
}

int Button::read() {
    return digitalRead(pin);
}

void Button::handle_presses() {
    Configuration& remote_cfg = *(this->config.config); 
    unsigned long last_press_time = 0;
    bool wait_for_double_press = false;

    while (true) {
        TickType_t wait_time = wait_for_double_press ? pdMS_TO_TICKS(TIMEOUT_SHORT_PRESS) : portMAX_DELAY;
        if (xSemaphoreTake(this->config.semaphore, wait_time) == pdPASS) {
            
            // 1. Check if it was a Press (Falling) or Release (Rising)
            if (this->is_pressed()) {
                // --- BUTTON PRESSED ---
                unsigned long press_start = millis();
                
                // Wait for release OR for long press threshold
                while (this->is_pressed()) {
                    if (millis() - press_start > TIMEOUT_LONG_PRESS) {
                        // Wait for user to let go so we don't trigger again
                        while(this->is_pressed()) vTaskDelay(10);
                        if (this->config.interrupt_handler_long) {
                            this->config.interrupt_handler_long(remote_cfg, millis() - press_start);
                        }
                        wait_for_double_press = false; // Cancel any pending double click
                        goto loop_end; 
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                // --- BUTTON RELEASED ---
                if (millis() - press_start > 50) { // Debounce check
                    if (wait_for_double_press) {
                        if (this->config.interrupt_handler_double) {
                            this->config.interrupt_handler_double(remote_cfg);
                        }
                        wait_for_double_press = false;
                    } else {
                        wait_for_double_press = true;
                        last_press_time = millis();
                    }
                }
            }
        } else {
            // --- TIMEOUT REACHED ---
            // xSemaphoreTake returned pdFALSE because TIMEOUT_SHORT_PRESS passed without a second press.
            if (wait_for_double_press) {
                if (this->config.interrupt_handler_short) {
                    this->config.interrupt_handler_short(remote_cfg);
                }
                wait_for_double_press = false;
            }
        }
        loop_end:; // Label for the goto
    }
}

// The ISR (Interrupt Service Routine)
// Needs the IRAM_ATTR to run from RAM for speed
void IRAM_ATTR handle_button_1_interrupt() {
    xSemaphoreGiveFromISR(button_1_cfg.semaphore, NULL);
}

void IRAM_ATTR handle_button_2_interrupt() {
    xSemaphoreGiveFromISR(button_2_cfg.semaphore, NULL);
}

void ButtonTask(void* pvParameters) {
    ButtonTaskConfig* button_cfg = (ButtonTaskConfig*)pvParameters;
    Button button = Button(*button_cfg);
    button.handle_presses();
}
