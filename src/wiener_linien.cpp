#include "screen.h"
#include "wiener_linien.h"


String WLDeparture::fix_json(String word) {
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

void WLDeparture::task_update(void * pvParameters) {
    WLDeparture* instance = (WLDeparture*)pvParameters;
    NetworkManager& network = NetworkManager::getInstance();
    char url[100] = URL_WIENER_LINIEN;
    const int pos = strlen(url);
    while(true) {
        // 1. SLEEP: Wait indefinitely for the Timer to notify this task
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(network.acquire() == pdTRUE) {
            HTTPClient https;
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Couldn't fetch data - WiFi not connected.");
                network.release();
                continue;
            }
            const String& rbl = config.get_rbl();
            if(rbl.isEmpty()){
                Serial.println("Couldn't fetch data - no RBL specified.");
                network.release();
                continue;
            }
            for(int i=pos; i<pos+rbl.length(); i++){
                url[i] = rbl[i-pos];
            }
            Serial.printf("Fetching: %s\n", url);

            // Start the HTTP request, reuse the SSL connection to avaoid failure because of heap fragmentation
            https.begin(instance->secure_client, url);
            int http_code = https.GET();

            // Check if the request was successful
            if (http_code == HTTP_CODE_OK) {
                DECLARE_JSON_DOC(root);
                WiFiClient& stream = https.getStream();
                DeserializationError error = deserializeJson(root, stream, DeserializationOption::NestingLimit(32));
                // String payload = https.getString();
                // DeserializationError error = deserializeJson(root, payload, DeserializationOption::NestingLimit(32));
                if (error) {
                    Serial.printf("JSON parsing error: %S\n", error.c_str());
                    https.end();
                    delay(config.settings.error_reset_delay);
                    network.release();
                    return;
                }
                if (xSemaphoreTake(instance->internal_mutex, portMAX_DELAY) == pdTRUE) {
                    instance->monitors.clear();
                    instance->fill_monitors_from_json(root);
                    xSemaphoreGive(instance->internal_mutex);
                    // Notify data coordinator of data update
                    if(instance->notification != nullptr){
                        xTaskNotifyGive(instance->notification);
                    } else {
                        Serial.println("Data update notification skipped.");
                    }
                }
                Serial.printf("Merged into %ld monitors.\n", instance->monitors.size());
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
                            vehicle.is_canceled = false;
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
        pdMS_TO_TICKS(config.settings.data_update_task_delay),
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
