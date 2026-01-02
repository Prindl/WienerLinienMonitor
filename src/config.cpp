#include "config.h"

Configuration::Configuration() {}

void Configuration::load() {
    // Load values into ram
    this->begin(true);
    this->ram_number_lines = this->db.getInt(PREF_NUM_LINES, DEFAULT_NUMBER_LINES);
    this->ram_filter_lines = this->db.getString(PREF_FILTER_TRANSPORT);
    this->ram_stop_id = this->db.getString(PREF_STOP_ID);
    this->ram_eco_mode = static_cast<EcoMode>(this->db.getInt(PREF_ECO_MODE, ECO_LIGHT));
    this->ram_eco_state = static_cast<EcoModeState>(this->db.getInt(PREF_ECO_STATE, ECO_OFF));
    this->ram_brightness = this->db.getDouble(PREF_BRIGHTNESS, 100.0);
    this->end();
}

void Configuration::begin(bool read_only) {
    this->db.begin(NS_SETTINGS, read_only);
}

void Configuration::end() {
    this->db.end();
}

int32_t Configuration::verify_number_lines(int32_t value) {
    switch (value) {
      case LIMIT_MIN_NUMBER_LINES ... LIMIT_MAX_NUMBER_LINES:
        return value;
      default:
        return DEFAULT_NUMBER_LINES;
    }
}

void Configuration::set_number_lines(int32_t value) {
    this->ram_number_lines = this->verify_number_lines(value);
    this->begin();
    this->db.putInt(PREF_NUM_LINES, this->ram_number_lines);
    this->end();
}

int32_t Configuration::get_number_lines() {
    return this->ram_number_lines;
}

void Configuration::set_lines_filter(const String& value) {
    this->ram_filter_lines = value;
    this->begin();
    this->db.putString(PREF_FILTER_TRANSPORT, this->ram_filter_lines);
    this->end();
}

const String& Configuration::get_lines_filter() {
    return this->ram_filter_lines;
}

void Configuration::set_stop_id(const String& value) {
    this->ram_stop_id = value;
    this->begin();
    this->db.putString(PREF_STOP_ID, this->ram_stop_id);
    this->end();
}

const String& Configuration::get_stop_id() {
    return this->ram_stop_id;
}

void Configuration::set_eco_mode(EcoMode value) {
    this->ram_eco_mode = value;
    this->begin();
    this->db.putInt(PREF_ECO_MODE, static_cast<int32_t>(value));
    this->end();
}

void Configuration::set_eco_mode(int32_t value) {
    this->ram_eco_mode = static_cast<EcoMode>(value);
    this->begin();
    this->db.putInt(PREF_ECO_MODE, value);
    this->end();
}

EcoMode Configuration::get_eco_mode() {
    return this->ram_eco_mode;
}

void Configuration::set_eco_mode_state(EcoModeState value) {
    this->ram_eco_state = value;
    this->begin();
    this->db.putInt(PREF_ECO_STATE, static_cast<int32_t>(value));
    this->end();
}

void Configuration::set_eco_mode_state(int32_t value) {
    this->ram_eco_state = static_cast<EcoModeState>(value);
    this->begin();
    this->db.putInt(PREF_ECO_STATE, value);
    this->end();
}

EcoModeState Configuration::get_eco_mode_state() {
    return this->ram_eco_state;
}

void Configuration::set_brightness(double value) {
    this->ram_brightness = value;
    this->begin();
    this->db.putDouble(PREF_BRIGHTNESS, value);
    this->end();
}

double Configuration::get_brightness() {
    return this->ram_brightness;
}

void Configuration::clear() {
    this->begin();
    this->db.clear();
    this->end();
}