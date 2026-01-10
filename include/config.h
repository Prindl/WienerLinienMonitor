#ifndef __CFG_H__
#define __CFG_H__

#include <Preferences.h>

#define TEXT_ROWS_PER_MONITOR 2

#define DEFAULT_NUMBER_LINES (2)
#define LIMIT_MIN_NUMBER_LINES (1)
#define LIMIT_MAX_NUMBER_LINES (3)

#define PREF_NUM_LINES ("SCREEN_LINES")
#define PREF_RBL_FILTER ("RBL_FILTER")
#define PREF_RBL ("STOP_ID")
#define PREF_EVA_FILTER ("EVA_FILTER")
#define PREF_EVA ("EVA_NR")
#define PREF_ECO_MODE ("ECO_MODE")
#define PREF_ECO_STATE ("ECO_STATE")
#define PREF_BRIGHTNESS ("BRIGHTNESS")

#define NS_SETTINGS ("Settings")

#define NUMBER_COUNTDOWNS (2)
#define SOFT_RESET_TIME (5000)
#define HARD_RESET_TIME (30000)
#define ERROR_RESET_DELAY (10000)
#define REBOOT_INTERVAL_MS (86400000ULL)
#define BUTTON_DELAY (100)
#define DATA_UPDATE_DELAY (20000)
#define SCREEN_UPDATE_DELAY (10)
#define ADDITIONAL_COUNTDOWN_DELAY (50)
#define INSTRUCTION_FONT_SIZE (4)
#define SCROLLRATE (2)
#define DELAY_SCROLL (1000)

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

class Configuration {
    private:
        Preferences db;
        int32_t ram_number_lines;
        /* Wiener Linien Configuration */
        String ram_rbl_filter;
        String ram_rbl;
        /* OEBB Configuration */
        String ram_eva_filter;
        String ram_eva;
        /* Power Mode Configuration */
        EcoMode ram_eco_mode;
        EcoModeState ram_eco_state;
        double ram_brightness;

        static int32_t verify_number_lines(int32_t count);

        explicit Configuration();
    public:
        static Configuration& getInstance();

        // Delete copy constructor and assignment operator
        Configuration(const Configuration&) = delete;
        void operator=(const Configuration&) = delete;

        
        void load();
        void clear();

        void begin(bool read_only = false);
        void end();

        void set_number_lines(int32_t value);
        int32_t get_number_lines();

        void set_rbl_filter(const String& value);
        const String& get_rbl_filter();

        void set_rbl(const String& value);
        const String& get_rbl();

        void set_eva_filter(const String& value);
        const String& get_eva_filter();

        void set_eva(const String& value);
        const String& get_eva();

        void set_eco_mode(EcoMode value);
        void set_eco_mode(int32_t value);
        EcoMode get_eco_mode();

        void set_eco_mode_state(EcoModeState value);
        void set_eco_mode_state(int32_t value);
        EcoModeState get_eco_mode_state();

        void set_brightness(double value);
        double get_brightness();
};

#endif // __CFG_H__