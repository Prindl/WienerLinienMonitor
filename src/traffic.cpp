#include "screen.h"
#include "traffic.h"

TraficManager& TraficManager::getInstance() {
    static TraficManager instance;
    return instance;
}

TraficManager::TraficManager(): shift_cnt(0), countdown_idx(0), p_trafic_clock(nullptr) {
    this->internal_mutex = xSemaphoreCreateMutex();
    if (this->internal_mutex == NULL) {
        Serial.println("FATAL: Could not create TraficManager Mutex");
    }
}

Monitor* findMonitor(std::vector<Monitor>& monitors, const String& line_name, const String& stop_name) {
    for (auto& m : monitors) {
        if (m.line == line_name && m.stop == stop_name) {
            return &m;
        }
    }
    return nullptr;
}

TrafficClock::TrafficClock(long ms_perCountdown, long cd_perIterations, long it_perHour)
    : kMillisecondsPerCountdown(ms_perCountdown),
      kCountdownsPerIteration(cd_perIterations),
      kIterationsPerHour(it_perHour) {
    Reset();
}

unsigned long TrafficClock::Milliseconds() const {
    return millis();
}

long TrafficClock::GetTotalCountdown() const {
    unsigned long totalMilliseconds = Milliseconds();
    return totalMilliseconds / kMillisecondsPerCountdown;
}

long TrafficClock::GetTotalIteration() const {
    return GetTotalCountdown() / kCountdownsPerIteration;
}

void TrafficClock::Reset() {
    start_time_ = millis();
}

long TrafficClock::GetCountdown() const {
    long seconds = GetTotalCountdown() % kCountdownsPerIteration;
    return seconds;
}

long TrafficClock::GetIteration() const {
    long iterations = GetTotalIteration() % kIterationsPerHour;
    return iterations;
}

long TrafficClock::GetFullCycle() const {
    long cycle = GetTotalIteration() / kIterationsPerHour;
    return cycle;
}

void TrafficClock::PrintTime() const {
    Serial.print("Time: ");
    Serial.print(GetFullCycle());
    Serial.print(":");
    Serial.print(GetIteration());
    Serial.print(":");
    Serial.print(GetCountdown());
    Serial.print(".");
    Serial.println(Milliseconds());
}

template<typename T>
std::vector<T> cyclicSubset(const std::vector<T>& input, size_t N, size_t start) {
  std::vector<T> result;
  size_t size = input.size();

  // If the input vector is empty or N is 0, return an empty result.
  if (input.empty() || N == 0) {
    return result;
  }

  // Let's start with the start element and add elements to the result.
  for (size_t i = start; i < start + N; ++i) {
    result.push_back(input[i % size]);
  }

  return result;
}

TraficManager::~TraficManager() {
    deleteClock();
}

bool TraficManager::hasClock() {
    return p_trafic_clock != nullptr;
}

void TraficManager::deleteClock() {
    if (hasClock()) {
        delete p_trafic_clock;
        p_trafic_clock = nullptr;
    }
}

void TraficManager::createClock() {
    Screen& screen = Screen::getInstance();
    const int& cnt_countdows = config.settings.number_countdowns;
    double f_size;
    if (all_trafic_set.size() == 1) {
        if (all_trafic_set[0].vehicles.size() > 2*screen.GetNumberRows()) {
            f_size = 2*screen.GetNumberRows();
        } else {
            f_size = static_cast<double>(all_trafic_set[0].vehicles.size());
        }
    } else {
        f_size = static_cast<double>(all_trafic_set.size());
    }
    double f_sceen_cells = static_cast<double>(screen.GetNumberRows());

    long iterations_cnt = static_cast<long>(ceil(f_size / f_sceen_cells));

    long iteration_ms = config.settings.data_update_task_delay / iterations_cnt;
    long countdown_ms = iteration_ms / cnt_countdows;
    p_trafic_clock = new TrafficClock(
        countdown_ms + config.settings.additional_countdown_delay,
        cnt_countdows,
        iterations_cnt
    );

}

BaseType_t TraficManager::acquire(){
    return xSemaphoreTake(this->internal_mutex, portMAX_DELAY);
}

void TraficManager::release(){
    xSemaphoreGive(this->internal_mutex);
}

bool TraficManager::has_data() {
  return !all_trafic_set.empty();
}

// Block 1: Update Traffic Data
void TraficManager::update(const std::vector<Monitor>& vec) {
    Screen& screen = Screen::getInstance();
    prev_iterations = 0;
    if (vec.size() > 0){
        Serial.printf("Updating data with %d monitors:\n", vec.size());
        for (auto& monitor : vec) {
            Serial.printf("  Monitor for %s towards %s.\n", monitor.line, monitor.towards);
        }
    }
    if (hasClock()) {
        const int trafic_set_size = static_cast<int>(vec.size());
        if (hasClock()) p_trafic_clock->Reset();

        int rows_in_screen_cnt = std::min(screen.GetNumberRows(), trafic_set_size);
        auto currentTraficSubset = cyclicSubset(all_trafic_set, rows_in_screen_cnt, shift_cnt);
        auto futureTraficSubset = cyclicSubset(vec, rows_in_screen_cnt, shift_cnt);

        sortTrafic(currentTraficSubset);
        sortTrafic(futureTraficSubset);
        SelectiveReset(currentTraficSubset, futureTraficSubset);
    }
    all_trafic_set = vec;
}

void TraficManager::updateScreen() {
    Screen& screen = Screen::getInstance();
    if (!this->has_data()) {
        screen.DrawCenteredText("No Real-Time information available.");
        return;
    }
    const int32_t number_text_lines = config.get_number_lines();
    const int trafic_set_size = static_cast<int>(all_trafic_set.size());
    // Dynamic Row adjustment of screen
    if (trafic_set_size < number_text_lines) {
        if (trafic_set_size == 1) {
            if (all_trafic_set[0].vehicles.size() < number_text_lines) {
                screen.SetRowCount(all_trafic_set[0].vehicles.size());
                // deleteClock();
            } else {
                screen.SetRowCount(number_text_lines);
                // deleteClock();
            }
        } else {
            screen.SetRowCount(trafic_set_size);
            // deleteClock();
        }
    } else {
        screen.SetRowCount(number_text_lines);
        // deleteClock();
    }
    const int cnt_screen_rows = screen.GetNumberRows();
    int rows_in_screen_cnt = std::min(cnt_screen_rows, trafic_set_size);
    auto currentTraficSubset = cyclicSubset(all_trafic_set, rows_in_screen_cnt, shift_cnt);

    sortTrafic(currentTraficSubset);

    if (!hasClock()) {
        createClock();
    } else {
        long cur_iterations = p_trafic_clock->GetIteration();
        countdown_idx = p_trafic_clock->GetCountdown();
        if (cur_iterations != prev_iterations) {
            prev_iterations = cur_iterations;
            auto futureSubset = cyclicSubset(all_trafic_set, rows_in_screen_cnt, shift_cnt + cnt_screen_rows);

            sortTrafic(futureSubset);
            SelectiveReset(currentTraficSubset, futureSubset);
            shift_cnt += cnt_screen_rows;
        }
        DrawTraficOnScreen(currentTraficSubset);
    }
}

void TraficManager::SelectiveReset(const std::vector<Monitor>& currentTraficSubset, const std::vector<Monitor>& futureSubset) {
    Screen& screen = Screen::getInstance();
    if (currentTraficSubset.size() == futureSubset.size()) {

      size_t size = futureSubset.size();
      std::vector<bool> isNeedReset(size, true);
      for (size_t i = 0; i < size; ++i) {
        if (currentTraficSubset[i].traffic_info.description.isEmpty() || futureSubset[i].traffic_info.description.isEmpty()) {
          isNeedReset[i] = true;
        } else if (currentTraficSubset[i].traffic_info.description == futureSubset[i].traffic_info.description) {
          isNeedReset[i] = false;
        }
      }

      // update only for difrent names
      if (currentTraficSubset.size() > 1) {
        screen.SelectiveResetScroll(isNeedReset);
      }
    }
}

void TraficManager::sortTrafic(std::vector<Monitor>& v) {
    std::sort(
        v.begin(), v.end(),
        [](const Monitor& a, const Monitor& b) {
          return a.vehicles[0].countdown < b.vehicles[0].countdown;
        }
    );
}

String TraficManager::GetValidCountdown(const std::vector<Vehicle>& c, size_t index) {
    if (c.empty()) {
      return String();
    }

    size_t best_index = std::min(index, c.size() - 1);
    return String(c[best_index].countdown, DEC);
}

void TraficManager::DrawTraficOnScreen(const std::vector<Monitor>& currentTrafficSubset) {
    Screen& screen = Screen::getInstance();
    if (currentTrafficSubset.empty()) {
        screen.DrawCenteredText("No Real-Time information available.");
    } else {
        // Get the number of lines to display
        size_t numLines = screen.GetNumberRows();

        // Iterate through each line on the screen
        std::vector<ScreenEntity> vec_screen_entity;
        // generate entity
        for (size_t i = 0; i < numLines; ++i) {
            ScreenEntity monitor;
            std::vector<String> clean_str;
            bool accessibility = false;
            bool airport = false;
            bool ramp = false;
            for (size_t x = 0; x < TEXT_ROWS_PER_MONITOR; ++x) {
                clean_str.push_back("");
                clean_str.push_back("");
            }

            // Check if there's at least one monitor in the subset
            String towards_display;
            String traffic_info_display;
            if (currentTrafficSubset.size() == 1) {
                // Only one monitor - display vehicles instead
                const Monitor& currentMonitor = currentTrafficSubset[0];
                const auto& vehicles = currentMonitor.vehicles;
                int vehicle_idx;
                if (screen.GetNumberRows() == vehicles.size()) {
                    //Ignore countdown index because all data is displayed on one page
                    vehicle_idx = i;
                } else {
                    vehicle_idx = i + numLines*countdown_idx;
                }
                if (vehicle_idx < vehicles.size()) {
                    int has_traffic_info = currentMonitor.traffic_info.description.length();
                    if (has_traffic_info){
                        // traffic_info_display = currentMonitor.traffic_info.description;
                        traffic_info_display = currentMonitor.traffic_info.GetFullString();
                    } else {
                        traffic_info_display = currentMonitor.stop;
                    }                
                    if (!vehicles.empty()) {
                        const Vehicle& vehicle = currentMonitor.vehicles[vehicle_idx];
                        monitor.right_txt = vehicle.line;

                        if (vehicle.countdown <= 0) {
                            if ((millis() / 1000) % 2) {
                                monitor.left_txt = String("◱");
                            } else {
                                monitor.left_txt = String("◳");
                            }
                        } else {
                            monitor.left_txt = String(vehicle.countdown, DEC);
                        }
                        accessibility = vehicle.is_barrier_free;
                        airport = vehicle.is_airport;
                        ramp = vehicle.has_folding_ramp;
                        if (has_traffic_info) {
                            towards_display = currentMonitor.stop + " - " + vehicle.towards;
                        } else {
                            towards_display = vehicle.towards;
                        }
                    } else {
                        accessibility = currentMonitor.is_barrier_free;
                        if (has_traffic_info) {
                            towards_display = currentMonitor.stop + " - " + currentMonitor.towards;
                        } else {
                            towards_display = currentMonitor.towards;
                        }
                    }
                }
            } else {
                size_t monior_idx = i % currentTrafficSubset.size();
                // dont heve cyrcular arry
                if (i > currentTrafficSubset.size() - 1 && currentTrafficSubset.size() > 1) {
                    break;
                }

                const Monitor& currentMonitor = currentTrafficSubset[monior_idx];
                int has_traffic_info = currentMonitor.traffic_info.description.length();
                if (has_traffic_info){
                    // traffic_info_display = currentMonitor.traffic_info.description;
                    traffic_info_display = currentMonitor.traffic_info.GetFullString();
                } else {
                    traffic_info_display = currentMonitor.stop;
                }
                size_t idx = countdown_idx;
                if (idx > currentMonitor.vehicles.size() - 1) {
                    // If one monitor has more vehicles than the other, show the last element longer
                    idx = currentMonitor.vehicles.size() - 1;
                    // break;
                }
                // Set the right text
                monitor.right_txt = currentMonitor.line;

                if (!currentMonitor.vehicles.empty()) {
                    const Vehicle& vehicle = currentMonitor.vehicles[idx];
                    if (vehicle.countdown <= 0) {
                        if ((millis() / 1000) % 2) {
                          monitor.left_txt = String("◱");
                        } else {
                          monitor.left_txt = String("◳");
                        }
                    } else {
                        monitor.left_txt = GetValidCountdown(currentMonitor.vehicles, idx);
                    }
                    accessibility = vehicle.is_barrier_free;
                    ramp = vehicle.has_folding_ramp;
                    airport = vehicle.is_airport;
                    if (has_traffic_info) {
                        towards_display = currentMonitor.stop + " - " + vehicle.towards;
                    } else {
                        towards_display = vehicle.towards;
                    }
                  } else {
                    accessibility = currentMonitor.is_barrier_free;
                    if (has_traffic_info) {
                        towards_display = currentMonitor.stop + " - " + currentMonitor.towards;
                    } else {
                        towards_display = currentMonitor.towards;
                    }
                }
            }

            clean_str[0] = towards_display;
            if (traffic_info_display.length()) {
                clean_str[1] = traffic_info_display;
            }
            monitor.lines = clean_str;
            monitor.is_barrier_free = accessibility;
            monitor.has_folding_ramp = ramp;
            monitor.is_airport = airport;
            vec_screen_entity.push_back(monitor);
        }
        // render entity
        screen.SetRows(vec_screen_entity);
    }
}
