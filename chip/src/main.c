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
#define KM 1000

double dynamic_distance;
uint64_t UTC;

Adafruit_NeoPixel *neopixel_matrix;
typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} sd_rgb;
sd_rgb target_rgb;
uint32_t breath_max_steps = 100;
uint32_t breath_interval = 1000;

double *neopixel_matrix_brightness_mask;
double threshold_approaching = 1 * KM;
double threshold_city = 10 * KM;
double threshold_country = 2000 * KM;
double threshold_hometown = 10000 * KM;
double threshold_infinity = 20000 * KM;
sd_rgb place_meet_rgb;
sd_rgb place_approaching_rgb;
sd_rgb place_city_rgb;
sd_rgb place_country_rgb;
sd_rgb place_hometown_rgb;
sd_rgb place_infinity_rgb;

struct mg_connection *UTC_conn = NULL;
char *UTC_url = "";
struct mg_connection *dynamic_distance_conn = NULL;
char *dynamic_distance_url = "";

static void sd_initialize()
{
  target_rgb =
      (sd_rgb){.r = 31, .g = 31, .b = 31};
  place_meet_rgb =
      (sd_rgb){.r = 127, .g = 63, .b = 0};
  place_approaching_rgb =
      (sd_rgb){.r = 0, .g = 15, .b = 31};
  place_city_rgb =
      (sd_rgb){.r = 7, .g = 31, .b = 7};
  place_country_rgb =
      (sd_rgb){.r = 0, .g = 0, .b = 31};
  place_hometown_rgb =
      (sd_rgb){.r = 15, .g = 31, .b = 0};
  place_infinity_rgb =
      (sd_rgb){.r = 31, .g = 31, .b = 31};
  const char *sd_url = get_cfg()->soul_distance.url;
  const char *sd_local = get_cfg()->soul_distance.local;
  const char *sd_remote = get_cfg()->soul_distance.remote;
  if (sd_url != NULL)
  {
    UTC_url = calloc(256, sizeof(char));
    sprintf(UTC_url, "%s/UTC", sd_url);
  }
  if (sd_url != NULL && sd_local != NULL && sd_remote != NULL)
  {
    dynamic_distance_url = calloc(256, sizeof(char));
    sprintf(dynamic_distance_url, "%s/Distance?Local=%s&Remote=%s", sd_url, sd_local, sd_remote);
  }
}

static void step_show(uint32_t step)
{
  sd_rgb step_rgb = (sd_rgb){.r = step * target_rgb.r / breath_max_steps,
                             .g = step * target_rgb.g / breath_max_steps,
                             .b = step * target_rgb.b / breath_max_steps};
  sd_rgb tmp_rgb;
  for (uint16_t neopixel_index = 0; neopixel_index < NEOPIXEL_NUM; neopixel_index++)
  {
    double neopixel_brightness = neopixel_matrix_brightness_mask[neopixel_index];
    tmp_rgb = (sd_rgb){.r = step_rgb.r * neopixel_brightness,
                       .g = step_rgb.g * neopixel_brightness,
                       .b = step_rgb.b * neopixel_brightness};
    Adafruit_NeoPixel__setPixelColor_n_r_g_b(neopixel_matrix, neopixel_index, tmp_rgb.r, tmp_rgb.g, tmp_rgb.b);
  }
  Adafruit_NeoPixel__show(neopixel_matrix);
}

static void runner_breath(void *arg)
{
  (void)arg;
  if (!updating_firmware)
  {
    for (uint32_t step = 0; step < breath_max_steps; step++)
    {
      step_show(step);
    }
    for (uint32_t step = breath_max_steps; step > 0; step--)
    {
      step_show(step);
    }
    step_show(0);
  }
  mgos_set_timer(breath_interval /* ms */, false /* repeat */, runner_breath, NULL);
}

static void render()
{
  sd_rgb from_rgb = (sd_rgb){.r = 127, .g = 127, .b = 127};
  sd_rgb to_rgb = (sd_rgb){.r = 127, .g = 127, .b = 127};
  double position = 0;
  breath_interval = 10000;
  breath_max_steps = 1000;
  if (dynamic_distance < threshold_approaching)
  {
    from_rgb = place_meet_rgb;
    to_rgb = place_approaching_rgb;
    position = 1.0 * (dynamic_distance - 0) / (threshold_approaching - 0);
    breath_interval = 1000 + position * (10000 - 1000);
    breath_max_steps = 100 + position * (1000 - 100);
  }
  else if (dynamic_distance < threshold_city)
  {
    from_rgb = place_approaching_rgb;
    to_rgb = place_city_rgb;
    position = 1.0 * (dynamic_distance - threshold_approaching) / (threshold_city - threshold_approaching);
  }
  else if (dynamic_distance < threshold_country)
  {
    from_rgb = place_city_rgb;
    to_rgb = place_country_rgb;
    position = 1.0 * (dynamic_distance - threshold_city) / (threshold_country - threshold_city);
  }
  else if (dynamic_distance < threshold_hometown)
  {
    from_rgb = place_country_rgb;
    to_rgb = place_hometown_rgb;
    position = 1.0 * (dynamic_distance - threshold_country) / (threshold_hometown - threshold_country);
  }
  else if (dynamic_distance < threshold_infinity)
  {
    from_rgb = place_hometown_rgb;
    to_rgb = place_infinity_rgb;
    position = 1.0 * (dynamic_distance - threshold_hometown) / (threshold_infinity - threshold_hometown);
  }
  else
  {
    from_rgb = place_infinity_rgb;
    to_rgb = place_infinity_rgb;
    position = 1.0;
  }
  target_rgb = (sd_rgb){
      .r = (to_rgb.r > from_rgb.r) ? from_rgb.r + position * (to_rgb.r - from_rgb.r) : from_rgb.r - position * (from_rgb.r - to_rgb.r),
      .g = (to_rgb.g > from_rgb.g) ? from_rgb.g + position * (to_rgb.g - from_rgb.g) : from_rgb.g - position * (from_rgb.g - to_rgb.g),
      .b = (to_rgb.b > from_rgb.b) ? from_rgb.b + position * (to_rgb.b - from_rgb.b) : from_rgb.b - position * (from_rgb.b - to_rgb.b)};
  CONSOLE_LOG(LL_INFO, ("Target RGB: %d %d %d\n", target_rgb.r, target_rgb.g, target_rgb.b));
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
    render();
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
    render();
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
  sd_initialize();
  updating_firmware = false;
  neopixel_matrix_brightness_mask = calloc(NEOPIXEL_NUM, sizeof(double));
  for (uint16_t i = 0; i < NEOPIXEL_NUM; i++)
  {
    neopixel_matrix_brightness_mask[i] = 1;
  }
  neopixel_matrix = Adafruit_NeoPixel____init___n_p_t(NEOPIXEL_NUM, NEOPIXEL_PIN, NEO_GRB);
  Adafruit_NeoPixel__begin(neopixel_matrix);
  dynamic_distance = 0;
  mgos_set_timer(0 /* ms */, false /* repeat */, runner_breath, NULL);
  mgos_set_timer(3600*1000 /* ms */, true /* repeat */, update_UTC, NULL);
  mgos_set_timer(60*1000 /* ms */, true /* repeat */, update_dynamic_distance, NULL);

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
