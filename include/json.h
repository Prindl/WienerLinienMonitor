#ifndef __JSON_H__
#define __JSON_H__

#include <ArduinoJson.h> // by Benoit Blanchon
#include <time.h>

time_t timegm(struct tm *const t);

namespace ArduinoJson {
    template <>
    struct Converter<time_t> {
        // This is called when you do: JsonVariant.as<time_t>()
        static time_t fromJson(JsonVariantConst variant) {
            const char* s = variant.as<const char*>();
            if (!s) return 0;

            struct tm t = {0};
            // Fast parsing for ISO8601 (YYYY-MM-DDTHH:MM:SSZ)
            if (sscanf(s, "%d-%d-%dT%d:%d:%dZ", 
                       &t.tm_year, &t.tm_mon, &t.tm_mday, 
                       &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
                return 0;
            }

            t.tm_year -= 1900; // Year since 1900
            t.tm_mon -= 1;     // Month 0-11
            t.tm_isdst = 0;
            return timegm(&t);
        }

        // This would be used if you were creating JSON from time_t
        static void toJson(JsonVariant variant, time_t value) {
            char buf[21];
            struct tm *t = gmtime(&value);
            strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
            variant.set(buf);
        }

        static bool check(JsonVariantConst variant) {
            return variant.is<const char*>();
        }
    };
}

#endif//__JSON_H__