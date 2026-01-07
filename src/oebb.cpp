#include "oebb.h"
#include "screen.h"

void OEBBDeparture::task_traffic(void *pvParameters) {
    OEBBDeparture* instance = (OEBBDeparture*)pvParameters;
    NetworkManager& network = NetworkManager::getInstance();
    while(true) {
        if (instance->web_socket.isConnected()) {
            // If connected, loop is lightweight (just keep-alive)
            instance->web_socket.loop();
        } else if (network.acquire()) {
            // If disconnected, loop() implies a heavy Reconnect Handshake!
            instance->web_socket.loop();
            network.release();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void OEBBDeparture::event(WStype_t type, uint8_t * payload, size_t length) {
    NetworkManager& network = NetworkManager::getInstance();
    if(type == WStype_TEXT){
        if(network.acquire() == pdTRUE){
            DECLARE_JSON_DOC(data);
            // Serial.printf("[WS] Live Update: %s\n", payload);
            DeserializationError error = deserializeJson(data, payload, DeserializationOption::NestingLimit(64));
            if (error) {
                Serial.printf("JSON parsing error: %S\n", error.c_str());
            } else if (data["method"].as<String>() == "update") {
                // Send response to server
                DECLARE_JSON_DOC(response);
                response["jsonrpc"] = data["jsonrpc"];
                response["result"] = JsonVariant();
                response["id"] = data["id"];
                response.shrinkToFit(); // Frees unused heap bytes before serialization
                String tmp;
                serializeJson(response, tmp);
                this->web_socket.sendTXT(tmp);
                if (xSemaphoreTake(this->internal_mutex, portMAX_DELAY) == pdTRUE) {
                    this->monitors.clear();
                    this->fill_monitors_from_json(data);
                    this->monitors.shrink_to_fit();
                    xSemaphoreGive(this->internal_mutex);
                    // Notify data coordinator of data update
                    xTaskNotifyGive(this->notification);
                }
                Serial.printf("Received %d monitor from OEBB API.\n", this->monitors.size());
            }
            network.release();
        }
    } else if(type == WStype_CONNECTED){
        Serial.printf("[WS] Connected. Protocol Switched.\n");
    } else if(type == WStype_DISCONNECTED){
        if(payload){
            Serial.printf("[WS] Disconnected: %s\n", payload);
        } else {
            Serial.printf("[WS] Disconnected.");
        }
    } else if(type == WStype_ERROR){
        Serial.printf("[WS] Error.\n");
    }
}

void OEBBDeparture::get_station() {
    NetworkManager& network = NetworkManager::getInstance();
    const String& eva = config.get_eva();
    char url[100] = URL_OEBB;
    const int pos = strlen(url);
    if(eva.length() > 0){
        if (network.acquire() == pdTRUE) {
            HTTPClient https;
            https.setReuse(true);
            https.setUserAgent(USR_AGENT);
            for(int i=pos; i<pos+eva.length(); i++){
                url[i] = eva[i-pos];
            }
            Serial.printf("Fetching: %s\n", url);

            https.begin(url);
            https.addHeader("Accept", "application/json, text/plain, */*");

            int http_code = https.GET();

            if (http_code > 0) {
                Serial.printf("[HTTP] GET... code: %d\n", http_code);
                if (http_code == HTTP_CODE_OK) {
                    String payload = https.getString();
                    DECLARE_JSON_DOC(root);
                    DeserializationError error = deserializeJson(root, payload, DeserializationOption::NestingLimit(64));
                    if (error) {
                        Serial.printf("JSON parsing error: %S\n", error.c_str());
                    } else {
                        if (!root["name"].isNull()){
                            this->station_name = root["name"].as<String>();
                        }
                        if (!root["plc"].isNull()){
                            this->station_id = root["plc"].as<String>();
                        }
                    }
                    Serial.printf("Station %s: %s\n", this->station_name, this->station_id);
                } else if (http_code == 304) {
                    Serial.println("[HTTP] 304: Data hasn't changed since your If-Modified-Since date.");
                }
            } else {
                Serial.printf("[HTTP] GET... failed, error: %s\n", https.errorToString(http_code).c_str());
            }
            https.end();
            network.release();
        }
    } else {
        this->station_name.clear();
        this->station_id.clear();
    }
}

void OEBBDeparture::fill_monitors_from_json(JsonDocument& root) {
    if (root["params"].isNull()) return;
    struct tm today;
    if(!getLocalTime(&today)){
        Serial.print("Couldn't get the correct time.");
    }
    time_t now = mktime(&today);
    const JsonObject data = root["params"]["data"];
    const JsonArray departures = data["departures"];
    const JsonArray notices = data["specialNotices"];
    if (!departures.isNull()) {
        for (const auto& departure : departures) {
            String line_name = departure["line"].as<String>();
            const JsonObject dst = departure["destination"];
            const JsonObject via = departure["via"];
            String towards = Screen::ConvertGermanToLatin(dst["default"].as<String>()) + " - Platform " + departure["track"].as<String>();
            String info = Screen::ConvertGermanToLatin(via["default"].as<String>());
            info.replace("&#8203;", "");
            String stop = this->station_name + " " + info;
            String time = dst["default"].as<String>();
            time_t scheduled = departure["scheduled"].as<time_t>();
            int minutes_to_depart = static_cast<int>(difftime(scheduled, now) / 60);

            
            Monitor* monitor_p = findMonitor(this->monitors, line_name, stop);
            Monitor monitor;
            monitor.line = line_name;
            monitor.stop = stop;
            monitor.towards = towards;
            monitor.is_barrier_free = false;

            // struct TrafficInfo traffic_info;
            // traffic_info.related_lines.push_back(line_name);
            // traffic_info.description = info;
            // if (monitor_p) {
            //     monitor_p->traffic_info = traffic_info;
            // } else {
            //     monitor.traffic_info = traffic_info;
            // }

            Vehicle vehicle;
            vehicle.countdown = minutes_to_depart;
            vehicle.line = monitor.line;
            vehicle.towards = monitor.towards;
            vehicle.is_barrier_free = monitor.is_barrier_free;
            vehicle.has_folding_ramp = false;
            if (monitor_p) {
                //Monitor with line name already exists -> different towards
                monitor_p->vehicles.push_back(vehicle);
            } else {
                monitor.vehicles.push_back(vehicle);
            }
            if (!monitor_p) {
                // New monitor with linename and stop
                this->monitors.push_back(monitor);
            }
        }
        for (auto& m: this->monitors) {
            // sort vehicles by arriving time
            std::sort(
                m.vehicles.begin(), m.vehicles.end(),
                [](const Vehicle& a, const Vehicle& b) {
                        return a.countdown < b.countdown;
                }
            );
        }
    }
}

OEBBDeparture::OEBBDeparture() : internal_mutex(nullptr), handle_task_traffic(nullptr), notification(nullptr) {

}

void OEBBDeparture::setup() {
    if(this->internal_mutex == nullptr){
        this->internal_mutex = xSemaphoreCreateMutex();
    }
    this->get_station();
    if (this->station_id.length() > 0){
        this->close();// If it was setup already, close the existing departure board
        String loader_url = "/abfahrtankunft/webdisplay/web_client/ws/?stationId=" + this->station_id + "&contentType=departure&staticLayout=true&page=1&offset=0&ignoreIncident=false&expandAll=false";
        this->web_socket.beginSSL("meine.oebb.at", 443, loader_url, "", "");
        this->web_socket.setExtraHeaders("Origin: https://meine.oebb.at");
        this->web_socket.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
            this->event(type, payload, length);
        });
        this->web_socket.setReconnectInterval(5000);
        if(this->handle_task_traffic == nullptr){
            BaseType_t status = xTaskCreatePinnedToCore(this->task_traffic, "WS_Task", 1024 * 24, this, 1, &handle_task_traffic, APP_CPU_NUM);
            if(status != pdTRUE){
                Serial.printf("Could not create update task for OEBB: %d\n", status);
            }
        } else {
            vTaskResume(this->handle_task_traffic);
        }
    } else {
        Serial.println("Setup of OEBB departure board failed.");
    }
}

void OEBBDeparture::close() {
    if (this->web_socket.isConnected()) {
        this->web_socket.disconnect();
    }
    if(this->handle_task_traffic != nullptr){
        vTaskSuspend(this->handle_task_traffic);
    }
}

bool OEBBDeparture::is_connected() {
    return this->web_socket.isConnected();
}

void OEBBDeparture::set_notification(TaskHandle_t task){
    this->notification = task;
}

void OEBBDeparture::get_latest_snapshot(std::vector<Monitor>& data){
    data.clear();
    // Lock briefly just to copy the internal vector
    if (xSemaphoreTake(this->internal_mutex, portMAX_DELAY) == pdTRUE) {
        data.insert(data.end(), this->monitors.begin(), this->monitors.end());
        xSemaphoreGive(this->internal_mutex);
    }
}
