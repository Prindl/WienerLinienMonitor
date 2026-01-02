#ifndef __WIENER_LINIEN_H__
#define __WIENER_LINIEN_H__

#include <ArduinoJson.h>  // by Benoit Blanchon 7.4.2
#include <HTTPClient.h>
#include <WiFi.h>

#include "traffic.h"

#define URL_BASE "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang&rbl="

#define USE_ARDUINOJSON_V7

#ifndef USE_ARDUINOJSON_V7
#define JSON_HEAP_SIZE (2048 * 16)
#endif

void fillMonitorsFromJson(JsonDocument& root, std::vector<Monitor>& monitors);
Monitor* findMonitor(std::vector<Monitor>& monitors, const String& line_name, const String& stop_name);
std::vector<Monitor> GetMonitorsFromHttp(const String& rbl_id);
std::vector<Monitor> GetFilteredMonitors(const std::vector<Monitor>&, const String&);
std::vector<String> GetSplittedStrings(String, char);
String FixJsonMistake(String);

#endif//__WIENER_LINIEN_H__