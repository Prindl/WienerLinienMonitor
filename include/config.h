#ifndef __CFG_H__
#define __CFG_H__

#include <ArduinoJson.h>  // by Benoit Blanchon 6.21.3
#include <SPIFFS.h>

#define JSON_CONFIG_FILE "/configuration.json"
#define USE_ARDUINOJSON_V7

#define TEXT_ROWS_PER_MONITOR 2

#define DEFAULT_NUMBER_LINES (2)
#define LIMIT_MIN_NUMBER_LINES (1)
#define LIMIT_MAX_NUMBER_LINES (3) 

#define JSON_TAG_NUM_LINES ("NUMBER_TEXT_LINES")
#define JSON_TAG_FILTER_TRANSPORT ("FILTER_TRANSPORT")
#define JSON_TAG_STOP_ID ("STOP_ID")
#define JSON_TAG_ECO_MODE ("ECO_MODE")
#define JSON_TAG_ECO_STATE ("ECO_STATE")
#define JSON_TAG_BRIGHTNESS ("BRIGHTNESS")

#define DBG_CONFIGURATION
#undef DBG_CONFIGURATION

enum EcoMode {
   NO_ECO = 0,
   ECO_LIGHT,  // display OFF
   ECO_MEDIUM, // display OFF + task suspend and reduced CPU
   ECO_HEAVY   // display OFF + task suspend and reduced CPU + WiFi OFF
};

enum EcoModeState {
   ECO_OFF = 0,
   ECO_ON,  // Eco mode was manually turned on
   ECO_AUTOMATIC_ON, // Eco mode was automatically turned on
};

struct Settings {
  // The number queue. Indicates how many countdown shows per cycle.
  static constexpr int number_countdowns = 2;
  // Number of seconds to press the soft reset button to restart the device.
  static constexpr int soft_reset_time = 5000;
  // Number of seconds to press the hard reset button to restart the device.
  static constexpr int hard_reset_time = 30000;
  // Number of milliseconds to delay before resetting the device if an error.
  static constexpr int error_reset_delay = 10 * 1000;
  // Number of milliseconds to reset esp.
  static constexpr int ms_reboot_interval = 24 * 60 * 60 * 1000;
  // Number of milliseconds to delay between checking the reset button state.
  static constexpr int button_task_delay = 100;
  // Number of milliseconds to delay between updating the data.
  static constexpr int data_update_task_delay = 20 * 1000;
  // Number of milliseconds to delay between updating the screen.
  static constexpr int screen_update_task_delay = 10;
  // real count down can be faster thet real data_update_task_delay
  // this offset time help meke more smoth transition
  static constexpr int additional_countdown_delay = 50;
  // Size of font that show in first second plug in
  static constexpr int instruction_font_size = 4;
  // how much pixels scroll per frame
  static constexpr int scrollrate = 2;
  // how long to wait before starting scrolling in ms
  static constexpr int delay_scroll = 1000;
};

class Configuration {
    private:
        ///< The number of lines of text that can be displayed in each idx_row.
        int number_text_lines;

        ///< The raw lines filter. Example "D,2,6".
        String filter_lines;

        ///< The lines RBL.
        String stop_id_lines;

        // The power saving mode used
        EcoMode eco_mode;

        // The state of the eco mode
        EcoModeState eco_state;

        // The screens brightness
        double brightness;

        static int verify_number_lines(int count);

    public:
        static struct Settings settings;

        explicit Configuration();
        Configuration(const Configuration& other);

        Configuration& operator=(const Configuration& other);
        bool operator==(const Configuration& other) const;
        bool operator!=(const Configuration& other) const;

        void set_number_lines(int num);
        int get_number_lines() const;

        void set_lines_filter(const String& filter);
        const String& get_lines_filter() const;

        void set_stop_id(const String& id);
        const String& get_stop_id() const;

        void set_eco_mode(EcoMode mode);
        void set_eco_mode(int mode);
        EcoMode get_eco_mode() const;

        void set_eco_mode_state(EcoModeState state);
        void set_eco_mode_state(int state);
        EcoModeState get_eco_mode_state() const;

        void set_brightness(double brightness);
        double get_brightness() const;

        void save_file(const char *filename);
        bool load_file(const char *filename);

        static void delete_file(const char *filename);
};

#endif // __CFG_H__