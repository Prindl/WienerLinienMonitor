#include "json.h"

time_t timegm(struct tm *const t){
    time_t ret;
    char *tz;

    // 1. Save the current timezone environment variable
    tz = getenv("TZ");
    if (tz) {
        tz = strdup(tz);
    }

    // 2. Set timezone to UTC
    setenv("TZ", "", 1);
    tzset();

    // 3. mktime now treats the struct as UTC
    ret = mktime(t);

    // 4. Restore the original timezone
    if (tz) {
        setenv("TZ", tz, 1);
        free(tz);
    } else {
        unsetenv("TZ");
    }
    tzset();

    return ret;
}