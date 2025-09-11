#ifndef ESP_TASK_H
#define ESP_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Task priorities
#define TASK_PRIORITY_HMI 3
#define TASK_PRIORITY_WIFI 2
#define TASK_PRIORITY_APP 1

// Task handles
extern TaskHandle_t xHMITaskHandle;
extern TaskHandle_t xWiFiTaskHandle;
extern TaskHandle_t xAppTaskHandle;

// Mutex for shared resources
extern SemaphoreHandle_t xVPMutex;

// Task functions
void TaskHMI(void *pvParameters);
void TaskWiFi(void *pvParameters);
void TaskApp(void *pvParameters);

// Notification values
#define NOTIFICATION_WIFI_CONFIG 0x01
#define NOTIFICATION_SUSPEND     0x02
#define NOTIFICATION_RESUME      0x03

#endif // ESP_TASK_H
