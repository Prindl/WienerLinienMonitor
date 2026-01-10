#include <Arduino.h>

#include "network_manager.h"

NetworkManager& NetworkManager::getInstance() {
    static NetworkManager instance;
    return instance;
}

NetworkManager::NetworkManager() {
    this->internal_mutex = xSemaphoreCreateMutex();
}

BaseType_t NetworkManager::acquire() {
    return xSemaphoreTake(this->internal_mutex, portMAX_DELAY);
}

void NetworkManager::release() {
    if (this->internal_mutex != NULL) {
        xSemaphoreGive(this->internal_mutex);
    }
}