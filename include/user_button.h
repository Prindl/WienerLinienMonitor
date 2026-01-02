#ifndef __USER_BUTTON_H__
#define __USER_BUTTON_H__

#include "config.h"

#define TIMEOUT_LONG_PRESS (800)
#define TIMEOUT_SHORT_PRESS (250)

enum ButtonTransition {
    TR_NONE = -1,
    TR_PRESSED = 0,
    TR_RELEASED = 1
};

struct ButtonTaskConfig {
    uint8_t pin;
    Configuration* config;
    SemaphoreHandle_t semaphore;
    void (*isr)();
    void (*interrupt_handler_short)(Configuration& config);
    void (*interrupt_handler_long)(Configuration& config, unsigned long time_pressed);
    void (*interrupt_handler_double)(Configuration& config);
};

class Button {
    public:
        uint8_t state;

        Button(ButtonTaskConfig& config, uint8_t mode = INPUT_PULLUP);

        bool is_pressed();
        
        int read();

        void handle_presses();

    private:
        uint8_t pin;
        ButtonTaskConfig& config;
};

extern ButtonTaskConfig button_1_cfg;
extern ButtonTaskConfig button_2_cfg;

void IRAM_ATTR handle_button_1_interrupt();
void IRAM_ATTR handle_button_2_interrupt();

void ButtonTask(void* pvParameters);

#endif//__USER_BUTTON_H__