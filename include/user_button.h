#ifndef __USER_BUTTON_H__
#define __USER_BUTTON_H__

#define TIMEOUT_LONG_PRESS (800)
#define TIMEOUT_SHORT_PRESS (250)

struct ButtonTaskConfig {
    uint8_t pin;
    Configuration* config;
    SemaphoreHandle_t semaphore;
    void (*isr)();
    void (*interrupt_handler_short)();
    void (*interrupt_handler_long)(unsigned long time_pressed);
    void (*interrupt_handler_double)();
};

extern ButtonTaskConfig button_1_cfg;
extern ButtonTaskConfig button_2_cfg;

void IRAM_ATTR handle_button_1_interrupt();
void IRAM_ATTR handle_button_2_interrupt();

class Button {
    public:
        Button(ButtonTaskConfig& config, uint8_t mode = INPUT_PULLUP);

        int read();

        bool is_pressed();

        void handle_presses();

        static void action(void* pvParameters);

    private:
        ButtonTaskConfig config;
};

#endif//__USER_BUTTON_H__