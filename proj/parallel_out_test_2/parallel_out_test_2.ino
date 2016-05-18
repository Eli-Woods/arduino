#include<FastLED.h>

#define NUM_LED 90
#define NUM_STRIPS 8

CRGB leds[NUM_STRIPS * NUM_LED];

// READ ME!!! //
// These are the pins on the arduion we're using. Attach a 5ft strip to each of the pins, then just tell me
// what colors go to which pin number. Ezpz lemon squeezy.
// WS2811_PORTD: 25,26,27,28,14,15,29,11
//

void setup() {
  LEDS.addLeds<WS2811_PORTD, NUM_STRIPS>(leds, NUM_LED);
  LEDS.setBrightness(32);

  //LEDS.setMaxPowerInMilliWatts(24000);
}

void loop() {
  static uint8_t hue = 0;
  for (int i = 0; i < NUM_STRIPS; i++) {
    for (int j = 0; j < NUM_LED; j++) {
      leds[(i * NUM_LED) + j] = CHSV((32 * i) + hue + j, 192, 255);
    }
  }

  hue++;

  LEDS.show();
  LEDS.delay(10);
}
