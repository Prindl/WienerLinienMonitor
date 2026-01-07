#ifndef __NETWORK_MANAGER_H__
#define __NETWORK_MANAGER_H__

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class NetworkManager {
public:
    // Singleton Accessor
    static NetworkManager& getInstance();

    // Prevent copying
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    /**
     * @brief Tries to acquire the network lock.
     * @return true if acquired, false if timeout occurred.
     */
    BaseType_t acquire();

    /**
     * @brief Releases the network lock.
     */
    void release();

private:
    NetworkManager(); // Private constructor
    SemaphoreHandle_t internal_mutex;
};

#endif//__NETWORK_MANAGER_H__
