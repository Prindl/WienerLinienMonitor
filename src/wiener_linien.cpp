#include "config.h"
#include "network_manager.h"
#include "screen.h"
#include "wiener_linien.h"

String WLDeparture::fix_json(const String& word) {
    String new_word = Screen::ConvertGermanToLatin(word);
    // Check if the input string contains spaces
    new_word.trim();
    if (new_word.length() > 1 && new_word.indexOf(' ') == -1 && new_word.indexOf('-') == -1) {
        new_word[0] = toupper(new_word[0]);  // Convert the first letter to uppercase
        for (int i = 1; i < new_word.length(); i++) {
            new_word[i] = tolower(new_word[i]);  // Convert all letters to lowercase
        }
    }
  return new_word;
}

void WLDeparture::task_update(void * pvParameters) {
    WLDeparture* instance = (WLDeparture*)pvParameters;
    NetworkManager& network = NetworkManager::getInstance();
    Configuration& config = Configuration::getInstance();
    JsonDocument filter;
    // To filter the huge json WL is providing
    if(filter["data"].isNull()){
        JsonObject filter_data = filter["data"].to<JsonObject>();

        JsonObject filter_monitors = filter_data["monitors"].add<JsonObject>();
        filter_monitors["locationStop"]["properties"]["title"] = true;

        JsonObject filter_lines = filter_monitors["lines"].add<JsonObject>();
        filter_lines["name"] = true;
        filter_lines["towards"] = true;
        filter_lines["barrierFree"] = true;
        filter_lines["foldingRamp"] = true;
        filter_lines["trafficjam"] = true;

        JsonObject filter_departure = filter_lines["departures"]["departure"].add<JsonObject>();
        filter_departure["departureTime"]["countdown"] = true;

        JsonObject filter_vehicle = filter_departure["vehicle"].to<JsonObject>();
        filter_vehicle["name"] = true;
        filter_vehicle["towards"] = true;
        filter_vehicle["barrierFree"] = true;
        filter_vehicle["foldingRamp"] = true;
        filter_vehicle["trafficjam"] = true;

        JsonObject filter_trafficInfos = filter_data["trafficInfos"].add<JsonObject>();
        filter_trafficInfos["title"] = true;
        filter_trafficInfos["description"] = true;
        filter_trafficInfos["relatedLines"] = true;
    }    
    char url[140] = URL_WIENER_LINIEN; // 11 RBLs reserved
    const int pos = strlen(url);
    // To switch away from stream parsing if it fails
    bool use_stream_parsing = true;
    while(true) {
        // 1. SLEEP: Wait indefinitely for the Timer to notify this task
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(network.acquire() == pdTRUE) {
            HTTPClient https;
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("Couldn't fetch data - WiFi not connected."));
                network.release();
                continue;
            }
            const String& rbl = config.get_rbl();
            int rbl_length = rbl.length();
            if(!rbl_length){
                Serial.println(F("Couldn't fetch data - no RBL specified."));
                network.release();
                continue;
            }
            for(int i = pos; i<pos+rbl_length; i++){
                url[i] = rbl[i-pos];
            }
            url[pos+rbl_length] = '\0';
            Serial.printf("Fetching: %s\n", url);

            // Start the HTTP request, reuse the SSL connection to avaoid failure because of heap fragmentation
            https.begin(instance->secure_client, url);
            int http_code = https.GET();

            // Check if the request was successful
            if (http_code == HTTP_CODE_OK) {
                JsonDocument root;
                // Stream parsing does not always work :/ (very large JSON this fails)
                DeserializationError error;
                if(use_stream_parsing){
                    error = deserializeJson(
                        root,
                        https.getStream(),
                        DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(16)
                    );
                } else {
                    error = deserializeJson(
                        root,
                        https.getString(),
                        DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(16)
                    );
                }
                if (error) {
                    Serial.printf("JSON parsing error: %S\n", error.c_str());
                    switch (error.code())
                    {
                        case DeserializationError::Code::InvalidInput:
                            // The first time Stream parsing fails, switch to String
                            use_stream_parsing = false;
                            break;
                        case DeserializationError::Code::NoMemory:
                            // Not enough memory to parse - restarting
                            Serial.println(F("Not enough memory to deserialise the json from WienerLinien API"));
                            delay(ERROR_RESET_DELAY);
                            ESP.restart();
                            break;
                        
                        default:
                            break;
                    }
                } else if (xSemaphoreTake(instance->internal_mutex, portMAX_DELAY) == pdTRUE) {
                    instance->monitors.clear();
                    instance->fill_monitors_from_json(root);
                    xSemaphoreGive(instance->internal_mutex);
                    // Notify data coordinator of data update
                    if(instance->notification != nullptr){
                        xTaskNotifyGive(instance->notification);
                    } else {
                        Serial.println(F("Data update notification skipped."));
                    }
                    Serial.printf("Merged into %ld monitors.\n", instance->monitors.size());
                }
            } else {
                Serial.printf("HTTP Code: %d\n", http_code);
                // Currently the software causes a lot of heap fragmentation
                // This causes the SSL connection to fail, because it needs a lot of continuous RAM 
                // To fix this the original software defined a reboot interval
                // If SSL fails, the connection will be refused
                if (http_code == HTTPC_ERROR_CONNECTION_REFUSED){
                    // We restart to reset the heap
                    ESP.restart();
                }
            }
            https.end();
            network.release();
        }
    }
}

void WLDeparture::callback_timer_update(TimerHandle_t xTimer) {
    // 3. Retrieve the 'this' pointer from the Timer ID
    WLDeparture* instance = (WLDeparture*)pvTimerGetTimerID(xTimer);
    if (instance != nullptr && instance->handle_task_update != nullptr) {
        xTaskNotifyGive(instance->handle_task_update);
    }
}

void WLDeparture::fill_monitors_from_json(JsonDocument& root) {
    if (root["data"].isNull()) return;
    Configuration& config = Configuration::getInstance();
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
        String rbl_filter = config.get_rbl_filter();
        std::vector<String> filters;
        if(rbl_filter.length()){
            int pos = 0, end = rbl_filter.indexOf(",", pos);
            if(end == -1){
                filters.push_back(rbl_filter);
            } else {
                do {
                    filters.push_back(rbl_filter.substring(pos, end));
                    pos = end+1;
                    end = rbl_filter.indexOf(",", pos);
                } while(end != -1);
                //Add the last elment
                filters.push_back(rbl_filter.substring(pos));
            }
        }
        for (const auto& json_monitor : json_monitors) {
            const JsonArray json_lines = json_monitor["lines"];
            const JsonObject json_stop = json_monitor["locationStop"]["properties"];
            String stop_name = Screen::ConvertGermanToLatin(json_stop["title"].as<String>());
            for (const auto& json_line : json_lines) {
                String line_name = json_line["name"].as<String>();
                bool filter_match = filters.empty();
                for(String filter : filters){
                    if(filter == line_name) {
                        filter_match = true;
                        break;
                    }
                }
                if(!filter_match){
                    continue;
                }
                String line_towards = fix_json(json_line["towards"].as<String>());
                bool line_barrierfree = json_line["barrierFree"].as<bool>();
                Monitor* monitor_p = findMonitor(this->monitors, line_name, stop_name);
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
                            vehicle.is_cancelled = false;
                            vehicle.is_airport = false;
                            vehicle.countdown = departureTime["countdown"].as<int>();
                            const JsonObject json_vehicle = departure["vehicle"];
                            if (!json_vehicle.isNull()) {
                                vehicle.line = json_vehicle["name"].as<String>();
                                vehicle.towards = fix_json(json_vehicle["towards"].as<String>());
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
                    this->monitors.push_back(monitor);
                }
            }
        }
    }
}

WLDeparture::WLDeparture() : internal_mutex(nullptr), handle_timer_update(nullptr), handle_task_update(nullptr), notification(nullptr){
    this->internal_mutex = xSemaphoreCreateMutex();
    this->secure_client = WiFiClientSecure();
    this->secure_client.setInsecure();
}

void WLDeparture::setup(){
    BaseType_t status = xTaskCreatePinnedToCore(
        &task_update,
        "task_update_wl",
        1024 * 24,
        this,
        2,
        &handle_task_update,
        APP_CPU_NUM
    );
    if(status != pdTRUE){
        Serial.printf("Could not create update task for WienerLinien: %d\n", status);
    }
    this->handle_timer_update = xTimerCreate(
        "timer_update_wl",
        pdMS_TO_TICKS(DATA_UPDATE_DELAY),
        pdTRUE,
        (void*)this,
        &callback_timer_update
    );

    if (this->handle_timer_update != NULL) {
        // Run the update function immediately once
        callback_timer_update(this->handle_timer_update);
        xTimerStart(this->handle_timer_update, 0);
    }    
}

void WLDeparture::set_notification(TaskHandle_t task){
    this->notification = task;
}

void WLDeparture::get_latest_snapshot(std::vector<Monitor>& data){
    data.clear();
    // Lock briefly just to copy the internal vector
    if (xSemaphoreTake(this->internal_mutex, portMAX_DELAY) == pdTRUE) {
        data.insert(data.end(), this->monitors.begin(), this->monitors.end());
        xSemaphoreGive(this->internal_mutex);
    }
}
