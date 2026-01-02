#include "wiener_linien.h"

String FixJsonMistake(String word) {
    word = Screen::ConvertGermanToLatin(word);
    // Check if the input string contains spaces
    word.trim();
    if (word.length() > 1 && word.indexOf(' ') == -1 && word.indexOf('-') == -1) {
        word[0] = toupper(word[0]);  // Convert the first letter to uppercase
        for (int i = 1; i < word.length(); i++) {
            word[i] = tolower(word[i]);  // Convert all letters to lowercase
        }
    }
  return word;
}

Monitor* findMonitor(std::vector<Monitor>& monitors, const String& line_name, const String& stop_name) {
    for (auto& m : monitors) {
        if (m.line == line_name && m.stop == stop_name) {
            return &m;
        }
    }
    return nullptr;
}

std::vector<Monitor> GetMonitorsFromHttp(const String& rbl_id) {
    std::vector<Monitor> monitors;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Couldn't fetch data - WiFi not connected.");
        return monitors;
    }
    String url = URL_BASE + rbl_id;

    // Print the URL to the serial port
    Serial.println("Fetching: " + url);

    // Start the HTTP request
    HTTPClient http;
    http.begin(url);

    // Get the status code of the response
    int httpCode = http.GET();

    // Check if the request was successful
    if (httpCode <= 0) {
        Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    } else if (httpCode == HTTP_CODE_OK) {
        // Stream parsing - NO String buffer!
        // WiFiClient& stream = http.getStream();
        // For big JSON Stream parsing does not work on ESP32
        String payload = http.getString();
#ifdef USE_ARDUINOJSON_V7
        JsonDocument root;
#else
        DynamicJsonDocument root(JSON_HEAP_SIZE);
#endif
        DeserializationError error = deserializeJson(root, payload, DeserializationOption::NestingLimit(64));
        if (error) {
            Serial.printf("JSON parsing error: %S\n", error.c_str());
            http.end();
            delay(config.settings.error_reset_delay);
            return monitors;
        }
        fillMonitorsFromJson(root, monitors);
        Serial.printf("Merged into %ld monitors.\n", monitors.size());

    } else {
        Serial.printf("HTTP Code: %d\n", httpCode);
    }
    http.end();
    return monitors;
}

void fillMonitorsFromJson(JsonDocument& root, std::vector<Monitor>& monitors) {
    if (root["data"].isNull()) return;
    const JsonObject data = root["data"];
    std::vector<TrafficInfo> traffic_infos;
    const JsonArray json_traffic_infos = data["trafficInfos"];
    if (!json_traffic_infos.isNull()) {
        for (const auto& json_traffic_info : json_traffic_infos) {
            struct TrafficInfo traffic_info;
            const JsonVariant json_title = json_traffic_info["title"];
            if (!json_title.isNull()) {
                traffic_info.title = Screen::ConvertGermanToLatin(json_title.as<String>());
            }
            const JsonVariant json_description = json_traffic_info["description"];
            if (!json_description.isNull()) {
                traffic_info.description = Screen::ConvertGermanToLatin(json_description.as<String>());
            }
            const JsonArray related_lines = json_traffic_info["relatedLines"];
            for (const auto& related_line : related_lines) {
                traffic_info.related_lines.push_back(related_line.as<String>());
            }
            traffic_infos.push_back(traffic_info);
        }
    }

    const JsonArray json_monitors = data["monitors"];
    Serial.printf("Received %d monitor from WinerLinien API\n", json_monitors.size());
    if (!json_monitors.isNull()) {
        for (const auto& json_monitor : json_monitors) {
            const JsonArray json_lines = json_monitor["lines"];
            const JsonObject json_stop = json_monitor["locationStop"]["properties"];
            String stop_name = Screen::ConvertGermanToLatin(json_stop["title"].as<String>());
            for (const auto& json_line : json_lines) {
                String line_name = json_line["name"].as<String>();
                String line_towards = FixJsonMistake(json_line["towards"].as<String>());
                bool line_barrierfree = json_line["barrierFree"].as<bool>();
                Monitor* monitor_p = findMonitor(monitors, line_name, stop_name);
                Monitor monitor;
                monitor.line = line_name;
                monitor.stop = stop_name;
                monitor.towards = line_towards;
                monitor.is_barrier_free = line_barrierfree;
                for (const auto& info : traffic_infos) {
                    if (info.hasRelatedLine(line_name)) {
                        if (monitor_p) {
                            monitor_p->traffic_info = info;
                        } else {
                            monitor.traffic_info = info;
                        }
                        break;
                    }
                }
                
                const JsonVariant json_departure = json_line["departures"]["departure"];
                if (json_departure.is<JsonArray>()) {
                    for (const auto& departure : json_departure.as<JsonArray>()) {
                        int countdown = -1;
                        const JsonObject departureTime = departure["departureTime"];
                        if (!departureTime.isNull()) {
                            Vehicle vehicle;
                            vehicle.countdown = departureTime["countdown"].as<int>();
                            const JsonObject json_vehicle = departure["vehicle"];
                            if (!json_vehicle.isNull()) {
                                vehicle.line = json_vehicle["name"].as<String>();
                                vehicle.towards = FixJsonMistake(json_vehicle["towards"].as<String>());
                                vehicle.is_barrier_free = json_vehicle["barrierFree"].as<bool>();
                                vehicle.has_folding_ramp = json_vehicle["foldingRamp"].as<bool>();
                            } else {
                                // take information in upper lavel
                                vehicle.line = monitor.line;
                                vehicle.towards = monitor.towards;
                                vehicle.is_barrier_free = monitor.is_barrier_free;
                                vehicle.has_folding_ramp = false;
                            }
                            if (monitor_p) {
                                //Monitor with line name already exists -> different towards
                                monitor_p->vehicles.push_back(vehicle);
                            } else {
                                monitor.vehicles.push_back(vehicle);
                            }
                        }
                    }
                    if (monitor_p) {
                        // sort vehicles by arriving time
                        std::sort(
                            monitor_p->vehicles.begin(), monitor_p->vehicles.end(),
                            [](const Vehicle& a, const Vehicle& b) {
                                return a.countdown < b.countdown;
                            }
                        );
                    }
                }
                if (!monitor_p) {
                    // New monitor with linename and stop
                    monitors.push_back(monitor);
                }
            }
        }

    }
}

/**
 * @brief Splits a string by a specified separator character.
 * @param data The input string to be split.
 * @param separator The character used for splitting.
 * @return A vector of substrings resulting from the split operation.
 */
std::vector<String> GetSplittedStrings(String data, char separator) {
  int separatorIndex = 0;
  std::vector<String> result;

  while (separatorIndex != -1) {
    separatorIndex = data.indexOf(separator);
    String chunk = data.substring(0, separatorIndex);
    result.push_back(chunk);
    if (separatorIndex != -1) {
      data = data.substring(separatorIndex + 1);
    }
  }

  return result;
}

/**
 * @brief Filters a vector of Monitor objects based on a filter string.
 * @param data The input vector of Monitor objects.
 * @param filter The filter string to match against Monitor names.
 * @return A vector containing Monitor objects that match the filter.
 */
std::vector<Monitor> GetFilteredMonitors(const std::vector<Monitor>& data, const String& filter) {
  if (filter.isEmpty()) {
    return data;
  }
  std::vector<String> filter_entries = GetSplittedStrings(filter, ',');
  std::vector<Monitor> result;

  for (const auto& monitor : data) {
      for (const auto& entry : filter_entries) {

          // Trim whitespace (optional, but recommended)
          String trimmed = entry;
          trimmed.trim();

          // Check if this entry contains '/'
          int slashIndex = trimmed.indexOf('/');

          if (slashIndex < 0) {
              // Entry is just "name"
              if (monitor.line == trimmed) {
                  result.push_back(monitor);
                  break;
              }
          } else {
              // Entry is "name/direction"
              String nameFilter = trimmed.substring(0, slashIndex);
              String directionFilter = trimmed.substring(slashIndex + 1);

              if (monitor.line == nameFilter && strncmp(monitor.towards.c_str(), directionFilter.c_str(), directionFilter.length()) == 0) {
                  result.push_back(monitor);
                  break;
              }
          }
      }
  }
  return result;
}
