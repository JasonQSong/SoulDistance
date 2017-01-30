// Load Mongoose OS API
load('api_timer.js');
load('api_gpio.js');
load('api_neopixel.js');

// Blink built-in LED every second
let PIN = ffi('int get_led_gpio_pin()')();  // Helper C function that returns a
// built-in LED GPIO

let neo_pixel_num = 50;
let neo_pixel_pin = 15;

let device_local = 1;
let device_remote = 2;

GPIO.set_mode(PIN, GPIO.MODE_OUTPUT);

let neopixel_matrix = NeoPixel.Adafruit_NeoPixel____init___n_p_t(neo_pixel_num, neo_pixel_pin, NeoPixel.NEO_GRB);
NeoPixel.Adafruit_NeoPixel__begin(neopixel_matrix);

Timer.set(60 * 1000, 1, function () {
  //Read config and distance from server
});

let main_loop = function () {
  let value = GPIO.toggle(PIN);
  print(value ? 'Tick' : 'Tock');
  for (c = 0; c <= 127; c++) {
    for (i = 0; i < neo_pixel_num; i++) {
      NeoPixel.Adafruit_NeoPixel__setPixelColor_n_r_g_b(neopixel_matrix, i, c, 0, 0);
    }
    NeoPixel.Adafruit_NeoPixel__show(neopixel_matrix);
  }
  Timer.set(500 /* milliseconds */, 0 /* repeat */, main_loop, null);
}


