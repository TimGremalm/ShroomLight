#ifndef APA106_DRIVER_H
#define APA106_DRIVER_H

#include <stdint.h>

typedef union {
	struct __attribute__ ((packed)) {
		uint8_t r, g, b;
	};
	uint32_t num;
} rgbVal;

typedef union {
	struct __attribute__ ((packed)) {
		uint8_t h, s, v;
	};
	uint32_t num;
} hsvVal;

void apa106_init(int gpioNum);
void apa106_setColors(unsigned int length, rgbVal *array);
rgbVal apa106_HsvToRgb(hsvVal hsv);
hsvVal apa106_RgbToHsv(rgbVal rgb);

inline rgbVal makeRGBVal(uint8_t r, uint8_t g, uint8_t b) {
  rgbVal val;

  val.r = r;
  val.g = g;
  val.b = b;
  return val;
}

inline hsvVal makeHSVVal(uint8_t h, uint8_t s, uint8_t v) {
  hsvVal val;

  val.h = h;
  val.s = s;
  val.v = v;
  return val;
}

#endif /* APA106_DRIVER_H */

