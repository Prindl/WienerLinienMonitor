#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#include "config.h"
#include "screen.h"

extern Configuration config;
extern Screen* p_screen;

template<typename T> std::vector<T> cyclicSubset(const std::vector<T>& input, size_t N, size_t start);

struct Vehicle {
    String line;
    String towards;
    int countdown;
    bool is_barrier_free;
    bool has_folding_ramp;
};

struct TrafficInfo {
    String title;
    String description;
    std::vector<String> related_lines;

    bool hasRelatedLine(const String& line) const {
        for (const auto& rl : related_lines) {
            if (rl == line) {
                return true;
            }
        }
        return false;
    }

    String GetFullString() const {
        return title + " - " + description;
    }

};

struct Monitor {
    String line;                      // Line name 2, 72, D etc.
    String stop;                      // Stop name Himmelmutterweg.
    String towards;                   // Tram directions.
    bool is_barrier_free;
    struct TrafficInfo traffic_info;  // Line alarm
    std::vector<Vehicle> vehicles;

    String GetFullDirection() const {
        return stop + " - " + towards;
    }

  };

class TrafficClock {
    private:
        const unsigned long kMillisecondsPerCountdown = 5000;
        const long kCountdownsPerIteration = 2;
        const long kIterationsPerHour = 2;
        unsigned long start_time_;
    
    public:
        TrafficClock(long ms_perCountdown, long cd_perIterations, long it_perHour);

        unsigned long Milliseconds() const;

        long GetTotalCountdown() const;

        long GetTotalIteration() const;

        void Reset();

        long GetCountdown() const;

        long GetIteration() const;

        long GetFullCycle() const;

        void PrintTime() const;
};

class TraficManager {
    private:
        std::vector<Monitor> all_trafic_set;
        int shift_cnt;
        int countdown_idx;
        // unsigned long previousMillisTraficSet;
        // unsigned long previousMillisCountDown;
        // const int INTERVAL_UPDATE = config.settings.data_update_task_delay;
        TrafficClock* p_trafic_clock;
        long prev_iterations = 0;


    public:
        int last_min_size = -1;  // Initialize last_min_size to an invalid value
        std::map<String, std::vector<String>> SplittedStringCache;

        TraficManager(): shift_cnt(0), countdown_idx(0), p_trafic_clock(nullptr) {}

        ~TraficManager();

        bool hasClock();

        void deleteClock();

        void createClock();

        bool has_data();

        void update(const std::vector<Monitor>& vec);

        void updateScreen();

        void SelectiveReset(const std::vector<Monitor>& currentTraficSubset, const std::vector<Monitor>& futureSubset);

        void sortTrafic(std::vector<Monitor>& v);

        String GetValidCountdown(const std::vector<Vehicle>& c, size_t index);

        void DrawTraficOnScreen(const std::vector<Monitor>& currentTrafficSubset);

        std::vector<String> getSplittedStringFromCache(const String& key_string);

        String getMaximumPosibleSingleNoScrollWord(const String& str);

        void splitStringToWords(const String& str, std::vector<String>& words);

        std::vector<String> splitToString(const String& input);
};

#endif//__TRAFFIC_H__