#include "colors.h"
#include "network_manager.h"
#include "power_manager.h"
#include "resources.h"
#include "screen.h"


PowerManager::PowerManager() : task_screen(nullptr), bl_pwm_channel(0), use_bl_pwm(false), _is_portal_active(false) {

}

PowerManager& PowerManager::getInstance() {
    static PowerManager instance;
    return instance;
}

void PowerManager::configure_wifi_manager() {
    static String prompt_eco = StringDatabase::GetPowerModePrompt();
    static String val_eco    = String(config.get_eco_mode());
    
    static String prompt_rbl = StringDatabase::GetRBLPrompt();
    static String val_rbl    = config.get_rbl();
    
    static String prompt_eva = StringDatabase::GetEVAPrompt();
    static String val_eva    = config.get_eva();
    
    static String prompt_count = StringDatabase::GetLineCountPrompt(LIMIT_MIN_NUMBER_LINES, LIMIT_MAX_NUMBER_LINES, DEFAULT_NUMBER_LINES);
    static String val_count    = String(config.get_number_lines());
    
    static String prompt_filter_rbl = StringDatabase::GetRBLFilterPrompt();
    static String val_filter_rbl    = config.get_rbl_filter();
    
    static String prompt_filter_eva = StringDatabase::GetEVAFilterPrompt();
    static String val_filter_eva    = config.get_eva_filter();

    static WiFiManagerParameter param_eco(PARAM_ID_ECO, prompt_eco.c_str(), String(config.get_eco_mode()).c_str(), 2);
    static WiFiManagerParameter param_rbl(PARAM_ID_RBL, prompt_rbl.c_str(), config.get_rbl().c_str(), 64);
    static WiFiManagerParameter param_eva(PARAM_ID_EVA, prompt_eva.c_str(), config.get_eva().c_str(), 64);
    static WiFiManagerParameter param_count(
        PARAM_ID_COUNT,
        prompt_count.c_str(),
        String(config.get_number_lines()).c_str(),
        64
    );
    static WiFiManagerParameter param_filter_rbl(PARAM_ID_FILTER_RBL, prompt_filter_rbl.c_str(), config.get_rbl_filter().c_str(), 64);
    static WiFiManagerParameter param_filter_eva(PARAM_ID_FILTER_EVA, prompt_filter_eva.c_str(), config.get_eva_filter().c_str(), 64);
    static WiFiManagerParameter html_hline("<hr>");

    // 3. Add to manager
    wifi_manager.addParameter(&param_eco);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_rbl);
    wifi_manager.addParameter(&param_filter_rbl);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_eva);
    wifi_manager.addParameter(&param_filter_eva);
    wifi_manager.addParameter(&html_hline);
    wifi_manager.addParameter(&param_count);
}

void PowerManager::save_wfi_manager_parameters(WiFiManager& wifi_manager){
    WiFiManagerParameter** parameters = wifi_manager.getParameters();
    for(int i=0; i< wifi_manager.getParametersCount(); i++) {
        WiFiManagerParameter& parameter = *(parameters[i]);
        String id = parameter.getID();
        if (id == PARAM_ID_ECO){
            config.set_eco_mode(String(parameter.getValue()).toInt());
        } else if (id == PARAM_ID_RBL){
            config.set_rbl(parameter.getValue());
        } else if (id == PARAM_ID_EVA){
            config.set_eva(parameter.getValue());
        } else if (id == PARAM_ID_FILTER_RBL){
            config.set_rbl_filter(parameter.getValue());
        } else if (id == PARAM_ID_FILTER_EVA){
            config.set_eva_filter(parameter.getValue());
        } else if (id == PARAM_ID_COUNT){
            config.set_number_lines(String(parameter.getValue()).toInt());
        }
    }
}

void PowerManager::setup() {
    this->configure_wifi_manager();

    if (WiFi.psk().length() == 0) {
        this->_tft.fillScreen(COLOR_BG);
        this->_tft.setCursor(0, 0, config.settings.instruction_font_size);
        this->_tft.setTextColor(COLOR_TEXT_YELLOW, COLOR_BG);
        this->_tft.println("Setup Mode Active");
        this->_tft.println("------------------");
        this->_tft.println("Connect to WiFi:");
        this->_tft.setTextColor(COLOR_TEXT_GREEN);
        this->_tft.println(StringDatabase::GetWiFissid());
    }

    // Attempt to connect to Wi-Fi
    bool isConnected = wifi_manager.autoConnect(StringDatabase::GetWiFissid().c_str());

    if (!isConnected) {
        ESP.restart();
    } else {
        this->_tft.fillScreen(COLOR_BG);
        this->save_wfi_manager_parameters(wifi_manager);
    }

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &reboot_timer_callback,
        .name = "reboot_timer"
    };
    esp_timer_create(&periodic_timer_args, &handle_reboot_timer);
    uint64_t reboot_interval_us = config.settings.ms_reboot_interval * 1000ULL;
    esp_timer_start_once(
        handle_reboot_timer,
        reboot_interval_us
    );

    BaseType_t status = xTaskCreatePinnedToCore(
        this->task_config_portal,
        "task_config_portal",
        1024 * 16,
        this,
        1,
        &task_reconfigure,
        PRO_CPU_NUM
    );
    if(status != pdTRUE){
        Serial.printf("Could not create config portal task: %d\n", status);
    }
}

BaseType_t PowerManager::screen_acquire(){
    return Screen::getInstance().acquire();
}

void PowerManager::screen_release(){
     Screen::getInstance().release();
}

BaseType_t PowerManager::network_acquire(){
    return NetworkManager::getInstance().acquire();
}

void PowerManager::network_release(){
    NetworkManager::getInstance().release();
}

void PowerManager::reconfigure() {
    Screen& screen = Screen::getInstance();
    // wifi_manager.setSaveConfigCallback([this]() {
    wifi_manager.setSaveParamsCallback([this]() {
        Serial.println("Settings saved by user!");
        this->deactivate_portal();
    });
    // This starts the "Config Portal" on the current IP address
    wifi_manager.startWebPortal();
    if(screen.acquire() == pdTRUE){
        // Suspend Screen Updates 
        this->task_suspend();

        this->_tft.fillScreen(COLOR_BG);
        this->_tft.setCursor(0, 0, config.settings.instruction_font_size);
        this->_tft.setTextColor(COLOR_TEXT_YELLOW, COLOR_BG);
        this->_tft.println("Config Mode Active");
        this->_tft.println("------------------");
        this->_tft.println("Connect via Browser:");
        this->_tft.setTextColor(COLOR_TEXT_GREEN);
        this->_tft.println("http://" + WiFi.localIP().toString()); 
        this->_tft.setTextColor(COLOR_TEXT_YELLOW);
        this->_tft.println("\nPress [Button] twice to exit");

        this->_is_portal_active = true;
        while (this->_is_portal_active) {
            wifi_manager.process();
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        this->save_wfi_manager_parameters(wifi_manager);

        wifi_manager.stopWebPortal();

        this->_tft.fillScreen(COLOR_BG);
        this->_tft.setCursor(0,0);
        this->_tft.println("\n\nConnecting to WiFi...");

        if (!wifi_manager.autoConnect()) {
            ESP.restart();
        } else {
            this->_tft.fillScreen(COLOR_BG);
            this->task_resume();
        }
        screen.release();
    }
}

bool PowerManager::is_portal_active(){
    return wifi_manager.getWebPortalActive();
}

void PowerManager::deactivate_portal(){
    this->_is_portal_active = false;
}

void PowerManager::begin(double brightness){
  _tft.begin();
  display_off();
  _tft.setRotation(1);
  _tft.fillScreen(COLOR_BG);
  setup_backlight_pwm();
  delay(50);
  backlight_on(brightness);
}

void PowerManager::draw(){
    BaseType_t status = xTaskCreatePinnedToCore(task_screen_update, "task_screen_update", 1024 * 16, NULL, 1, &task_screen, APP_CPU_NUM);
    if (status != pdPASS) {
        Serial.printf("Could not create screen update task: %d\n", status);
    }
}

double PowerManager::get_brightness(){
    if (this->is_dimming_enabled()) {
        int current_level = ledcRead(bl_pwm_channel);
        double y = double(current_level) / DEFAULT_MAX_LEVEL;
        if (y <= 0.0) return 0.0;
        double brightness;
        if (y > 0.008856) {
            brightness = (116.0 * pow(y, 0.3333)) - 16.0;
        } else {
            brightness = 903.3 * y;
        }
        return fmax(0.0, fmin(100.0, brightness));
    } else {
        return digitalRead(TFT_BL) * 100.0;
    }
}

bool PowerManager::is_dimming_enabled(){
    return use_bl_pwm;
}

bool PowerManager::is_backlight_on(){
    if (this->is_dimming_enabled()) {
        return ledcRead(bl_pwm_channel) > 0; // at least 25% brightness
    } else {
        return digitalRead(TFT_BL) == TFT_BACKLIGHT_ON;
    }
}

void PowerManager::backlight_on(double brightness){
    if (!is_backlight_on()) {
        if (this->is_dimming_enabled()) {
            backlight_dim(brightness, DEFAULT_DURATION);
        } else {
            digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
        }
    }
}

void PowerManager::backlight_off(double brightness){
    if (is_backlight_on()) {
        if (this->is_dimming_enabled()) {
            backlight_dim(brightness, DEFAULT_DURATION);
        } else {
            digitalWrite(TFT_BL, LOW);
        }
    }
}

void PowerManager::setup_backlight_pwm() {
    //find an appropriate ledChannel
    if(this->is_dimming_enabled()){
        ledcDetachPin(TFT_BL);
        use_bl_pwm = false;
    }
    for (bl_pwm_channel = 0; bl_pwm_channel < 16; bl_pwm_channel++){
        if (ledcSetup(bl_pwm_channel, DEFAULT_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION) != 0){
            ledcAttachPin(TFT_BL, bl_pwm_channel);
            use_bl_pwm = true;
            break;
        }
    }
}

void PowerManager::backlight_fade_in(int target_level, int duration) {
    int brightness = ledcRead(bl_pwm_channel);
    int number_steps = abs(target_level - brightness);
    int step_delay = duration / number_steps;
    for (int dutyCycle = brightness; dutyCycle <= target_level; dutyCycle++) {
        ledcWrite(bl_pwm_channel, dutyCycle);
        delay(step_delay);
    }
}

void PowerManager::backlight_fade_out(int target_level, int duration) {
    int brightness = ledcRead(bl_pwm_channel);
    int number_steps = abs(target_level - brightness);
    int step_delay = duration / number_steps;
    for (int dutyCycle = brightness; dutyCycle >= target_level; dutyCycle--) {
        ledcWrite(bl_pwm_channel, dutyCycle);
        delay(step_delay);
    }
}

void PowerManager::backlight_dim(double brightness, int duration) {
    brightness = min(max(brightness, 0.0), 100.0);
    int current_brightness = ledcRead(bl_pwm_channel);
    int target_level = lround(pow(brightness/100.0, 2.8) * DEFAULT_MAX_LEVEL);
    if (current_brightness < target_level) {
        backlight_fade_in(target_level, duration);
    } else if (current_brightness > target_level) {
        backlight_fade_out(target_level, duration);
    }
}

void PowerManager::display_off() {
    backlight_off(0.0);
}

void PowerManager::display_on() {
    backlight_on(config.get_brightness());
}

void PowerManager::set_cpu_frequency(int f) {
    setCpuFrequencyMhz(f);
    if(this->is_dimming_enabled()){
        setup_backlight_pwm(); // Resync the PWM signal with updated cpu signal
    }
}

void PowerManager::bluetooth_start() {
    btStart();
}

void PowerManager::bluetooth_stop() {
    btStop();
}

bool PowerManager::wifi_start() {
    Serial.print("Reconnecting WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(); 
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 10) {
        delay(500);
        Serial.print(".");
        attempt++;
    }
    Serial.print("\n");
    return WiFi.status() == WL_CONNECTED;
}

void PowerManager::wifi_stop() {
  WiFi.disconnect(true);
  delay(10);
  WiFi.mode(WIFI_OFF);
}

void PowerManager::notify_reconfiguration(){
    xTaskNotifyGive(this->task_reconfigure);
}

void PowerManager::task_suspend(){
    if(task_screen != nullptr){
        eTaskState tstate = eTaskGetState(task_screen);
        if(tstate == eRunning || tstate == eReady || tstate == eBlocked){
            vTaskSuspend(task_screen);
        }
    }
}

void PowerManager::task_resume(){
    if(task_screen != nullptr){
        eTaskState tstate = eTaskGetState(task_screen);
        if(tstate == eSuspended){
            vTaskResume(task_screen);
        }
    }
}

bool PowerManager::is_eco_active() {
    EcoModeState eco_state = config.get_eco_mode_state();
    return eco_state == ECO_ON || eco_state == ECO_AUTOMATIC_ON;
}

void PowerManager::eco_mode_on() {
    switch (config.get_eco_mode())
    {
        case ECO_HEAVY:
            display_off();
            wifi_stop();
            task_suspend();
            set_cpu_frequency(80);
            break;
        case ECO_MEDIUM:
            display_off();
            task_suspend();
            set_cpu_frequency(80);
            break;
        case ECO_LIGHT:
            display_off();
            break;
        default:
            break;// Ignore invalid mode
        }
}

void PowerManager::eco_mode_off() {
    switch (config.get_eco_mode())
    {
        case ECO_HEAVY:
            set_cpu_frequency(240);
            task_resume();
            wifi_start();
            display_on();
            break;
        case ECO_MEDIUM:
            set_cpu_frequency(240);
            task_resume();
            display_on();
            break;
        case ECO_LIGHT:
            display_on();
            break;           
        default:
            break;// Ignore invalid mode
    }
}

void PowerManager::task_config_portal(void *pvParameters) {
    PowerManager* pm = (PowerManager*)pvParameters;

    while(true) {
        // Wait here indefinitely until the Button Task sends a notification
        // This uses 0% CPU while waiting
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        Serial.println("Config Task: Starting Portal...");

        if (config.get_eco_mode() == ECO_HEAVY) {
            pm->wifi_start();
        }
        if(pm->is_eco_active()) {
            pm->display_on();
        }
        pm->reconfigure();
        if(pm->is_eco_active()) {
            pm->display_off();
        }
        if (config.get_eco_mode() == ECO_HEAVY) {
            pm->wifi_stop();
        }
    }
}

// void PowerManager::reboot_timer_callback(TimerHandle_t xTimer) {
void IRAM_ATTR PowerManager::reboot_timer_callback(void* arg) {
    Serial.println("Reboot timer expired. Restarting ESP32...");
    ESP.restart();
}
