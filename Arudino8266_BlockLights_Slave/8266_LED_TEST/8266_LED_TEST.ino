#include <FastLED.h>

#define NUM_LEDS 1
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

void setup() {

    Serial.begin(115200); // Initialize Serial communication
    //delay(5000); // 5 seconds to let you open the COM port if you need

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED

}

void loop() {
  // loop through RGB for testing
  leds[0] = CRGB::Black;
  FastLED.show();
  Serial.println("Set LED to black");
  delay(1000);
  leds[0] = CRGB::Red;
  FastLED.show();
  Serial.println("Set LED to red");
  delay(1000);
  leds[0] = CRGB::Green;
  FastLED.show();
  Serial.println("Set LED to green");
  delay(1000);
  leds[0] = CRGB::Blue;
  FastLED.show();
  Serial.println("Set LED to blue");
  delay(1000);  
}
