#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <WiFi.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>  // by Bodmer 2.5.43, user config 206

#include "esp_timer.h"

#include "config.h"

#define DEFAULT_DURATION 300
#define DEFAULT_PWM_FREQUENCY 5000
#define DEFAULT_PWM_RESOLUTION 8
#define DEFAULT_MAX_LEVEL 255 // 2^DEFAULT_PWM_RESOLUTION - 1

#define PARAM_ID_ECO "power_mode"
#define PARAM_ID_RBL "rbl"
#define PARAM_ID_FILTER_RBL "filter_rbl"
#define PARAM_ID_EVA "eva"
#define PARAM_ID_FILTER_EVA "filter_eva"
#define PARAM_ID_COUNT "lines_count"

extern Configuration config;

extern void task_screen_update(void* pvParameters);
class PowerManager {
    private:
        WiFiManager wifi_manager;
        TFT_eSPI _tft = TFT_eSPI();
        TaskHandle_t task_screen;
        TaskHandle_t task_reconfigure;
        // TimerHandle_t reboot_timer;
        esp_timer_handle_t handle_reboot_timer;
        int bl_pwm_channel;
        bool use_bl_pwm;
        bool _is_portal_active;
        
        void backlight_fade_in(int target_level = DEFAULT_MAX_LEVEL, int duration = DEFAULT_DURATION);
        
        void backlight_fade_out(int target_level = 0, int duration = DEFAULT_DURATION);

        void configure_wifi_manager();

        void save_wfi_manager_parameters(WiFiManager& wifi_manager);

        explicit PowerManager();
        
        void setup_backlight_pwm();

        static void task_config_portal(void *pvParameters);

        // static void reboot_timer_callback(TimerHandle_t xTimer);
        static void IRAM_ATTR reboot_timer_callback(void* arg);

    public:

        static PowerManager& getInstance();

        // Delete copy constructor and assignment operator
        PowerManager(const PowerManager&) = delete;
        void operator=(const PowerManager&) = delete;

        TFT_eSPI& get_tft(){
            return _tft;
        }

        void setup();

        BaseType_t screen_acquire();

        void screen_release();

        BaseType_t network_acquire();

        void network_release();

        void reconfigure();

        bool is_portal_active();

        void deactivate_portal();

        void begin(double brightness = 100.0);

        void draw();

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

        void notify_reconfiguration();

        void task_suspend();

        void task_resume();

        bool is_eco_active();

        void eco_mode_on();

        void eco_mode_off();
};

#endif // __POWER_MANAGER_H__