
let NeoPixel = {
  Adafruit_NeoPixel____init___n_p_t: ffi('Adafruit_NeoPixel * Adafruit_NeoPixel____init___n_p_t(uint16_t,uint8_t,uint8_t)'),
  Adafruit_NeoPixel__begin: ffi('Adafruit_NeoPixel * Adafruit_NeoPixel__begin(Adafruit_NeoPixel *)'),
  Adafruit_NeoPixel__setPixelColor_n_r_g_b: ffi('void Adafruit_NeoPixel__setPixelColor_n_r_g_b(Adafruit_NeoPixel *, uint16_t, uint8_t, uint8_t, uint8_t'),
  Adafruit_NeoPixel__show: ffi('void Adafruit_NeoPixel__show(Adafruit_NeoPixel *)'),
  NEO_RGB: ((0 << 6) | (0 << 4) | (1 << 2) | (2)),
  NEO_RBG: ((0 << 6) | (0 << 4) | (2 << 2) | (1)),
  NEO_GRB: ((1 << 6) | (1 << 4) | (0 << 2) | (2)),
  NEO_GBR: ((2 << 6) | (2 << 4) | (0 << 2) | (1)),
  NEO_BRG: ((1 << 6) | (1 << 4) | (2 << 2) | (0)),
  NEO_BGR: ((2 << 6) | (2 << 4) | (1 << 2) | (0)),
};