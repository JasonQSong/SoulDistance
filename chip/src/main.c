#include <stdio.h>

#include "common/platform.h"
#include "common/cs_file.h"
#include "fw/src/mgos_app.h"
#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_sys_config.h"
#include "fw/src/mgos_timers.h"
#include "fw/src/mgos_hal.h"
#include "fw/src/mgos_dlsym.h"
#include "mjs.h"

#include "Adafruit_NeoPixel.h"

#if CS_PLATFORM == CS_P_ESP8266
#define LED_GPIO 2 /* On ESP-12E there is a blue LED connected to GPIO2  */
#elif CS_PLATFORM == CS_P_ESP32
#define LED_GPIO 21 /* No LED on DevKitC, use random GPIO close to GND pin */
#elif CS_PLATFORM == CS_P_CC3200
#define LED_GPIO 64 /* The red LED on LAUNCHXL */
#elif CS_PLATFORM == CS_P_STM32 && defined(STM32F746xx)
/* Nucleo-144 F746 - pin B7 (blue LED) */
#define LED_GPIO 0x04000080
#else
#error Unknown platform
#endif

int get_led_gpio_pin(void) {
  return LED_GPIO;
}

enum mgos_app_init_result mgos_app_init(void) {
  /* Initialize JavaScript engine */
  struct mjs *mjs = mjs_create();
  mjs_set_ffi_resolver(mjs, mgos_dlsym);
  mjs_err_t err = mjs_exec_file(mjs, "init.js", NULL);
  if (err != MJS_OK) {
    LOG(LL_ERROR, ("MJS exec error: %s\n", mjs_strerror(mjs, err)));
  }

  return MGOS_APP_INIT_SUCCESS;
}
