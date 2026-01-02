#include "config.h"

int Configuration::verify_number_lines(int count) {
    switch (count) {
      case LIMIT_MIN_NUMBER_LINES ... LIMIT_MAX_NUMBER_LINES:
        return count;
      default:
        return DEFAULT_NUMBER_LINES;
    }
}

Configuration::Configuration() :
    number_text_lines(DEFAULT_NUMBER_LINES),
    eco_mode(ECO_LIGHT),
    eco_state(ECO_OFF),
    brightness(100.00)
{

}

Configuration::Configuration(const Configuration& other) :
    number_text_lines(other.number_text_lines),
    filter_lines(other.filter_lines),
    stop_id_lines(other.stop_id_lines),
    eco_mode(other.eco_mode),
    eco_state(other.eco_state),
    brightness(other.brightness)
{

}

Configuration& Configuration::operator=(const Configuration& other) {
    this->number_text_lines = other.number_text_lines;
    this->filter_lines = other.filter_lines;
    this->stop_id_lines = other.stop_id_lines;
    this->eco_mode = other.eco_mode;
    this->eco_state = other.eco_state;
    this->brightness = other.brightness;
    return *this;
}

bool Configuration::operator==(const Configuration& other) const {
    if (this == &other) {
        return true;
    }
    return this->number_text_lines == other.number_text_lines
        && this->filter_lines == other.filter_lines
        && this->stop_id_lines == other.stop_id_lines
        && this->eco_mode == other.eco_mode
        && this->eco_state == other.eco_state
        && this->brightness == other.brightness;
}

bool Configuration::operator!=(const Configuration& other) const {
    return !(*this == other);
}

void Configuration::set_number_lines(int num) {
    this->number_text_lines = verify_number_lines(num);
}

int Configuration::get_number_lines() const {
    return this->number_text_lines;
}

void Configuration::set_lines_filter(const String& filter) {
    this->filter_lines = filter;
}

const String& Configuration::get_lines_filter() const {
    return this->filter_lines;
}

void Configuration::set_stop_id(const String& id) {
    this->stop_id_lines = id;
}

const String& Configuration::get_stop_id() const {
    return this->stop_id_lines;
}

void Configuration::set_eco_mode(EcoMode mode) {
    this->eco_mode = mode;
}

void Configuration::set_eco_mode(int mode) {
    this->eco_mode = static_cast<EcoMode>(mode);
}

EcoMode Configuration::get_eco_mode() const {
    return this->eco_mode;
}

void Configuration::set_eco_mode_state(EcoModeState state) {
    this->eco_state = state;
}

void Configuration::set_eco_mode_state(int state) {
    this->eco_state = static_cast<EcoModeState>(state);
}

EcoModeState Configuration::get_eco_mode_state() const {
    return this->eco_state;
}

void Configuration::set_brightness(double brightness) {
    this->brightness = brightness;
}

double Configuration::get_brightness() const {
    return this->brightness;
}

void Configuration::save_file(const char *filename) {
#ifdef USE_ARDUINOJSON_V7
    JsonDocument json;
#else
    StaticJsonDocument<512> json;
#endif
    json[JSON_TAG_NUM_LINES] = this->number_text_lines;
    json[JSON_TAG_FILTER_TRANSPORT] = this->filter_lines;
    json[JSON_TAG_STOP_ID] = this->stop_id_lines;
    json[JSON_TAG_ECO_MODE] = this->eco_mode;
    json[JSON_TAG_ECO_STATE] = this->eco_state;
    json[JSON_TAG_BRIGHTNESS] = this->brightness;

    File file_config = SPIFFS.open(filename, "w");
    if (!file_config) {
        Serial.printf("Failed to open %s in for writing.\n", filename);
        return;
    }
#ifdef DBG_CONFIGURATION
    serializeJsonPretty(json, Serial);
#endif
    if (serializeJson(json, file_config) == 0) {
        Serial.printf("Failed to save %s.\n", filename);
    }
    file_config.close();
}

bool Configuration::load_file(const char *filename) {
    // SPIFFS.format(); // clean FS, for testing
    if (SPIFFS.exists(filename)) {
        File file_config = SPIFFS.open(filename, "r");
        if (!file_config) {
            Serial.printf("Failed to open %s in for reading.\n", filename);
            return false;
        }
#ifdef USE_ARDUINOJSON_V7
        JsonDocument json;
#else
        StaticJsonDocument<512> json;
#endif
        DeserializationError error = deserializeJson(json, file_config);
#ifdef DBG_CONFIGURATION
        serializeJsonPretty(json, Serial);
#endif
        if (!error) {
            this->number_text_lines = json[JSON_TAG_NUM_LINES].as<int>();
            this->filter_lines = json[JSON_TAG_FILTER_TRANSPORT].as<String>();
            this->stop_id_lines = json[JSON_TAG_STOP_ID].as<String>();
            this->eco_mode = json[JSON_TAG_ECO_MODE].as<EcoMode>();
            this->eco_state = json[JSON_TAG_ECO_STATE].as<EcoModeState>();
            this->brightness = json[JSON_TAG_BRIGHTNESS].as<double>();
            return true;
        } else {
            Serial.printf("Failed to load %s: %s\n", filename, error.c_str());
        }
        file_config.close();
    }
    return false;
}

void Configuration::delete_file(const char *filename) {
    if (SPIFFS.exists(filename)) {
        if (SPIFFS.remove(filename)) {
            Serial.printf("File %s successfully deleted.\n", filename);
        } else {
            Serial.printf("Failed to delete %s.\n", filename);
        }
    } else {
        Serial.printf("File %s doe snot exist.\n", filename);
    }
}
