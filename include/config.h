#ifndef __CFG_H__
#define __CFG_H__

#include <Preferences.h>

#define TEXT_ROWS_PER_MONITOR 2

#define DEFAULT_NUMBER_LINES (2)
#define LIMIT_MIN_NUMBER_LINES (1)
#define LIMIT_MAX_NUMBER_LINES (3)

#define PREF_NUM_LINES ("SCREEN_LINES")
#define PREF_FILTER_TRANSPORT ("FILTER")
#define PREF_STOP_ID ("STOP_ID")
#define PREF_ECO_MODE ("ECO_MODE")
#define PREF_ECO_STATE ("ECO_STATE")
#define PREF_BRIGHTNESS ("BRIGHTNESS")

#define NS_SETTINGS ("Settings")

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
        Preferences db;
        int32_t ram_number_lines;
        String ram_filter_lines;
        String ram_stop_id;
        EcoMode ram_eco_mode;
        EcoModeState ram_eco_state;
        double ram_brightness;

        static int32_t verify_number_lines(int32_t count);

    public:
        static struct Settings settings;

        explicit Configuration();

        void set_number_lines(int32_t value);
        int32_t get_number_lines();

        void set_lines_filter(const String& value);
        const String& get_lines_filter();

        void set_stop_id(const String& value);
        const String& get_stop_id();

        void set_eco_mode(EcoMode value);
        void set_eco_mode(int32_t value);
        EcoMode get_eco_mode();

        void set_eco_mode_state(EcoModeState value);
        void set_eco_mode_state(int32_t value);
        EcoModeState get_eco_mode_state();

        void set_brightness(double value);
        double get_brightness();

        void begin(bool read_only = false);
        void end();

        void clear();
        void load();

};

#endif // __CFG_H__