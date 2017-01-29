// Load Mongoose OS API
load('api_timer.js');
load('api_gpio.js');

load('api_neopixel.js');

// Blink built-in LED every second
let PIN = ffi('int get_led_gpio_pin()')();  // Helper C function that returns a
                                            // built-in LED GPIO

let neo_pixel_num=50;
let neo_pixel_pin=15;

let device_local=1;
let device_remote=2;

GPIO.set_mode(PIN, GPIO.MODE_OUTPUT);

let neo_pixel_matrix=new Adafruit_NeoPixel();
NeoPixel.__init___n_p_t(neo_pixel_matrix,neo_pixel_num,neo_pixel_pin,NeoPixel.NEO_GRB);
NeoPixel.begin(neo_pixel_matrix);

Timer.set(60* 1000,1,function(){

})

let main_loop=function(){
  let value = GPIO.toggle(PIN);
  print(value ? 'Tick' : 'Tock');
  Timer.set(500 /* milliseconds */, 0 /* repeat */, main_loop, null);
}



