#include <stdio.h>

#include "common/platform.h"
#include "common/cs_file.h"
#include "fw/src/mgos_app.h"
#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_sys_config.h"
#include "fw/src/mgos_timers.h"
#include "fw/src/mgos_hal.h"
#include "fw/src/mgos_dlsym.h"
#include "fw/src/mgos_console.h"
#include "fw/src/mgos_updater_http.h"
#include "fw/src/mgos_mongoose.h"
#include "mjs.h"

#include "platforms/esp8266/esp_neopixel.h"

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

bool updating_firmware;
static struct update_context *s_ctx;

int get_led_gpio_pin(void)
{
  return LED_GPIO;
}

#define NEOPIXEL_NUM 40
#define NEOPIXEL_PIN 15
#define MAX_STEPS 300

double dynamic_distance;

Adafruit_NeoPixel *neopixel_matrix;
double *neopixel_matrix_brightness_mask;

uint8_t target_red = 127;
uint8_t target_green = 0;
uint8_t target_blue = 0;

static void step_show(uint32_t step, uint8_t target_red, uint8_t target_green, uint8_t target_blue)
{
  uint8_t step_red = step * target_red / MAX_STEPS;
  uint8_t step_green = step * target_green / MAX_STEPS;
  uint8_t step_blue = step * target_blue / MAX_STEPS;
  uint8_t tmp_red, tmp_green, tmp_blue;
  for (uint16_t neopixel_index = 0; neopixel_index < NEOPIXEL_NUM; neopixel_index++)
  {
    double neopixel_brightness = neopixel_matrix_brightness_mask[neopixel_index];
    tmp_red = step_red * neopixel_brightness;
    tmp_green = step_green * neopixel_brightness;
    tmp_blue = step_blue * neopixel_brightness;
    Adafruit_NeoPixel__setPixelColor_n_r_g_b(neopixel_matrix, neopixel_index, tmp_red, tmp_green, tmp_blue);
  }
  Adafruit_NeoPixel__show(neopixel_matrix);
}

static void runner_breath(void *arg)
{
  (void)arg;
  if (updating_firmware)
  {
    mgos_set_timer(1000 /* ms */, false /* repeat */, runner_breath, NULL);
    return;
  }
  for (uint32_t step = 0; step < 100; step++)
  {
    step_show(step, target_red, target_green, target_blue);
  }
  for (uint32_t step = 100; step > 0; step--)
  {
    step_show(step, target_red, target_green, target_blue);
  }
  step_show(0, target_red, target_green, target_blue);
  mgos_set_timer(1000 /* ms */, false /* repeat */, runner_breath, NULL);
}

static void dynamic_distance_handler(struct mg_connection *nc, int ev, void *ev_data)
{
  CONSOLE_LOG(LL_INFO, ("dynamic_distance_handler"));
  if (ev == MG_EV_HTTP_REPLY)
  {
    CONSOLE_LOG(LL_INFO, ("HTTP_REPLY"));
    struct http_message *hm = (struct http_message *)ev_data;
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    CONSOLE_LOG(LL_INFO, ("%s\n", hm->message.p));
  }
}

static void updater_distance(void *arg)
{
  (void)arg;
  if (updating_firmware)
  {
    return;
  }
  const char *dynamic_distance_url = "https://4jvqd73602.execute-api.us-west-1.amazonaws.com/SoulDistance/Distance?Local=1&Remote=2";
  // const char *dynamic_distance_url = "https://www.google.com/";
  CONSOLE_LOG(LL_INFO, ("updater_distance"));
  mg_connect_http(mgos_get_mgr(),dynamic_distance_handler,dynamic_distance_url, NULL,NULL);
}

static void update_firmware_result_cb(struct update_context *ctx)
{
  if (ctx != s_ctx)
    return;
  updating_firmware = false;
}

static void update_firmware(void *arg)
{
  (void)arg;
  updating_firmware = true;
  s_ctx = updater_context_create();
  if (s_ctx == NULL)
  {
    mgos_set_timer(1000 /* ms */, false /* repeat */, update_firmware, NULL);
  }
  else
  {
    s_ctx->ignore_same_version = true;
    s_ctx->fctx.commit_timeout = get_cfg()->update.timeout;
    s_ctx->result_cb = update_firmware_result_cb;
    mgos_updater_http_start(s_ctx, get_cfg()->update.url);
  }
}

enum mgos_app_init_result mgos_app_init(void)
{
  mgos_upd_commit();
  updating_firmware = false;
  neopixel_matrix_brightness_mask = calloc(NEOPIXEL_NUM, sizeof(double));
  for (uint16_t i = 0; i < NEOPIXEL_NUM; i++)
  {
    neopixel_matrix_brightness_mask[i] = 1;
  }
  neopixel_matrix_brightness_mask[0] = 0.25;
  neopixel_matrix_brightness_mask[1] = 0.5;
  neopixel_matrix_brightness_mask[2] = 0.75;
  neopixel_matrix = Adafruit_NeoPixel____init___n_p_t(NEOPIXEL_NUM, NEOPIXEL_PIN, NEO_GRB);
  Adafruit_NeoPixel__begin(neopixel_matrix);
  dynamic_distance = 0;
  mgos_set_timer(0 /* ms */, false /* repeat */, runner_breath, NULL);
  mgos_set_timer(5000 /* ms */, true /* repeat */, updater_distance, NULL);

  /* Initialize JavaScript engine */
  // struct mjs *mjs = mjs_create();
  // mjs_set_ffi_resolver(mjs, mgos_dlsym);
  // mjs_err_t err = mjs_exec_file(mjs, "init.js", NULL);
  // if (err != MJS_OK)
  // {
  //   LOG(LL_ERROR, ("MJS exec error: %s\n", mjs_strerror(mjs, err)));
  // }
  mgos_set_timer(get_cfg()->update.interval * 1000 /* ms */, true /* repeat */, update_firmware, NULL);
  return MGOS_APP_INIT_SUCCESS;
}












