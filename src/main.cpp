#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <limits>
#include <WiFiManager.h>  // by tzapu 2.0.16

#include "colors.h"
#include "config.h"
#include "oebb.h"
#include "power_manager.h"
#include "resources.h"
#include "screen.h"
#include "user_button.h"
#include "wiener_linien.h"

#define DEBUG
#undef DEBUG

/* Function Declarations */
void task_data_coordinator(void* pvParameters);
void task_screen_update(void* pvParameters);

void action_dim(Configuration& config);
void action_eco_mode(Configuration& config, unsigned long time_pressed);
void action_reset(Configuration& config, unsigned long time_pressed);
void action_switch_layout(Configuration& config);
void action_reconfigure(Configuration& config);

void activate_eco_mode();
void deactivate_eco_mode();

/* Global Variables */
Configuration config = Configuration();

ButtonTaskConfig button_1_cfg;
ButtonTaskConfig button_2_cfg;

WLDeparture wl_departure = WLDeparture();
OEBBDeparture oebb_departure = OEBBDeparture();

/* Task Functions */

void task_data_coordinator(void* pvParameters) {
    TraficManager& traffic_manager = TraficManager::getInstance();
    static std::vector<Monitor> combined_data;
    static std::vector<Monitor> wl_data;
    static std::vector<Monitor> oebb_data;

    combined_data.reserve(32);
    wl_data.reserve(16);
    oebb_data.reserve(16);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Notification of new data received -> clear old data
        combined_data.clear();

        // Fetch the latest data, only if RBL/EVA are configured
        if(config.get_rbl().length()) {
            wl_departure.get_latest_snapshot(wl_data);
            combined_data.insert(combined_data.end(), wl_data.begin(), wl_data.end());
        }
        if(config.get_eva().length()) {
            oebb_departure.get_latest_snapshot(oebb_data);
            combined_data.insert(combined_data.end(), oebb_data.begin(), oebb_data.end());
        }
        
        // Apply global filters (e.g., hiding trains that are > 60 mins away)
        if (!combined_data.empty()) {
            if(traffic_manager.acquire() == pdTRUE){
                traffic_manager.update(combined_data);
                traffic_manager.release();
            }
            Serial.printf("[Master] Combined Update: %d monitors total.\n", combined_data.size());
        }
    }
}

void task_screen_update(void* pvParameters) {
    PowerManager& instance = PowerManager::getInstance();
    TraficManager& traffic_manager = TraficManager::getInstance();
    Screen& screen = Screen::getInstance();
    const int delay_ms = config.settings.screen_update_task_delay;
    while (true) {
        if(!instance.is_portal_active()) {
            //Only draw if the config protal is not active
            if(traffic_manager.acquire() == pdTRUE){
                if(screen.acquire() == pdTRUE){
                    // TrafficManager handles the mutex internally
                    traffic_manager.updateScreen();
                    screen.release();
                }
                traffic_manager.release();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
/* Button Action Callbacks */

void action_reset(Configuration& config, unsigned long time_pressed){
    WiFiManager wifi_manager;
    if (config.settings.soft_reset_time <= time_pressed && time_pressed < config.settings.hard_reset_time) {
        wifi_manager.resetSettings();
        ESP.restart();
    } else if (time_pressed >= config.settings.hard_reset_time){
        config.clear();
        wifi_manager.resetSettings();
        ESP.restart();
    }
}

void action_dim(Configuration& config){
    PowerManager& pm = PowerManager::getInstance();
    double brightness = config.get_brightness();
    if (25.0 < brightness && brightness <= 100.0) {
        brightness -= 25.0;
        config.set_brightness(brightness);
        pm.backlight_dim(brightness, 300);
    } else if (brightness == 25.0 || brightness == 0.0) {
        brightness = 100.0;
        config.set_brightness(brightness);
        pm.backlight_dim(brightness, 300);
    }
}

void action_eco_mode(Configuration& config, unsigned long time_pressed){
    TraficManager& traffic_manager = TraficManager::getInstance();
    if (time_pressed >= 1000) {
        if(traffic_manager.acquire() == pdTRUE){
            switch (config.get_eco_mode_state())
            {
                case ECO_OFF:
                    config.set_brightness(0.0);
                    activate_eco_mode();
                    break;
                case ECO_ON:
                case ECO_AUTOMATIC_ON:
                    config.set_brightness(100.0);
                    deactivate_eco_mode();
                    break;
                default:
                    break;
            }
            traffic_manager.release();
        }
    }
}

void action_switch_layout(Configuration& config){
    int num_lines = config.get_number_lines();
    if (num_lines == LIMIT_MAX_NUMBER_LINES) {
        config.set_number_lines(LIMIT_MIN_NUMBER_LINES);
    } else {
        config.set_number_lines(num_lines + 1);
    }
}

void action_reconfigure(Configuration& config){
    PowerManager& pm = PowerManager::getInstance();
    // Only reconfigure when eco mode is off
    // if(!pm.is_eco_active()){
        if(!pm.is_portal_active()){
            pm.notify_reconfiguration();
        } else{
            pm.deactivate_portal();
        }
    // }
}

/* Eco Mode State Transitions Functions */

void activate_eco_mode() {
    PowerManager& pm = PowerManager::getInstance();
    if (config.get_eco_mode() == ECO_HEAVY && oebb_departure.is_connected()){
        oebb_departure.close();
    }
    pm.eco_mode_on();
    config.set_eco_mode_state(ECO_ON);
}

void deactivate_eco_mode() {
    PowerManager& pm = PowerManager::getInstance();
    pm.eco_mode_off();
    config.set_eco_mode_state(ECO_OFF);
    if (config.get_eco_mode() == ECO_HEAVY && !oebb_departure.is_connected()){
        vTaskDelay(pdMS_TO_TICKS(1000));
        oebb_departure.setup();
    }
}

/**
 * @brief Setup function for initializing the application.
 */
void setup() {
    // Create the PowerManager + TrafficManager Instance
    PowerManager& pm = PowerManager::getInstance();
    TraficManager& tm = TraficManager::getInstance();
    // Initialize Serial communication
    Serial.begin(115200);

    // Check if PSRAM is available
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    if(!(chip_info.features & CHIP_FEATURE_EMB_PSRAM)) {
        Serial.println("No embedded PSRAM available.");
    }
    uint32_t psram_size = ESP.getPsramSize();
    if(psram_size > 0){
        Serial.printf("PSRAM detected, total size: %d bytes\n", psram_size);
        if (!esp_spiram_is_initialized()) {
            Serial.printf("PSRAM not initialised!\n");
        }
    }

    Serial.println("Turning Bluetooth OFF...");
    pm.bluetooth_stop();

    //Loading the config
    config.load();

    Serial.println("Init TFT...");
    double brightness = config.get_brightness();
    // Start the screen with the specified brightness
    pm.begin(brightness);

    // Create a task for reading the reset button state
    uint8_t screen_rotation = pm.get_tft().getRotation();
    Serial.println("Init reset actions...");
    button_2_cfg.config = &config;
    button_2_cfg.isr = (screen_rotation == 1) ? &handle_button_2_interrupt : &handle_button_1_interrupt ;
    button_2_cfg.interrupt_handler_short = (screen_rotation == 1) ? nullptr : &action_dim;
    button_2_cfg.interrupt_handler_long = (screen_rotation == 1) ? &action_reset : &action_eco_mode;
    button_2_cfg.interrupt_handler_double = (screen_rotation == 1) ? &action_reconfigure : &action_switch_layout;
    button_2_cfg.pin = (screen_rotation == 1) ? GPIO_NUM_0 : GPIO_NUM_14;
    button_2_cfg.semaphore = xSemaphoreCreateBinary();
    BaseType_t status = xTaskCreatePinnedToCore(
        Button::action,
        "task_btn_1",
        (screen_rotation == 1) ? 1024 * 8 : 1024 * 16,
        static_cast<void*>(&button_2_cfg),
        1,
        NULL,
        PRO_CPU_NUM
    );
    if(status != pdTRUE){
        Serial.printf("Could not create button 2 task: %d\n", status);
    }

    // Enter Power Manager Setup (Configure WiFi, RBL, EVA, etc.)
    pm.setup();

    //Sync with a time server
    TFT_eSPI& tft = pm.get_tft();
    tft.setCursor(0, 0, config.settings.instruction_font_size);
    tft.setTextColor(COLOR_TEXT_YELLOW, COLOR_BG);
    tft.println("\n\nTime Synchronisation...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Waiting for NTP time sync...");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
    } else {
        Serial.println("Time synchronized!");
    }

    // Check if WiFi is successfully connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi!");
        delay(config.settings.error_reset_delay);
        ESP.restart();  // Restart the ESP32
    }
    if (config.get_eco_mode_state() != ECO_OFF) {
        pm.eco_mode_on();
    }
    // Setup is finished, setup WienerLinien Timer
    wl_departure.setup();
    // Requires Internet to fetch station ID
    if(!pm.is_eco_active()){
        oebb_departure.setup();
    }

    // Configure User Buttons
    button_1_cfg.config = &config;
    button_1_cfg.isr = (screen_rotation == 1) ? &handle_button_1_interrupt :  &handle_button_2_interrupt;
    button_1_cfg.interrupt_handler_short = (screen_rotation == 1) ? &action_dim : nullptr;
    button_1_cfg.interrupt_handler_long = (screen_rotation == 1) ? &action_eco_mode : &action_reset;
    button_1_cfg.interrupt_handler_double = (screen_rotation == 1) ? &action_switch_layout : &action_reconfigure;
    button_1_cfg.pin = (screen_rotation == 1) ? GPIO_NUM_14 : GPIO_NUM_0;
    button_1_cfg.semaphore = xSemaphoreCreateBinary();
    status = xTaskCreatePinnedToCore(
        Button::action,
        "task_btn_2",
        (screen_rotation == 1) ? 1024 * 16 : 1024 * 8,
        static_cast<void*>(&button_1_cfg),
        1,
        NULL,
        PRO_CPU_NUM
    );
    if(status != pdTRUE){
        Serial.printf("Could not create button 1 task: %d\n", status);
    }
    
    // Create tasks for data updating and screen updating
    TaskHandle_t data_coordinator;
    status = xTaskCreatePinnedToCore(task_data_coordinator, "task_data_update", 1024 * 16, NULL, 2, &data_coordinator, APP_CPU_NUM);
    if (status == pdPASS) {
        wl_departure.set_notification(data_coordinator);
        oebb_departure.set_notification(data_coordinator);
    } else {
        Serial.printf("Could not create data coordinator task: %d\n", status);
    }
    pm.draw();
}

/**
 * @brief Main loop function (not actively used in this application).
 */
void loop() {
  // This loop is intentionally left empty since the application is task-based.
}
