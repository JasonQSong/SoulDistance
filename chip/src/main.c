#include <stdio.h>
#include <string.h>

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
// #include "mjs.h"

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

double dynamic_distance;
uint64_t UTC;

Adafruit_NeoPixel *neopixel_matrix;
double *neopixel_matrix_brightness_mask;

uint8_t target_red = 127;
uint8_t target_green = 0;
uint8_t target_blue = 0;
uint32_t max_steps = 60;
uint32_t breath_interval = 1000;

static void step_show(uint32_t step, uint8_t target_red, uint8_t target_green, uint8_t target_blue)
{
  uint8_t step_red = step * target_red / max_steps;
  uint8_t step_green = step * target_green / max_steps;
  uint8_t step_blue = step * target_blue / max_steps;
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
  if (!updating_firmware)
  {
    for (uint32_t step = 0; step < max_steps; step++)
    {
      step_show(step, target_red, target_green, target_blue);
    }
    for (uint32_t step = max_steps; step > 0; step--)
    {
      step_show(step, target_red, target_green, target_blue);
    }
    step_show(0, target_red, target_green, target_blue);
  }
  mgos_set_timer(breath_interval /* ms */, false /* repeat */, runner_breath, NULL);
}

static struct mg_str get_json_resonse_from_hm_message(struct mg_str message)
{
  CONSOLE_LOG(LL_INFO, ("Parsing: [%.*s]\n", (int)message.len, message.p));
  struct mg_str response;
  response.p = NULL;
  response.len = 0;
  size_t start = 0;
  for (size_t i = 0; i < message.len; i++)
  {
    if (message.p[i] == '{')
    {
      response.p = message.p + i;
      start = i;
    }
    if (message.p[i] == '}')
    {
      response.len = i - start + 1;
    }
  }
  if (response.p == NULL || response.len == 0)
  {
    CONSOLE_LOG(LL_INFO, ("No json response found.\n"));
  }
  return mg_strdup(response);
}

struct mg_connection *UTC_conn = NULL;
const char *UTC_url = "https://4jvqd73602.execute-api.us-west-1.amazonaws.com/SoulDistance/UTC";
static void update_UTC_handler(struct mg_connection *nc, int ev, void *ev_data)
{
  switch (ev)
  {
  case MG_EV_HTTP_REPLY:
    CONSOLE_LOG(LL_INFO, ("UTC_hanler:HTTP_REPLY\n"));
    struct http_message *hm = (struct http_message *)ev_data;
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    struct mg_str response = get_json_resonse_from_hm_message(hm->message);
    if (response.p == NULL || response.len == 0)
    {
      CONSOLE_LOG(LL_INFO, ("No json response\n"));
      return;
    }
    CONSOLE_LOG(LL_INFO, ("Response: [%.*s]\n", (int)response.len, response.p));
    int count = json_scanf(response.p, response.len, "{UTC: %lld}", &UTC);
    CONSOLE_LOG(LL_INFO, ("count: %d UTC:%lld", count, UTC));
    return;
  case MG_EV_CLOSE:
    UTC_conn = NULL;
    return;
  }
}
static void update_UTC(void *arg)
{
  (void)arg;
  if (UTC_conn != NULL)
    return;
  if (updating_firmware)
    return;
  CONSOLE_LOG(LL_INFO, ("updater_UTC"));
  struct mg_connect_opts opts;
  memset(&opts, 0, sizeof(opts));
  opts.ssl_ca_cert = "VeriSignG5.pem";
  UTC_conn = mg_connect_http_opt(
      mgos_get_mgr(), update_UTC_handler, opts, UTC_url, NULL, NULL);
}

struct mg_connection *dynamic_distance_conn = NULL;
const char *dynamic_distance_url = "https://4jvqd73602.execute-api.us-west-1.amazonaws.com/SoulDistance/Distance?Local=1&Remote=2";
static void update_dynamic_distance_handler(struct mg_connection *nc, int ev, void *ev_data)
{
  switch (ev)
  {
  case MG_EV_HTTP_REPLY:
    CONSOLE_LOG(LL_INFO, ("HTTP_REPLY"));
    struct http_message *hm = (struct http_message *)ev_data;
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    struct mg_str response = get_json_resonse_from_hm_message(hm->message);
    if (response.p == NULL || response.len == 0)
    {
      CONSOLE_LOG(LL_INFO, ("No json response\n"));
      return;
    }
    CONSOLE_LOG(LL_INFO, ("Response: [%.*s]\n", (int)response.len, response.p));
    uint64_t LocalUpdateUTC, RemoteUpdateUTC;
    int count = json_scanf(
        response.p, response.len, "{LocalUpdateUTC: %lld, RemoteUpdateUTC: %lld, Distance: %llf}",
        &LocalUpdateUTC, &RemoteUpdateUTC, &dynamic_distance);
    CONSOLE_LOG(LL_INFO, ("count: %d LocalUpdateUTC:%lld RemoteUpdateUTC: %lld, Distance: %llf",
                          count, LocalUpdateUTC, RemoteUpdateUTC, dynamic_distance));
    return;
  case MG_EV_CLOSE:
    dynamic_distance_conn = NULL;
    return;
  }
}
static void update_dynamic_distance(void *arg)
{
  (void)arg;
  if (dynamic_distance_conn != NULL)
    return;
  if (updating_firmware)
    return;
  CONSOLE_LOG(LL_INFO, ("updater_distance"));
  struct mg_connect_opts opts;
  memset(&opts, 0, sizeof(opts));
  opts.ssl_ca_cert = "VeriSignG5.pem";
  dynamic_distance_conn = mg_connect_http_opt(
      mgos_get_mgr(), update_dynamic_distance_handler, opts, dynamic_distance_url, NULL, NULL);
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
  mgos_set_timer(2000 /* ms */, true /* repeat */, update_UTC, NULL);
  mgos_set_timer(10000 /* ms */, true /* repeat */, update_dynamic_distance, NULL);
  update_dynamic_distance(NULL);

  /* Initialize JavaScript engine */
  // struct mjs *mjs = mjs_create();
  // mjs_set_ffi_resolver(mjs, mgos_dlsym);
  // mjs_err_t err = mjs_exec_file(mjs, "init.js", NULL);
  // if (err != MJS_OK)
  // {
  //   LOG(LL_ERROR, ("MJS exec error: %s\n", mjs_strerror(mjs, err)));
  // }
  mgos_set_timer(get_cfg()->update.interval * 1000000 /* ms */, true /* repeat */, update_firmware, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
