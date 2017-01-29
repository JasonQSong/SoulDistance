
let NeoPixel = {
  __init___n_p_t: ffi('void Adafruit_NeoPixel____init___n_p_t(Adafruit_NeoPixel *this,uint16_t n,uint8_t p,neoPixelType t)'),
  begin: ffi('void Adafruit_NeoPixel__begin(Adafruit_NeoPixel *this)'),
  NEO_RGB: ((0 << 6) | (0 << 4) | (1 << 2) | (2)),
  NEO_RBG: ((0 << 6) | (0 << 4) | (2 << 2) | (1)),
  NEO_GRB: ((1 << 6) | (1 << 4) | (0 << 2) | (2)),
  NEO_GBR: ((2 << 6) | (2 << 4) | (0 << 2) | (1)),
  NEO_BRG: ((1 << 6) | (1 << 4) | (2 << 2) | (0)),
  NEO_BGR: ((2 << 6) | (2 << 4) | (1 << 2) | (0)),
}