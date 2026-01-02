#include "power_manager.h"

void PowerManager::begin(double brightness){
  _tft.begin();
  display_off();
  _tft.setRotation(1);
  _tft.fillScreen(COLOR_BG);
  setup_backlight_pwm();
  delay(50);
  backlight_on(brightness);
}


void PowerManager::toggle_backlight(){
    if (is_backlight_on()) {
        backlight_on();
    } else {
        backlight_off();
    }
}

double PowerManager::get_brightness(){
    if (use_bl_pwm) {
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
    if (use_bl_pwm) {
        return ledcRead(bl_pwm_channel) > 0; // at least 25% brightness
    } else {
        return digitalRead(TFT_BL) == TFT_BACKLIGHT_ON;
    }
}

void PowerManager::backlight_on(double brightness){
    if (!is_backlight_on()) {
        if (use_bl_pwm) {
            backlight_dim(brightness, DEFAULT_DURATION);
        } else {
            digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
        }
    }
}

void PowerManager::backlight_off(double brightness){
    if (is_backlight_on()) {
        if (use_bl_pwm) {
            backlight_dim(brightness, DEFAULT_DURATION);
        } else {
            digitalWrite(TFT_BL, LOW);
        }
    }
}

void PowerManager::setup_backlight_pwm() {
    //find an appropriate ledChannel
    if(use_bl_pwm){
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

void PowerManager::_backlight_fade_in(int target_level, int duration) {
    int brightness = ledcRead(bl_pwm_channel);
    int number_steps = abs(target_level - brightness);
    int step_delay = duration / number_steps;
    for (int dutyCycle = brightness; dutyCycle <= target_level; dutyCycle++) {
        ledcWrite(bl_pwm_channel, dutyCycle);
        delay(step_delay);
    }
}

void PowerManager::_backlight_fade_out(int target_level, int duration) {
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
        _backlight_fade_in(target_level, duration);
    } else if (current_brightness > target_level) {
        _backlight_fade_out(target_level, duration);
    }
}

void PowerManager::display_off() {
    backlight_off(0.0);

    // _tft.writecommand(TFT_SLPIN);

    // 3. Cut physical power to the LCD (GPIO 15)
    // This also kills the green power LED to save a few more mA
    // digitalWrite(15, LOW); 
}

void PowerManager::display_on() {
//   digitalWrite(15, HIGH);
//   delay(50); // Give the LCD controller time to wake up
//   tft.begin(); // You usually need to re-init the screen after GPIO 15 was LOW
    // _tft.writecommand(TFT_SLPOUT);
    Serial.printf("Truning brightness to: %lf\n", config.get_brightness());
    backlight_on(config.get_brightness());
}

void PowerManager::set_cpu_frequency(int f) {
    setCpuFrequencyMhz(f);
    if(use_bl_pwm){
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

void PowerManager::set_task(TaskHandle_t task){
    _task = task;
}

void PowerManager::task_suspend(){
    if(_task != nullptr){
        eTaskState tstate = eTaskGetState(_task);
        if(tstate == eRunning || tstate == eReady || tstate == eBlocked){
            vTaskSuspend(_task);
        }
    }
}

void PowerManager::task_resume(){
    if(_task != nullptr){
        eTaskState tstate = eTaskGetState(_task);
        if(tstate == eSuspended){
            vTaskResume(_task);
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
