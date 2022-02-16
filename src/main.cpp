#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include <TinyPICO.h>
#include <Wire.h>
#include <grafic.h>
#include <SPI.h>

TinyPICO tp = TinyPICO();

#define NUMPIXELS 144 // Number of LEDs in strip

// Here's how to control the LEDs from any two pins:
#define DATAPIN 21
#define CLOCKPIN 22

#define NUM_LEDS 144

#define IR_INPUT_PIN 15
#define NO_LED_FEEDBACK_CODE
#include "TinyIRReceiver.hpp"

#define BTN_BRIGHT_UP 0x0009
#define BTN_BRIGHT_DOWN 0x0015
#define BTN_RESTART 0x0007
#define BTN_BATTERY 0x0047
#define BTN_FASTER 0x000D
#define BTN_SLOWER 0x0019
#define BTN_OFF 0x0045
#define BTN_PATTERN_PREV 0x0040
#define BTN_PATTERN_NEXT 0x0043
#define BTN_NONE 0x9999
#define BTN_AUTOPLAY 0x0044

// Empty and full thresholds (millivolts) used for battery level display:
#define BATT_MIN_MV 3150 // Some headroom over battery cutoff near 2.9V
#define BATT_MAX_MV 4000 // And little below fresh-charged battery near 4.1V

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

void showBatteryLevel(void);
void showColumn(void);

volatile uint16_t irCode = BTN_NONE;
uint32_t lastImageTime = 0L; // Time of last image change
const uint8_t brightness[] = {15, 31, 63, 127, 255};
int bLevel = 20;

const uint16_t lineTable[] = {
    // 375 * 2^(n/3)
    1000000L / 375, // 375 lines/sec = slowest
    1000000L / 472,
    1000000L / 595,
    1000000L / 750, // 750 lines/sec = mid
    1000000L / 945,
    1000000L / 1191,
    1000000L / 1500 // 1500 lines/sec = fastest
};

uint16_t lineInterval = 1000000L / 250;
uint32_t lastLineTime = 0L;

uint8_t pixel = 0;
uint8_t frame = 0;
uint8_t column = 0;
uint8_t line = 0;

uint8_t frames = 8;
uint8_t rows = 72;
uint8_t columns = 114;

void setup()
{
  Serial.begin(115200);
  strip.begin(); // Initialize pins for output
  strip.clear(); // Make sure strip is clear
  strip.setBrightness(30);
  strip.show();  // Turn all LEDs off ASAP

  Serial.println(F("START " __FILE__ " from " __DATE__));
  initPCIInterruptForTinyReceiver();
  Serial.println(F("Ready to receive NEC IR signals at pin " STR(IR_INPUT_PIN)));

  showBatteryLevel();
}

int counter = 0;

void loop()
{
  uint32_t t = millis();

  showColumn();

  while (((t = micros()) - lastLineTime) < lineInterval)
  {
    switch (irCode)
    {
    case BTN_BRIGHT_UP:
      if (bLevel < (sizeof(brightness) - 1))
        strip.setBrightness(brightness[++bLevel]);
      Serial.println("brightness up");
      break;
    case BTN_BRIGHT_DOWN:
      if (bLevel)
        strip.setBrightness(brightness[--bLevel]);
      Serial.println("brightness down");
      break;
    }
    irCode = BTN_NONE;
  }

  if (column < columns)
    column++;
  else
  {
    if (frame < frames)
    {
      column = 0;
      frame++;
    }
    else
    {
      column = 0;
      frame = 0;
    }
  }

  strip.show(); // Refresh LEDs
  lastLineTime = t;
}

void showColumn()
{

  for (int i = 0; i < rows; i++)
  {
    strip.setPixelColor(i, animation[0][column][i][0], animation[0][column][i][1], animation[0][column][i][2]);
    strip.setPixelColor(i + 72, animation[0][column][i][0], animation[0][column][i][1], animation[0][column][i][2]);
  }
}

void handleReceivedTinyIRData(uint16_t aAddress, uint8_t aCommand, bool isRepeat)
{
  /*
  Serial.print(F("A=0x"));
  Serial.print(aAddress, HEX);
  Serial.print(F(" C=0x"));
  Serial.print(aCommand, HEX);
  Serial.print(F(" R="));
  Serial.print(isRepeat);
  Serial.println();
  */
  irCode = aCommand;
}

void showBatteryLevel(void)
{
  // Display battery level bargraph on startup.  It's just a vague estimate
  // based on cell voltage (drops with discharge) but doesn't handle curve.
  uint16_t mV = tp.GetBatteryVoltage() * 1000;
  Serial.println(tp.GetBatteryVoltage() * 1000);
  uint8_t lvl = (mV >= BATT_MAX_MV) ? NUM_LEDS : // Full (or nearly)
                    (mV <= BATT_MIN_MV) ? 1
                                        : // Drained
                    1 + ((mV - BATT_MIN_MV) * NUM_LEDS + (NUM_LEDS / 2)) /
                            (BATT_MAX_MV - BATT_MIN_MV + 1); // # LEDs lit (1-NUM_LEDS)
  for (uint8_t i = 0; i < lvl; i++)
  {                                     // Each LED to batt level...
    uint8_t g = (i * 5 + 2) / NUM_LEDS; // Red to green
    strip.setPixelColor(i, 4 - g, g, 0);
    strip.show(); // Animate a bit
    delay(250 / NUM_LEDS);
  }
  delay(2500);   // Hold last state a moment
  strip.clear(); // Then clear strip
  strip.show();
}