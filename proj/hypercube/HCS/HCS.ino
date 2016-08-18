// LED Includes
#define USE_OCTOWS2811
#include <OctoWS2811.h>
#include <FastLED.h>

// Arduino Includes
#ifdef __AVR__
#include <avr/interrupt.h>
#endif

// BNO055 Includes
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// Digital Pins for Interrupts (may be subject to change)
#define LASER0 0
#define LASER1 1
#define LASER2 17
#define LASER3 22
#define PIEZO0 18
#define PIEZO1 19
#define DONOTPRS 23
// NEED TO DECIDE ON A PIN TO USE FOR LASER BREAK RESET SWITCH

#define UNUSED 13

// For describing the shells easier in code
#define INNER 0
#define OUTER 1

// Object for our accelerometer and other variables
Adafruit_BNO055 bno = Adafruit_BNO055(55);
boolean bno_running = false;
double acc_thresh = 10; // TOTALLY ARBITRARY VALUE RN
uint32_t curr_second, last_second;
uint8_t cumm_time;
const uint8_t bno_off_time = 5; // Turn off after 10 seconds of being idle


// Constants describing numbers of LEDs and strips
const uint16_t strip_len = 90; // Number of LED per strip
const uint8_t num_per_group = 6; // Number of strips per OctoWS2811 group

const uint8_t in_strips = 3 * num_per_group; // Number of strips on inner shell
const uint8_t out_strips = 5 * num_per_group; // Number of strips on outer shell

const uint16_t in_LED_tot = in_strips * strip_len;
const uint16_t out_LED_tot = out_strips * strip_len;
const uint16_t led_tot = in_LED_tot + out_LED_tot;

// Some templates that we can copy paste to the main LED array
CRGBArray<strip_len> led_tmplt;
CRGBArray<in_LED_tot> in_leds;
CRGBArray<out_LED_tot> out_leds;

// The array that ultimately gets pushed to the Octo Controller
CRGBArray<led_tot> leds;

// For controlling the brightness, again might not need this variability.
const uint8_t maxBrightness = 50;
uint8_t gBrightness = maxBrightness; // CHANGE THIS ONCE YOU HAVE ANOTHER POTENTIOMETER

// Setup and global variable delcaration for palettes
CRGBPalette16 gPalette, nPalette; // These point towards the background palette that we fill the templates with
CRGBPalette16 oPalette, iPalette; // Lets try using two separate palettes for the shells
CRGBPalette16 oTargetPalette, iTargetPalette; // These are for when we switch between color palettes
boolean iPaletteSwitch, oPaletteSwitch; // So that we update the right palette
uint8_t iPaletteSwitchCount, oPaletteSwitchCount;
TBlendType gBlending;
uint8_t iPaletteCounter, oPaletteCounter, gPaletteCounter; // Global Palette
uint8_t gIndex; // Global Palette Index
extern const uint8_t numPalettes, numPalStyles;

// To control hue globally through accelerometer input
uint8_t gHue;

// For animation switching, this number needs to be hard coded unforunately
const uint8_t iNumAnimation = 7; 
const uint8_t oNumAnimation = 7; 
uint8_t iAnimCounter, oAnimCounter;
boolean iAnimSwitch, oAnimSwitch; // Use this flag so that we fade the color palette into each animation
uint8_t iAnimSwitchCount, oAnimSwitchCount; // Use this to count how many times we've faded to the 

// These are the flags that will get flipped during the ISR
volatile boolean do_not_pressed = false;
volatile boolean laser0_on = false;
volatile boolean laser1_on = false;
volatile boolean laser2_on = false;
volatile boolean laser3_on = false;
volatile uint8_t piezo0_count = 0;
volatile uint8_t piezo1_count = 0;
volatile uint32_t last_millis;
const uint32_t debounce_time = 30; // In seconds

void setup() {
  // Initialize the leds, specifially to use OctoWS2811 controller
  LEDS.addLeds<OCTOWS2811>(leds, strip_len * num_per_group); // No need to declare pin numbers since they are preset with parallel output
  LEDS.setBrightness(maxBrightness); // LETS SEE HOW FAR WE CAN PUSH IT
  LEDS.show();

  // Initialize global color variables
  gBrightness = maxBrightness;
  gBlending = NOBLEND;
  iPaletteCounter = 0;
  oPaletteCounter = iPaletteCounter+3;
  iPaletteSwitch = false;
  oPaletteSwitch = false;
  iPaletteSwitchCount = 0;
  oPaletteSwitchCount = 0;

  iAnimCounter = 0;
  oAnimCounter = 0;
  iAnimSwitch = false;
  oAnimSwitch = false;
  iAnimSwitchCount = 0;
  oAnimSwitchCount = 0;
  gIndex = 0;
  gHue = 0;

  // Random number generation for the noise overlap
  random16_set_seed(analogRead(UNUSED));

  // Setup for the interrupts, names hould be pretty self explanatory
  pinMode(LASER0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LASER0), laser0_ISR, RISING);

  pinMode(LASER1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LASER1), laser1_ISR, RISING);

  pinMode(LASER2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LASER2), laser2_ISR, RISING);

  pinMode(LASER3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LASER3), laser3_ISR, RISING);

  pinMode(PIEZO0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIEZO0), piezo0_ISR, RISING);

  pinMode(PIEZO1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIEZO1), piezo1_ISR, RISING);

  pinMode(DONOTPRS, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DONOTPRS), debounce_do_not_press, RISING);

  // Initialize the gyroscope. If it's not on, turn the whole display red
  // as an easy indication
  if (!bno.begin()) {
    fill_solid(leds, led_tot, CHSV(0, 255, gBrightness));
    LEDS.show();
    while(1);
  }

  bno.setExtCrystalUse(true);

}

void loop() {
  // Read in events from the gyroscope/accelerometer
  static imu::Vector<3> accel;
  static sensors_event_t event;
  EVERY_N_MILLISECONDS(20) {
    bno.getEvent(&event);
    accel = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  }

  // Check if the BNO has stopped moving below some threshold
  if(accel.x() < acc_thresh && accel.y() < acc_thresh) {
    // Only do these calculations if the accelerometer is currently being read from
    if(bno_running) {
      curr_second = millis()*1000;
      // Add the difference to the cummulative time if we're not on the same second
      if(curr_second > last_second) {
        cumm_time += curr_second - last_second;
      }
      if(cumm_time >= bno_off_time) {
        bno_running = false;
      }
    }
  }
  else if(accel.x() > acc_thresh || accel.y() > acc_thresh) {
    bno_running = true;
    cumm_time = 0;
    
    // Convert the heading to the color index
    gHue = map(event.orientation.x, 0, 359, 0, 255);

    // Convert the pitch to that rate at which the animation index is changed
    EVERY_N_MILLISECONDS_I(thisTimer, 20) {
      thisTimer.setPeriod(map(event.orientation.y, 0, 359, 5, 150));
      gIndex++;
    }
  }
  // If the the bno is sitting still, run the animations and hue change at a constant rate
  if(!bno_running) {
    EVERY_N_MILLISECONDS(5) {
      gIndex++;
    }
    EVERY_N_MILLISECONDS(20) {
      gHue++;
    }
  }

  // Switch the animation according to some timer
  EVERY_N_SECONDS(30) {
    iAnimCounter = (iAnimCounter + 1) % iNumAnimation;
    iAnimSwitch = true;
  }
  EVERY_N_SECONDS(10) {
    oAnimCounter = (oAnimCounter + 1) % oNumAnimation;
    oAnimSwitch = true;
  }


  // Update the global color palette. This is just the color scheme
  // that we will then repackage to have different spacial distributions
  EVERY_N_SECONDS(10100) {
    gPaletteCounter = (gPaletteCounter+1)%numPalettes;
  }
  updatePaletteScheme();

  // Update the palette style for the inner and outer shells. This
  // is basically how we package the given color palette
  EVERY_N_SECONDS(45) {
    oPaletteCounter = (oPaletteCounter +1) % numPalStyles;
    oPaletteSwitch = true;
  }
  EVERY_N_SECONDS(135) {
    iPaletteCounter = (iPaletteCounter + 1) % numPalStyles;
    iPaletteSwitch = true;
  }
  updateGPalette();

  // Lets try some different combinations of things by switching between basic animations
  // on the two shells separately
  switch (iAnimCounter) {
    case 0:
      chase_straight(INNER, false);
      break;
    case 1:
      chase_spiral(INNER, 16, false);
      break;
    case 2:
      static uint8_t iOffset = 0;
      EVERY_N_MILLISECONDS(250) {
        iOffset++;
      }
      chase_spiral(INNER, iOffset, false);
      break;
    case 3:
      shell_wrap(INNER, 0, false);
      break;
    case 4:
      shell_wrap(INNER, 1, false);
      break;
    case 5:
      shell_wrap(INNER, 2, false);
      break;
    case 6:
      shell_wrap(INNER, 3, false);
      break;
    case 7:
      chase_helix(INNER, 16, false);
      break;
  }
  switch (oAnimCounter) {
    case 0:
      chase_straight(OUTER, true);
      break;
    case 1:
      chase_spiral(OUTER, 4, true);
      break;
    case 2:
      static uint8_t oOffset = 0;
      EVERY_N_MILLISECONDS(100) {
        oOffset++;
      }
      chase_spiral(OUTER, oOffset, true);
      break;
    case 3:
      shell_wrap(OUTER, 0, true);
      break;
    case 4:
      shell_wrap(OUTER, 1, true);
      break;
    case 5:
      shell_wrap(OUTER, 2, true);
      break;
    case 6:
      shell_wrap(OUTER, 3, true);
      break;
    case 7:
      chase_helix(OUTER, 4, true);
      break;
  }

  // Merge each shell to the whole LED array and push to the lights
  merge_animations();

  // Now turn on the overlay animations if they're meant to be
  if (laser0_on) {
    ring_bounce_opp(20, 5);
  }
  if (laser1_on) {
    helix_spiral_overlay(20, 4);
  }
  if (laser2_on) {
    bar_wrap_overlay(50, 2, false);
  }
  if (laser3_on) {
    overlay_snow(20, 0.33);
  }
  if (laser0_on || laser1_on || laser2_on || laser3_on) {
    LEDS.show();
  }


}
