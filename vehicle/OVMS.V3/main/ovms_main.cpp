#include "ovms_log.h"
static const char *TAG = "ovms_main";

#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include "ovms.h"
#include "ovms_peripherals.h"
#include "ovms_housekeeping.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_module.h"
#include "ovms_boot.h"
#include <esp_task_wdt.h>

extern "C"
  {
  void app_main(void);
  }

static class FrameworkInit
  {
  public:
    inline FrameworkInit()
      {
      ESP_LOGI(TAG, "Set default logging level for * to %s",
        CONFIG_LOG_DEFAULT_LEVEL == 5 ? "VERBOSE" :
        CONFIG_LOG_DEFAULT_LEVEL == 4 ? "DEBUG" :
        CONFIG_LOG_DEFAULT_LEVEL == 3 ? "INFO" :
        CONFIG_LOG_DEFAULT_LEVEL == 2 ? "WARN" :
        CONFIG_LOG_DEFAULT_LEVEL == 1 ? "ERROR" : "None");
      esp_log_level_set("*",(esp_log_level_t)CONFIG_LOG_DEFAULT_LEVEL);

#if ESP_IDF_VERSION_MAJOR >= 5
      ESP_LOGI(TAG, "Task watchdog initialization deferred to app_main");
#else
#if !WDT_ALREADY_INITIALIZED
      ESP_LOGI(TAG, "Initialising WATCHDOG...");
      esp_task_wdt_init(120, true);
#else
      ESP_LOGI(TAG, "WATCHDOG already initialized...");
#endif // WDT_ALREADY_INITIALIZED
#endif
      }
  } fwi  __attribute__ ((init_priority (0150)));

Housekeeping* MyHousekeeping = NULL;
Peripherals* MyPeripherals = NULL;

void app_main(void)
  {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 120 * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
  if (wdt_err == ESP_ERR_INVALID_STATE)
    ESP_LOGI(TAG, "Task watchdog already initialized by ESP-IDF");
  else
    ESP_ERROR_CHECK(wdt_err);
#endif

  if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES)
    {
    nvs_flash_erase();
    nvs_flash_init();
    }

  ESP_LOGI(TAG, "Executing on CPU core %d",xPortGetCoreID());
  AddTaskToMap(xTaskGetCurrentTaskHandle());
  MyBoot.Init();

  ESP_LOGI(TAG, "Mounting CONFIG...");
  MyConfig.mount();

  ESP_LOGI(TAG, "Configure logging...");
  MyCommandApp.ConfigureLogging();

  ESP_LOGI(TAG, "Registering default configs...");
  MyConfig.RegisterParam("vehicle", "Vehicle", true, true);

  ESP_LOGI(TAG, "Starting HOUSEKEEPING...");
  MyHousekeeping = new Housekeeping();
  }
