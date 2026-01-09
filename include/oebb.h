#ifndef __OEBB_H__
#define __OEBB_H__

#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h> // Library: WebSockets by Markus Sattler

#include "json.h"
#include "traffic.h"
#include "network_manager.h"

#define URL_OEBB "https://meine.oebb.at/abfahrtankunft/api/evaNrs/"

extern Configuration config;

class OEBBDeparture {
    private:
        WebSocketsClient web_socket;
        String station_name;
        String station_id;
        SemaphoreHandle_t internal_mutex;
        TaskHandle_t handle_task_traffic;
        TaskHandle_t notification;
        std::vector<Monitor> monitors;
        
        static void task_traffic(void *pvParameters);

        void event(WStype_t type, uint8_t * payload, size_t length);
        
        void get_station();

        void fill_monitors_from_json(JsonDocument& root);

    public:
        explicit OEBBDeparture();

        void setup();

        void close();

        bool is_connected();

        void set_notification(TaskHandle_t task);

        void get_latest_snapshot(std::vector<Monitor>& data);
};

#endif//__OEBB_H__