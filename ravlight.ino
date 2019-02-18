/*
Adapted from https://learn.adafruit.com/trinket-sound-reactive-led-color-organ/code

Written by Adafruit Industries.  Distributed under the BSD license.
This paragraph must be included in any redistribution.
*/
#include <Adafruit_NeoPixel.h>

// These values should probably not be changed
#define N_PIXELS  16  // Number of pixels you are using
#define MIC_PIN   A1  // Microphone is attached to Gemma D2 (A1)
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE     10  // Noise/hum/interference in mic signal

// Change this value to set the operating mode for the LEDs
// Mode 1: Test the LED ring by making colors spin around
// Mode 2: Test the microphone input by using the LED ring as a crude volume meter
// Mode 3: Volume determines the brightness. A sharp change in volume will cycle the color.
// Mode 4: Change color from green to red based on the volume
// Mode 5: Cycle through colors. Cycle faster when volume is louder
#define MODE 5

// Special Mode: You can use this in combination with any of the others. When
// the volume exceeds a certain threshold, the lights will strobe. Set STROBE to
// 1 to enable.
#define STROBE 0
// Above this volume the lights will strobe
#define STROBE_VOLUME 150
// How fast to strobe the lights (larger is slower)
#define STROBE_SPEED 50

// You can change the following values to customize each of the modes

#if MODE == 1
// How fast do the colors move (larger is slower)
#define SPEED 30
// How much do the colors change from pixel-to-pixel
#define COLOR_DISTANCE 8

#elif MODE == 3
// If 1, the color change is random. If 0, the color change will progress along
// a color wheel.
#define RANDOMIZE_COLOR 1
// How fast to fade the brightness down when the volume goes down (larger is faster)
#define FADE_SPEED 200
// The minimum brightness level. Used to keep the lights on (but dim) when not played.
#define MIN_LEVEL 80
// How much the volume has to change in order to change the color
#define COLOR_CHANGE_THRESHOLD 20
// When *not* randomizing the color, this selects how much the color will change
// when the volume spikes
#define COLOR_CHANGE_AMOUNT 10
// Magic value I picked to make it look good. Affects the volume-to-brightness ratio
#define BRIGHT_SCALE 3.5

#elif MODE == 4
// How fast the color fades back to green when volume goes down (larger is faster)
#define COLOR_FADE_SPEED 100
// Scale the volume factor. Larger numbers go to red faster.
#define VOLUME_SCALE 1.5

#elif MODE == 5
// How fast to cycle through the colors (larger is slower)
#define SPEED 30

#endif

// Current "dampened" audio level
int lvl = 10;
uint32_t currentTime;

const uint8_t PROGMEM gamma8[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
 10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
 25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
 37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
 51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
 69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
 90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  currentTime = millis();
}

void loop() {
  uint8_t  i;
  uint32_t oldTime, deltaT;
  oldTime = currentTime;
  currentTime = millis();
  deltaT = currentTime - oldTime;
  int      n, height;
  n   = analogRead(MIC_PIN);                 // Raw reading from mic
  n   = abs(n - 512 - DC_OFFSET);            // Center on zero
  n   = (n <= NOISE) ? 0 : (n - NOISE);      // Remove noise/hum
  lvl = ((lvl * 7) + n) >> 3;    // "Dampened" reading (else looks twitchy)
  // lvl now ranges from 0 to ~300
  // Empirically, most playing fits into 0-150 range
  // But hard, loud hits can go to ~300

  strip.setBrightness(255);

#if MODE == 1
  testLEDs(deltaT);
#elif MODE == 2
  testMicrophone(deltaT);
#elif MODE == 3
  volumeCycle(deltaT);
#elif MODE == 4
  colorVolume(deltaT);
#elif MODE == 5
  cycleColors(deltaT);
#endif

#if STROBE != 0
  strobe(deltaT);
#endif

  strip.show();
}

#if MODE == 1
byte startPos = 0;
uint32_t accumTime = 0;
void testLEDs(uint32_t deltaT) {
  accumTime += deltaT;
  while (accumTime > SPEED) {
    accumTime -= SPEED;
    startPos++;
  }
  for(byte i=0; i<N_PIXELS; i++) {
     strip.setPixelColor(i, Wheel((startPos + i) * COLOR_DISTANCE));
  }
}
#endif

#if MODE == 2
int topPix = 0;
uint32_t lastTopTime = 0;
int lastTopLvl = 0;
uint8_t lvlPerPixel = 20;
void testMicrophone(uint32_t deltaT) {
  for(int i=0; i<N_PIXELS; i++) {
     strip.setPixelColor(i, 0, 255, 0);
  }

  if (lvl > lastTopLvl) {
    lastTopLvl = lvl;
    lastTopTime = currentTime;
  } else if (currentTime - lastTopTime > 3000) {
    lastTopLvl = lvl;
  }

  int topPix = min(N_PIXELS, lastTopLvl/lvlPerPixel);

  for(int i=0; i<topPix; i++) {
    strip.setPixelColor(i, 255, 0, 0);
  }
}
#endif


#if MODE == 3
uint8_t color = 0;
float bright = 0;
int maxLvlAvg = 300;
void volumeCycle(uint32_t deltaT) {

  float newBright = BRIGHT_SCALE * 255.0 * lvl / (maxLvlAvg - MIN_LEVEL) + MIN_LEVEL;
  if (newBright > 255) newBright = 255;
  float fadeBright = bright - FADE_SPEED * (deltaT / 1000.0);

  // Change the color if the volume spikes
  uint32_t pixelColor;
  if (newBright - bright > COLOR_CHANGE_THRESHOLD) {
    if (RANDOMIZE_COLOR) {
      pixelColor = strip.Color(random(0, 255), random(0, 255), random(0, 255));
    } else {
      color += COLOR_CHANGE_AMOUNT;
      pixelColor = Wheel(color);
    }
  }

  bright = newBright < fadeBright ? fadeBright : newBright;
  if (bright < 0) bright = 0;
  strip.setBrightness(pgm_read_byte(&gamma8[(uint8_t) bright]));

  for(int i=0; i<N_PIXELS; i++) {
     strip.setPixelColor(i, pixelColor);
  }
}
#endif

#if MODE == 4
uint8_t colorPos = 0;
uint8_t lastMaxColorPos = 0;
uint32_t startFade = 0;
void colorVolume(uint32_t deltaT) {
  int scaledLvl = VOLUME_SCALE * lvl;
  uint8_t newColorPos = scaledLvl > 255 ? 255 : scaledLvl;
  if (newColorPos > colorPos) {
    lastMaxColorPos = newColorPos;
    startFade = millis();
  }
  float fadeAmount = COLOR_FADE_SPEED * (millis() - startFade) / 1000.0;
  uint8_t fadeColorPos = fadeAmount < lastMaxColorPos ? lastMaxColorPos - fadeAmount : 0;

  colorPos = max(fadeColorPos, newColorPos);

  uint32_t pixelColor = gbrWheel(colorPos);

  strip.setBrightness(255);
  for(int i=0; i<N_PIXELS; i++) {
     strip.setPixelColor(i, pixelColor);
  }
}

// Fades from g - b - r
uint32_t gbrWheel(byte WheelPos) {
  if(WheelPos < 128) {
    return strip.Color(0, 255 - WheelPos * 2, WheelPos * 2);
  } else {
    WheelPos -= 128;
    return strip.Color(WheelPos * 2, 0, 255 - WheelPos * 2);
  }
}
#endif

#if MODE == 5
byte colorPos = 0;
uint32_t accumTime = 0;
// Ignore volume below this amount
int cutoff = 30;
// Assume that at 150 volume we want to change at max speed
float scaleFactor = SPEED / (150.0 - cutoff);
void cycleColors(uint32_t deltaT) {
  accumTime += deltaT;
  int volumeSpeed = lvl < cutoff ? 0 : scaleFactor * (lvl - cutoff);
  int threshold = SPEED - volumeSpeed;
  threshold = threshold < 1 ? 1 : threshold;
  while (accumTime > threshold) {
    accumTime -= threshold;
    colorPos++;
  }

  for(byte i=0; i<N_PIXELS; i++) {
     strip.setPixelColor(i, Wheel(colorPos));
  }
}
#endif

#if STROBE != 0
bool strobe_on = true;
uint32_t strobeTimer = 0;
void strobe(uint32_t deltaT) {
  if (lvl < STROBE_VOLUME) {
    return;
  }
  strobeTimer += deltaT;
  while (strobeTimer > STROBE_SPEED) {
    strobeTimer -= STROBE_SPEED;
    strobe_on = !strobe_on;
  }
  if (!strobe_on) {
    strip.setBrightness(0);
  }
}
#endif

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
