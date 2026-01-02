#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <WiFi.h>
#include <TFT_eSPI.h>  // by Bodmer 2.5.43, user config 206

#include "config.h"
#include "colors.h"

#define DEFAULT_DURATION 300
#define DEFAULT_PWM_FREQUENCY 5000
#define DEFAULT_PWM_RESOLUTION 8
#define DEFAULT_MAX_LEVEL 255 // 2^DEFAULT_PWM_RESOLUTION - 1

extern Configuration config;

class PowerManager {
    private:
        // TFT_eSPI& _tft;
        TFT_eSPI _tft = TFT_eSPI();
        TaskHandle_t _task;
        int bl_pwm_channel;
        bool use_bl_pwm;
        
        void _backlight_fade_in(int target_level = DEFAULT_MAX_LEVEL, int duration = DEFAULT_DURATION);
        
        void _backlight_fade_out(int target_level = 0, int duration = DEFAULT_DURATION);

    public:    

        PowerManager(): _task(nullptr), bl_pwm_channel(0), use_bl_pwm(false) {}

        ~PowerManager() {}

        TFT_eSPI& get_tft(){
            return _tft;
        }

        void begin(double brightness = 100.0);

        void setup_backlight_pwm();

        double get_brightness();

        bool is_dimming_enabled();

        void backlight_dim(double brightness, int duration);

        void toggle_backlight();

        bool is_backlight_on();

        void backlight_on(double brightness = 100.0);

        void backlight_off(double brightness = 0.0);

        void display_off();

        void display_on();

        void set_cpu_frequency(int f);

        void bluetooth_start();

        void bluetooth_stop();

        bool wifi_start();

        void wifi_stop();

        void set_task(TaskHandle_t task);

        void task_suspend();

        void task_resume();

        bool is_eco_active();

        void eco_mode_on();

        void eco_mode_off();
};

#endif // __POWER_MANAGER_H__