// LED library
#include <FastLED.h>


// Arduino Libraries
#ifdef __AVR__
#include <avr/interrupt.h>
#endif

// Analog Pins
#define RATE_POT 0 // Potentiometer for animation rate
#define HUE_POT 1 // Potentiometer for global hue

// Digital Pins
#define LED_IN 7

// Variables for the LED strand
const uint8_t gBrightness = 200;
const uint16_t numLED = 143;
CRGBArray<numLED> leds;

// Setup and global variable delcaration for palettes
CRGBPalette16 gPalette;
TBlendType gBlending = LINEARBLEND;
uint8_t gPaletteCounter = 0;
extern const uint8_t numPalettes;

// To control hue globally through a potentiometer input
uint8_t gHue = 0;
uint8_t gSpeed = 0;
uint8_t gIndex = 0;

// For animation switching, this number needs to be hard coded unforunately
const uint8_t numAnimation = 14;
uint8_t gAnimCounter = 0;

void setup() {
  delay(3000); // Safely power up

  // Setup Random Number Generator
  random16_set_seed(4832);
  random16_add_entropy(analogRead(5));

  // Setup strands of LED
  FastLED.addLeds<WS2812, LED_IN, GRB>(leds, numLED).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(gBrightness);
  FastLED.show();
}

void loop() {
  // Read color from potentiometer input
  gHue = map(analogRead(HUE_POT), 0, 1253, 0, 255);

  // Stop switching palettes if we are stopped on the animation,
  // but you can still play with the hue on the currently selected
  if (gAnimCounter > 2) {
    EVERY_N_MINUTES(3) {
      gPaletteCounter = (gPaletteCounter + 1) % numPalettes;
    }
  }
  updateGPalette();


  EVERY_N_MINUTES(5) {
    gAnimCounter = (gAnimCounter + 1) % numAnimation;
  }


  EVERY_N_MILLISECONDS_I(thisTimer, 75) {
    thisTimer.setPeriod(map(analogRead(RATE_POT), 0, 1253, 1, 100));
    gIndex++;
  }

  // Select animation to run based on global counter
  switch ( gAnimCounter ) {
    case 0:
      fill_all();
      break;
    case 1:
      fill_all_grad();
      break;
    case 2:
      fill_all_grad_2();
      break;
    case 3:
      theater_chase();
      break;
    case 4:
      theater_chase_mir();
      break;
    case 5:
      theater_chase_mir2();
      break;
    case 6:
      theater_tri8();
      break;
    case 7:
      theater_tri();
      break;
    case 8:
      theater_tri8_mir();
      break;
    case 9:
      theater_tri_mir();
      break;
    case 10:
      theater_tri8_mir2();
      break;
    case 11:
      theater_tri_mir2();
      break;
    case 12:
      theater_mod();
      break;
    case 13:
      theater_mod_mir();
      break;
    case 14:
      theater_mod_mir2();
      break;
  }

}
