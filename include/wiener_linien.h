#ifndef __WIENER_LINIEN_H__
#define __WIENER_LINIEN_H__

#include <HTTPClient.h>
#include <WiFi.h>

#include "json.h"
#include "traffic.h"

#define URL_WIENER_LINIEN "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang&rbl="

class WLDeparture {
    private:
        WiFiClientSecure secure_client;
        SemaphoreHandle_t internal_mutex;
        TimerHandle_t handle_timer_update;
        TaskHandle_t handle_task_update;
        TaskHandle_t notification;
        std::vector<Monitor> monitors;
    
        String fix_json(const String& word);

        static void task_update(void * pvParameters);

        static void callback_timer_update(TimerHandle_t xTimer);

        void fill_monitors_from_json(JsonDocument& root);

    public:
        explicit WLDeparture();

        void setup();

        void set_notification(TaskHandle_t task);

        void get_latest_snapshot(std::vector<Monitor>& data);

};

#endif//__WIENER_LINIEN_H__