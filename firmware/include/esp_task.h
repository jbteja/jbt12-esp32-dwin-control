#ifndef ESP_TASK_H
#define ESP_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Task priorities
#define TASK_PRIORITY_HMI 3
#define TASK_PRIORITY_WIFI 2
#define TASK_PRIORITY_SYNC 1

// Task handles
extern TaskHandle_t xHMITaskHandle;
extern TaskHandle_t xWiFiTaskHandle;
extern TaskHandle_t xSyncTaskHandle;

// Mutex for shared resources
extern SemaphoreHandle_t xVPMutex;

// Queue for inter-task communication
extern QueueHandle_t xHMIUpdateQueue;

// Task functions
void TaskHMI(void *pvParameters);
void TaskWiFi(void *pvParameters);
void TaskSync(void *pvParameters);

#endif // ESP_TASK_H
