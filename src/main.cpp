#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <limits>
#include <WiFiManager.h>  // by tzapu 2.0.16

#include "resources.h"
#include "config.h"
#include "user_button.h"
#include "wiener_linien.h"
#include "power_manager.h"

#define DEBUG
// #undef DEBUG

/* Function Declarations */
void task_screen_update(void*);
void task_data_update(void*);
void setup_wifi();

void action_dim(Configuration& config);
void action_eco_mode(Configuration& config, unsigned long time_pressed);
void action_reset(Configuration& config, unsigned long time_pressed);
void action_switch_layout(Configuration& config);

void activate_eco_mode();
void deactivate_eco_mode();

/* Global Variables */
Configuration config = Configuration();
SemaphoreHandle_t dataMutex = xSemaphoreCreateMutex();
PowerManager pm = PowerManager();
Screen* p_screen = nullptr;
TraficManager* pTraficManager = nullptr;

ButtonTaskConfig button_1_cfg;
ButtonTaskConfig button_2_cfg;

class SmartWatch {
    public:
        SmartWatch(const char* function_name) : function_name(function_name), start_millis(millis()) {

        }

        unsigned long GetExecution_ms() {
            unsigned long end_millis = millis();
            return end_millis - start_millis;
        }

        ~SmartWatch() {
#ifdef DEBUG
            Serial.print("Function '");
            Serial.print(function_name);
            Serial.print("' executed in ");
            Serial.print(GetExecution_ms());
            Serial.println(" milliseconds.");
#endif
        }

    private:
        const char* function_name;
        unsigned long start_millis;
};

/* Task Functions */

/**
 * @brief Updates data for a specific task.
 * @param pvParameters Pointer to task-specific parameters.
 */
void task_data_update(void* pvParameters) {
    std::vector<Monitor> allTrafficSetInit;
    const int& delay_planned_ms = config.settings.data_update_task_delay;
    int delay_real_ms = 0;
    const int max_retries_empty_data = 3;
    const int max_retries_emtpy_display_off = 3 * 5; //3 retries are one minute -> 5 minutes
    int cnt_empty_trafic_set = config.get_eco_mode_state() == ECO_AUTOMATIC_ON ? max_retries_emtpy_display_off : 0;
    
    while (true) {
        { //SmartWatch start
            SmartWatch sm(__FUNCTION__);
            // Fetch JSON data (may take several seconds)
            if (pm.is_eco_active() && config.get_eco_mode() == ECO_HEAVY) {
                if(config.get_eco_mode_state() == ECO_AUTOMATIC_ON) {
                    // turn wifi on to fetch data only in automatic eco mode
                    pm.wifi_start();
                    allTrafficSetInit = GetMonitorsFromHttp(config.get_stop_id());
                    pm.wifi_stop();
                }
            } else {
                allTrafficSetInit = GetMonitorsFromHttp(config.get_stop_id());
            }
            allTrafficSetInit = GetFilteredMonitors(allTrafficSetInit, config.get_lines_filter());
            if (allTrafficSetInit.empty()) {
                cnt_empty_trafic_set++;
#ifdef DEBUG
                Serial.printf("No Data received. Count: %d\n", cnt_empty_trafic_set);
#endif
            }
            // Acquire the data mutex
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                if (!allTrafficSetInit.empty() && !pTraficManager->has_data() ||
                     allTrafficSetInit.empty() &&  pTraficManager->has_data()) {
                    p_screen->clear();
                }
                if (!allTrafficSetInit.empty()) {
                    pTraficManager->update(allTrafficSetInit);
                    if(cnt_empty_trafic_set >= max_retries_emtpy_display_off && config.get_eco_mode_state() == ECO_AUTOMATIC_ON){
                        // Power saving module - only turn off automatically if it was enabled automatically
                        deactivate_eco_mode();
                    }
                    cnt_empty_trafic_set = 0; //Reset consecutive empty count
                } else if (cnt_empty_trafic_set >= max_retries_empty_data) {
                    // Update data as empty only after a set number of retries
                    pTraficManager->update(allTrafficSetInit);
                    // After 5 minutes of no data reception turn backlight off
                    if(cnt_empty_trafic_set >= max_retries_emtpy_display_off){
                        // Power saving module
                        config.set_eco_mode_state(ECO_AUTOMATIC_ON);
                        activate_eco_mode();
                    }
                }
                // Release the data mutex
                xSemaphoreGive(dataMutex);
            }
            int execution_ms = static_cast<int>(sm.GetExecution_ms());
            delay_real_ms = max(0, delay_planned_ms - execution_ms);
        }  //SmartWatch end
        vTaskDelay(pdMS_TO_TICKS(delay_real_ms));
    }
}

/**
 * @brief Updates the screen for a specific task.
 * @param pvParameters Pointer to task-specific parameters.
 */
void task_screen_update(void* pvParameters) {
    const int delay_ms = config.settings.screen_update_task_delay;
    while (true) {
        // Acquire the data mutex
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            pTraficManager->updateScreen();
            xSemaphoreGive(dataMutex); // Release the data mutex
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/**
 * @brief Manages Wi-Fi configuration using WiFiManager.
 */
void setup_wifi() {
    WiFiManager wifi_manager;
    wifi_manager.setDebugOutput(true);
    // wifi_manager.setCustomHeadElement(StringDatabase::GetStyleCSS().c_str());

    // Config is loaded in the setup
    
    // Create Wi-FiManager parameters
    const EcoMode eco_mode = config.get_eco_mode();
    String prompt_eco = StringDatabase::GetPowerModePrompt();
    WiFiManagerParameter param_eco("power_mode", prompt_eco.c_str(), String(eco_mode).c_str(), 2);

    const String& stop_id = config.get_stop_id();
    String prompt_rbl = StringDatabase::GetRBLPrompt();
    WiFiManagerParameter param_rbl("lines_rbl", prompt_rbl.c_str(), stop_id.c_str(), 64);

    const String& num_lines = String(config.get_number_lines());
    String prompt_num_lines = StringDatabase::GetLineCountPrompt(
        LIMIT_MIN_NUMBER_LINES,
        LIMIT_MAX_NUMBER_LINES,
        DEFAULT_NUMBER_LINES
    );
    WiFiManagerParameter param_count("lines_count", prompt_num_lines.c_str(), num_lines.c_str(), 64);

    const String& filter = config.get_lines_filter();
    String prompt_filter = StringDatabase::GetLineFilterPrompt();
    WiFiManagerParameter param_filter("lines_filter", prompt_filter.c_str(), filter.c_str(), 64);

    WiFiManagerParameter html_hline("<hr>");

    // Add parameters to Wi-FiManager
    wifi_manager.addParameter(&param_eco);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_rbl);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_count);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_filter);

    TFT_eSPI& tft = pm.get_tft();
    if (WiFi.psk().length() == 0) {
        // Show screen hint wifi
        tft.setTextSize(0);
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setCursor(0, 0, config.settings.instruction_font_size);
        tft.print(StringDatabase::GetInstructionsText());
    }

    // Attempt to connect to Wi-Fi
    bool isConnected = wifi_manager.autoConnect(StringDatabase::GetWiFissid().c_str());

    if (!isConnected) {
        ESP.restart();
    } else {
        tft.fillScreen(COLOR_BG);
        // Set the new values
        config.set_number_lines(String(param_count.getValue()).toInt());
        config.set_lines_filter(param_filter.getValue());
        config.set_stop_id(param_rbl.getValue());
        config.set_eco_mode(String(param_eco.getValue()).toInt());
    }
}

void task_watchdog_reboot(void* pvParameters) {
    const TickType_t task_delay = pdMS_TO_TICKS(1000);
    while (true) {
        if (millis() >= config.settings.ms_reboot_interval) {
            ESP.restart();
        }
        vTaskDelay(task_delay);
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
    if (time_pressed >= 1000) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE){
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
            xSemaphoreGive(dataMutex);
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

/* Eco Mode State Transitions Functions */

void activate_eco_mode() {
    pm.eco_mode_on();
    if (!pm.is_eco_active()) {
        config.set_eco_mode_state(ECO_ON);
    }
}

void deactivate_eco_mode() {
    pm.eco_mode_off();
    config.set_eco_mode_state(ECO_OFF);
}

/**
 * @brief Setup function for initializing the application.
 */
void setup() {
    // Initialize Serial communication
    Serial.begin(115200);

    Serial.println("Turning Bluetooth OFF...");
    pm.bluetooth_stop();

    //Loading the config
    config.load();

    Serial.println("Init TFT...");
    double brightness = config.get_brightness();
    // Start the screen with the specified brightness
    pm.begin(brightness);

    // Create a task for reading the reset button state
    Serial.println("Init reset actions...");
    button_2_cfg.config = &config;
    button_2_cfg.isr = &handle_button_2_interrupt;
    button_2_cfg.interrupt_handler_short = nullptr;
    button_2_cfg.interrupt_handler_long = &action_reset;
    button_2_cfg.interrupt_handler_double = nullptr;
    button_2_cfg.pin = (pm.get_tft().getRotation() == 1) ? GPIO_NUM_0 : GPIO_NUM_14;
    button_2_cfg.semaphore = xSemaphoreCreateBinary();
    xTaskCreate(ButtonTask, "ResetButtonTask", 2048 * 4, static_cast<void*>(&button_2_cfg), 1, NULL);

    // Create Watchdog to reset device every 24 hours
    xTaskCreate(task_watchdog_reboot, "WatchDogReboot", 2048 * 4, NULL, 1, NULL);

    setup_wifi();

    pTraficManager = new TraficManager;
    p_screen = new Screen(pm.get_tft(), config.get_number_lines(), TEXT_ROWS_PER_MONITOR);

    // Check if WiFi is successfully connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi!");
        delay(config.settings.error_reset_delay);
        ESP.restart();  // Restart the ESP32
    }
    if (config.get_eco_mode_state() != ECO_OFF) {
        pm.eco_mode_on();
    }

    // Configure User Buttons
    button_1_cfg.config = &config;
    button_1_cfg.isr = &handle_button_1_interrupt;
    button_1_cfg.interrupt_handler_short = &action_dim;
    button_1_cfg.interrupt_handler_long = &action_eco_mode;
    button_1_cfg.interrupt_handler_double = &action_switch_layout;
    button_1_cfg.pin = (pm.get_tft().getRotation() == 1) ? GPIO_NUM_14 : GPIO_NUM_0;
    button_1_cfg.semaphore = xSemaphoreCreateBinary();
    xTaskCreate(ButtonTask, "ButtonTask", 2048 * 4, static_cast<void*>(&button_1_cfg), 1, NULL);
    
    // Create tasks for data updating and screen updating
    xTaskCreate(task_data_update, "task_data_update", 2048 * 64, NULL, 2, NULL);
    TaskHandle_t screen_update_task_p;
    xTaskCreate(task_screen_update, "task_screen_update", 2048 * 16, NULL, 1, &screen_update_task_p);
    // Set the task to be suspended for eco_mode >= ECO_MEDIUM
    pm.set_task(screen_update_task_p);
}

/**
 * @brief Main loop function (not actively used in this application).
 */
void loop() {
  // This loop is intentionally left empty since the application is task-based.
}
