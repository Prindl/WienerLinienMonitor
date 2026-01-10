#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#include <map>

template<typename T> std::vector<T> cyclicSubset(const std::vector<T>& input, size_t N, size_t start);

struct Vehicle {
    String line;
    String towards;
    String remark;
    int countdown;
    bool is_barrier_free;
    bool has_folding_ramp;
    bool is_cancelled;
    bool is_airport;
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
        if (title.length() && description.length()){
            return title + " - " + description;
        } else if (title.length()){
            return title;
        } else if (description.length()){
            return description;
        } else {
            return "";
        }
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
        SemaphoreHandle_t internal_mutex;
        int shift_cnt;
        int countdown_idx;
        TrafficClock* p_trafic_clock;
        long prev_iterations = 0;

        explicit TraficManager();
        ~TraficManager();

    public:
        static TraficManager& getInstance();

        // Delete copy constructor and assignment operator
        TraficManager(const TraficManager&) = delete;
        TraficManager& operator=(const TraficManager&) = delete;

        int last_min_size = -1;  // Initialize last_min_size to an invalid value
        std::map<String, std::vector<String>> SplittedStringCache;

        BaseType_t acquire();
        void release();

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

Monitor* findMonitor(std::vector<Monitor>& monitors, const String& line_name, const String& stop_name);

#endif//__TRAFFIC_H__