#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

// ------------------------------
// Hardware & Pin Definitions
// ------------------------------
#define NUM_LEDS         96      // Total LEDs for the two rings (48 per ring)
#define RING_LEDS        48      // LEDs per ring
#define RING_DATA_PIN    5       // Data pin for the rings (GPIO5)
#define STATUS_LED_PIN   6       // Data pin for the status LED (GPIO6)

// Button pins (active LOW with internal pull-ups)
#define BUTTON_MODE_INC   1      // GPIO1: Increment mode
#define BUTTON_MODE_DEC   3      // GPIO3: Decrement mode
#define BUTTON_BRIGHT_INC 2      // GPIO2: Increase brightness
#define BUTTON_BRIGHT_DEC 4      // GPIO4: Decrease brightness

// ------------------------------
// Global Variables
// ------------------------------
volatile int currentMode = 0;        // Current mode (0 to numModes-1)
const int numModes = 27;              // Total modes: 0..26
volatile int ringBrightness = 50;     // LED ring brightness (0-255)

CRGB leds[NUM_LEDS];                 // FastLED array for the rings
Adafruit_NeoPixel statusLED(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800); // Single status LED

// ------------------------------
// WiFi & Web Server Setup in AP Mode
// ------------------------------

// New WiFi credentials for AP mode
const char* apSSID = "SmartGlassesP3";
const char* apPassword = "glasses2025";  // Choose a secure password

// Set up the WebServer on port 80
WebServer server(80);

// ------------------------------
// Forward Declarations of Tasks
// ------------------------------
void setupWiFi(void * parameter);
void webServerTask(void * parameter);
void ledRingTask(void * parameter);
void statusLEDTask(void * parameter);
void buttonTask(void * parameter);

// ------------------------------
// Utility Functions
// ------------------------------

// Gamma correction for perceptually linear brightness.
uint8_t gammaCorrect(uint8_t value) {
  float normalized = value / 255.0;
  normalized = pow(normalized, 2.2);
  return (uint8_t)(normalized * 255.0);
}

// Evenly distribute the 256 hue values over 'ringSize' LEDs using error accumulation.
void fill_ring_with_exact_rainbow(CRGB *ring, uint8_t ringSize, uint8_t startHue) {
  const float baseIncrement = 5.0;                   // floor(256/48) = 5
  const float exactStep = 256.0 / ringSize;            // ~5.33333
  const float fractional = exactStep - baseIncrement;  // ~0.33333
  
  float hue = startHue;
  float error = 0.0;
  for (uint8_t i = 0; i < ringSize; i++) {
    ring[i] = CHSV((uint8_t)hue, 255, 255);
    hue += baseIncrement;
    error += fractional;
    if (error >= 1.0) {
      hue += 1;
      error -= 1.0;
    }
  }
}

// ------------------------------
// FreeRTOS Task: WiFi Setup in AP Mode
// ------------------------------
void setupWiFi(void * parameter) {
  // Set the ESP32 to AP mode.
  WiFi.mode(WIFI_AP);
  // Start the soft AP with the given SSID and password.
  WiFi.softAP(apSSID, apPassword);
  Serial.println("ESP32 running in AP mode");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  // In AP mode, we don't wait for a connection.
  vTaskDelete(NULL); // End task once AP is set up.
}

// ------------------------------
// FreeRTOS Task: Web Server
// ------------------------------
void handleRoot() {
  server.send(200, "text/plain", "SmartGlasses ESP32");
}

void handleSet() {
  if (server.hasArg("mode")) {
    int modeVal = server.arg("mode").toInt();
    if (modeVal >= 0 && modeVal < numModes) {
      currentMode = modeVal;
    }
  }
  if (server.hasArg("brightness")) {
    int brightVal = server.arg("brightness").toInt();
    if (brightVal >= 0 && brightVal <= 255) {
      ringBrightness = brightVal;
      FastLED.setBrightness(ringBrightness);
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void webServerTask(void * parameter) {
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Web server started in AP mode");
  while (1) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ------------------------------
// FreeRTOS Task: LED Ring Updates
// ------------------------------
void ledRingTask(void * parameter) {
  // Variables for modes that need persistent state.
  static uint8_t startHueRing1 = 0;
  static uint8_t startHueRing2 = 0;
  
  // For Theatre Chase (mode 3)
  static uint8_t theatreOffset = 0;
  
  // For Side Wipe (mode 4)
  static int wipeExp = 0;                // Expansion counter for the wipe effect
  static CRGB wipeColor = CRGB::Blue;    // Starting wipe color
  static bool sideWipeInitialized = false; // Flag to clear the ring when mode is first entered
  if (currentMode != 4) {
    sideWipeInitialized = false;
  }

  // Global hue for the new modes:
  static uint8_t gHue = 0;  


  while (1) {
    switch(currentMode) {
      case 0: // Rainbow cycle
        fill_ring_with_exact_rainbow(leds, RING_LEDS, startHueRing1);
        fill_ring_with_exact_rainbow(leds + RING_LEDS, RING_LEDS, startHueRing2);
        startHueRing1++;
        startHueRing2++;
        FastLED.show();
        vTaskDelay(10 / portTICK_PERIOD_MS);
        break;
        
      case 1: // Solid blue
        fill_solid(leds, NUM_LEDS, CRGB::Blue);
        FastLED.show();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        break;
        
      case 2: // Flash white
        {
          static bool toggle = false;
          fill_solid(leds, NUM_LEDS, toggle ? CRGB::White : CRGB::Black);
          toggle = !toggle;
        }
        FastLED.show();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        break;
        
      case 3: // Theatre chase
        {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          for (int i = 0; i < NUM_LEDS; ++i) {
            if ((i + theatreOffset) % 3 == 0) {
              leds[i] = CRGB::White;
            }
          }
          theatreOffset++;
          FastLED.show();
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        break;
        
      case 4: // Side wipe
        {
          // On first entry to Mode 4, clear the ring.
          if (!sideWipeInitialized) {
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            wipeExp = 0;
            sideWipeInitialized = true;
            vTaskDelay(50 / portTICK_PERIOD_MS);
          }
          
          // For Ring 1 (LEDs 0..47): 
          // The wipe starts at the center (LED 0) and moves outward.
          // We assume the ring is arranged such that index 0 is the center,
          // and the numbers increase clockwise until 47 brings you back to the center.
          int idx1 = wipeExp;          // Moving from center outwards.
          int idx2 = 47 - wipeExp;       // The opposite side.
          if (idx1 <= idx2) {            // Ensure we don't overlap.
            leds[idx1] = wipeColor;
            leds[idx2] = wipeColor;
          }
          
          // For Ring 2 (LEDs 48..95): 
          // It starts at LED 48 (center) and moves outward; 95 is the other center.
          int idx3 = 48 + wipeExp;
          int idx4 = 95 - wipeExp;
          if (idx3 <= idx4) {
            leds[idx3] = wipeColor;
            leds[idx4] = wipeColor;
          }
          
          FastLED.show();
          vTaskDelay(15 / portTICK_PERIOD_MS); // Faster update
          
          wipeExp++;
          if (wipeExp > 24) {  // When the wipe covers half the ring.
            wipeExp = 0;
            // Toggle the wipe color so the new color overlays the old.
            wipeColor = (wipeColor == CRGB::Blue) ? CRGB::Red : CRGB::Blue;
          }
        }
        break;

      case 5: // Sparkle mode
        {
          fadeToBlackBy(leds, NUM_LEDS, 10);
          int pos = random(NUM_LEDS);
          leds[pos] = CHSV(random(256), 200, 255);
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
        }
        break;

      case 6: { // Mode 6: Sinelon – moving colored dot with trailing fade on each ring.
        // Fade out the background slightly.
        fadeToBlackBy(leds, NUM_LEDS, 20);
        
        // For Ring 1 (indices 0 to RING_LEDS-1), moving dot using beatsin8.
        uint8_t pos1 = beatsin8(10, 0, RING_LEDS - 1);
        leds[pos1] += CHSV(gHue, 200, 255);
        
        // For Ring 2 (indices RING_LEDS to NUM_LEDS-1), reverse the mapping so it moves counterclockwise.
        uint8_t pos2 = beatsin8(10, 0, RING_LEDS - 1);
        uint8_t pos2Mapped = (RING_LEDS - 1) - pos2;
        leds[RING_LEDS + pos2Mapped] += CHSV(gHue + 64, 200, 255);
        
        FastLED.show();
        vTaskDelay(20 / portTICK_PERIOD_MS);
        gHue++; // Advance global hue.
      }
        break;
      
      case 7: { // Mode 7: Juggle – multiple colored dots moving on each ring.
        fadeToBlackBy(leds, NUM_LEDS, 20);
        // For Ring 1, draw 4 moving dots.
        for (uint8_t i = 0; i < 4; i++) {
            int pos = beatsin16(7 + i, 0, RING_LEDS - 1, 0, i * 1000);
            leds[pos] += CHSV(gHue + i * 32, 200, 255);
        }
        // For Ring 2, do the same but reverse the index mapping.
        for (uint8_t i = 0; i < 4; i++) {
            int pos = beatsin16(7 + i, 0, RING_LEDS - 1, 0, i * 1000);
            uint8_t posMapped = (RING_LEDS - 1) - pos;
            leds[RING_LEDS + posMapped] += CHSV(gHue + 128 + i * 32, 200, 255);
        }
        FastLED.show();
        vTaskDelay(20 / portTICK_PERIOD_MS);
        gHue++;
      }
        break;
        
      case 8: // Running Lights – a moving bright dot on each ring with a fading trail.
        {
          // Static indices for the running dot in each ring.
          static int runIndex1 = 0;
          static int runIndex2 = 0;
          // Fade the entire strip slightly to create a trailing effect.
          fadeToBlackBy(leds, NUM_LEDS, 50);
          // For Ring 1 (indices 0 to RING_LEDS-1), light up the current dot.
          leds[runIndex1] = CRGB::White;
          // For Ring 2 (indices RING_LEDS to NUM_LEDS-1), light up the current dot.
          leds[runIndex2 + RING_LEDS] = CRGB::White;
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          runIndex1 = (runIndex1 + 1) % RING_LEDS;
          runIndex2 = (runIndex2 + 1) % RING_LEDS;
        }
        break;

      case 9: { // Laser Sweep effect – a narrow, bright band that moves along each ring
          const int bandWidth = 3; // Width of the laser band
          // Use static variables for head position on each ring.
          static int laserPos1 = 0;
          static int laserPos2 = 0;
          // Fade the rings slightly to produce a trailing effect.
          fadeToBlackBy(leds, NUM_LEDS, 40);
          // For Ring 1 (indices 0 to RING_LEDS-1), move clockwise.
          for (int i = 0; i < bandWidth; i++) {
              int pos = (laserPos1 + i) % RING_LEDS;
              leds[pos] = CHSV(gHue, 255, 255);
          }
          // For Ring 2 (indices RING_LEDS to NUM_LEDS-1), reverse the index for a counterclockwise sweep.
          for (int i = 0; i < bandWidth; i++) {
              int pos = (laserPos2 + i) % RING_LEDS;
              int mappedPos = (RING_LEDS - 1) - pos;
              leds[RING_LEDS + mappedPos] = CHSV(gHue + 64, 255, 255);
          }
          FastLED.show();
          vTaskDelay(20 / portTICK_PERIOD_MS);
          laserPos1 = (laserPos1 + 1) % RING_LEDS;
          laserPos2 = (laserPos2 + 1) % RING_LEDS;
          gHue++;
      }
        break;
      
      case 10: { // Strobe Fade effect – a rapid strobe flash that then fades out
          static bool strobeOn = false;
          static int strobeCount = 0;
          if (strobeOn) {
              // Flash the entire rings with a bright color.
              fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, ringBrightness));
              FastLED.show();
              vTaskDelay(30 / portTICK_PERIOD_MS);
              strobeCount++;
              if (strobeCount > 2) { // after a few flashes, turn off strobe
                  strobeOn = false;
                  strobeCount = 0;
              }
          } else {
              // Fade the rings to black.
              fadeToBlackBy(leds, NUM_LEDS, 80);
              FastLED.show();
              vTaskDelay(30 / portTICK_PERIOD_MS);
              // Random chance to restart strobe.
              if (random8() < 50) {
                  strobeOn = true;
              }
          }
          gHue++;
      }
        break;
      
      case 11: { // Orbiting Comets effect – two bright comets per ring orbit with trailing fades
          static int cometPos1 = 0;
          static int cometPos2 = 0;
          // Create fading trails.
          fadeToBlackBy(leds, NUM_LEDS, 40);
          // For Ring 1, plot two comets.
          leds[cometPos1] = CHSV(gHue, 255, 255);
          leds[cometPos2] = CHSV(gHue + 32, 255, 255);
          // For Ring 2, reverse mapping so motion appears counterclockwise.
          int mapped1 = (RING_LEDS - 1) - cometPos1;
          int mapped2 = (RING_LEDS - 1) - cometPos2;
          leds[RING_LEDS + mapped1] = CHSV(gHue + 64, 255, 255);
          leds[RING_LEDS + mapped2] = CHSV(gHue + 96, 255, 255);
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          cometPos1 = (cometPos1 + 1) % RING_LEDS;
          cometPos2 = (cometPos2 + 2) % RING_LEDS; // different speed for variety
          gHue++;
      }
        break;
      
      case 12: { // Color Bounce effect – a bouncing ball of color on each ring with trailing fade
          static int bouncePos1 = 0;
          static int bounceDir1 = 1;  // 1: forward, -1: backward
          static int bouncePos2 = 0;
          static int bounceDir2 = 1;
          // Fade the rings to create a trailing effect.
          fadeToBlackBy(leds, NUM_LEDS, 50);
          // For Ring 1, light the bouncing dot.
          leds[bouncePos1] = CHSV(gHue, 255, 255);
          // For Ring 2, reverse mapping for the bouncing dot.
          int mappedBounce = (RING_LEDS - 1) - bouncePos2;
          leds[RING_LEDS + mappedBounce] = CHSV(gHue + 64, 255, 255);
          FastLED.show();
          vTaskDelay(40 / portTICK_PERIOD_MS);
          bouncePos1 += bounceDir1;
          if (bouncePos1 >= RING_LEDS - 1 || bouncePos1 <= 0) {
              bounceDir1 = -bounceDir1;
          }
          bouncePos2 += bounceDir2;
          if (bouncePos2 >= RING_LEDS - 1 || bouncePos2 <= 0) {
              bounceDir2 = -bounceDir2;
          }
          gHue++;
      }
        break;
      
      case 13: { // Psychedelic Swirl effect – an organic noise-based pattern that evolves over time
          static uint16_t swirlX = 0;
          // For Ring 1:
          for (int i = 0; i < RING_LEDS; i++) {
              uint8_t noise = inoise8(i * 40 + swirlX);
              leds[i] = CHSV(noise, 255, ringBrightness);
          }
          // For Ring 2, reverse the index order.
          for (int i = 0; i < RING_LEDS; i++) {
              uint8_t noise = inoise8(i * 40 + swirlX);
              int mapped = (RING_LEDS - 1) - i;
              leds[RING_LEDS + mapped] = CHSV(noise + 64, 255, ringBrightness);
          }
          FastLED.show();
          vTaskDelay(20 / portTICK_PERIOD_MS);
          swirlX += 30;
      }
        break;

      case 14: { // Neon Grid effect – alternating moving stripes on each ring
          const int stripeWidth = 4; // number of LEDs per stripe
          int offset = gHue % (stripeWidth * 2); // dynamic offset based on global hue
          // For Ring 1:
          for (int i = 0; i < RING_LEDS; i++) {
              if (((i + offset) % (stripeWidth * 2)) < stripeWidth) {
                  leds[i] = CHSV(gHue, 255, ringBrightness);
              } else {
                  leds[i] = CRGB::Black;
              }
          }
          // For Ring 2 (reverse-mapped):
          for (int i = 0; i < RING_LEDS; i++) {
              int mapped = (RING_LEDS - 1) - i;
              if (((i + offset) % (stripeWidth * 2)) < stripeWidth) {
                  leds[RING_LEDS + mapped] = CHSV(gHue + 64, 255, ringBrightness);
              } else {
                  leds[RING_LEDS + mapped] = CRGB::Black;
              }
          }
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          gHue++; 
      }
      break;
      
      case 15: { // Echo Waves – a sine-wave brightness pattern traveling around each ring
          // For Ring 1:
          for (int i = 0; i < RING_LEDS; i++) {
              uint8_t bri = sin8(i * 10 + gHue);
              leds[i] = CHSV(gHue, 255, bri);
          }
          // For Ring 2 (reverse order):
          for (int i = 0; i < RING_LEDS; i++) {
              uint8_t bri = sin8(i * 10 + gHue);
              int mapped = (RING_LEDS - 1) - i;
              leds[RING_LEDS + mapped] = CHSV(gHue + 96, 255, bri);
          }
          FastLED.show();
          vTaskDelay(20 / portTICK_PERIOD_MS);
          gHue++;
      }
        break;
      
      case 16: { // Firefly Dance – random firefly-like dots appear and fade on each ring
          // Fade the entire array slightly to create trails.
          fadeToBlackBy(leds, NUM_LEDS, 30);
          // Randomly light up a dot on Ring 1.
          if (random8() < 80) {
              int pos = random(RING_LEDS);
              leds[pos] = CHSV(gHue, 200, 255);
          }
          // And on Ring 2 (with reverse mapping).
          if (random8() < 80) {
              int pos = random(RING_LEDS);
              int mapped = (RING_LEDS - 1) - pos;
              leds[RING_LEDS + mapped] = CHSV(gHue + 64, 200, 255);
          }
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          gHue++;
      }
        break;
      
      case 17: { // Spinning Bar – a wide colored bar rotates around each ring
          const int barWidth = 6; // width of the bar
          // For Ring 1:
          static int barPos1 = 0;
          fill_solid(leds, RING_LEDS, CRGB::Black);
          for (int i = 0; i < barWidth; i++) {
              int pos = (barPos1 + i) % RING_LEDS;
              leds[pos] = CHSV(gHue, 255, ringBrightness);
          }
          // For Ring 2 (reverse-mapped):
          static int barPos2 = 0;
          fill_solid(leds + RING_LEDS, RING_LEDS, CRGB::Black);
          for (int i = 0; i < barWidth; i++) {
              int pos = (barPos2 + i) % RING_LEDS;
              int mapped = (RING_LEDS - 1) - pos;
              leds[RING_LEDS + mapped] = CHSV(gHue + 64, 255, ringBrightness);
          }
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          barPos1 = (barPos1 + 1) % RING_LEDS;
          barPos2 = (barPos2 + 1) % RING_LEDS;
          gHue++;
      }
        break;
      
      case 18: { // Liquid Ripple – a ripple emanates from a moving center on each ring
          // For Ring 1:
          static int rippleCenter1 = 0;
          for (int i = 0; i < RING_LEDS; i++) {
              int d = abs(i - rippleCenter1);
              // Since the ring is circular, take the shorter distance.
              d = min(d, RING_LEDS - d);
              uint8_t bri = 255 - d * (255 / (RING_LEDS / 2));
              bri = constrain(bri, 0, 255);
              leds[i] = CHSV(gHue, 255, bri);
          }
          // For Ring 2 (reverse-mapped):
          static int rippleCenter2 = 0;
          for (int i = 0; i < RING_LEDS; i++) {
              int d = abs(i - rippleCenter2);
              d = min(d, RING_LEDS - d);
              uint8_t bri = 255 - d * (255 / (RING_LEDS / 2));
              bri = constrain(bri, 0, 255);
              int mapped = (RING_LEDS - 1) - i;
              leds[RING_LEDS + mapped] = CHSV(gHue + 128, 255, bri);
          }
          FastLED.show();
          vTaskDelay(40 / portTICK_PERIOD_MS);
          rippleCenter1 = (rippleCenter1 + 1) % RING_LEDS;
          rippleCenter2 = (rippleCenter2 + 1) % RING_LEDS;
          gHue++;
      }
        break;

      case 19: { // Full Throttle Pulse effect: full-ring pulsing at high brightness
          // beatsin8 produces a sine wave oscillating between 220 and 255 at 30 BPM.
          uint8_t pulseVal = beatsin8(30, 220, 255);
          // Fill both rings uniformly.
          fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, pulseVal));
          FastLED.show();
          vTaskDelay(20 / portTICK_PERIOD_MS);
          gHue++;  // Advance the hue for color variation.
      }
        break;

      case 20: { // Rave Strobe effect: rapid, full-bright color flashes
          // Fill Ring 1 with a bright color.
          fill_solid(leds, RING_LEDS, CHSV(gHue, 255, 255));
          // Fill Ring 2 similarly (using same color offset, since full fill means no mapping issues).
          fill_solid(leds + RING_LEDS, RING_LEDS, CHSV(gHue + 64, 255, 255));
          FastLED.show();
          vTaskDelay(20 / portTICK_PERIOD_MS);
          // Increase hue more rapidly for a very dynamic color cycle.
          gHue += 5;
      }
        break;

      case 21: { // Thunder Pulse effect: intermittent full-bright flashes with a strong pulse
          static int pulseCounter = 0;
          // Define base brightness and flash brightness.
          const uint8_t baseBri = 220;
          const uint8_t flashBri = 255;
          // For a 60-frame cycle, if the counter is less than 5, output a full flash.
          uint8_t bri = (pulseCounter % 60 < 5) ? flashBri : baseBri;
          // Fill both rings with the chosen brightness.
          fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, bri));
          FastLED.show();
          vTaskDelay(30 / portTICK_PERIOD_MS);
          pulseCounter = (pulseCounter + 1) % 60;
          gHue++;
      }
        break;

      case 22: { // Shockwave – an expanding full-bright burst from a random center
          static int shockExp = 0;
          static int shockCenter1 = random(RING_LEDS);
          static int shockCenter2 = random(RING_LEDS);
          
          // Clear both rings completely.
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          
          // For Ring 1: light all LEDs within shockExp distance of shockCenter1.
          for (int i = 0; i < RING_LEDS; i++) {
              int d = abs(i - shockCenter1);
              if (d > RING_LEDS/2) d = RING_LEDS - d;
              if (d <= shockExp) {
                  leds[i] = CHSV(gHue, 255, 255);
              }
          }
          
          // For Ring 2: do the same with reverse mapping.
          for (int i = 0; i < RING_LEDS; i++) {
              int d = abs(i - shockCenter2);
              if (d > RING_LEDS/2) d = RING_LEDS - d;
              if (d <= shockExp) {
                  int mapped = (RING_LEDS - 1) - i;
                  leds[RING_LEDS + mapped] = CHSV(gHue + 32, 255, 255);
              }
          }
          
          FastLED.show();
          vTaskDelay(10 / portTICK_PERIOD_MS);
          shockExp += 3; // Rapidly expand the wave.
          if (shockExp > 12) {
               shockExp = 0;
               shockCenter1 = random(RING_LEDS);
               shockCenter2 = random(RING_LEDS);
               gHue++;  // Advance hue on reset.
          }
      }
        break;

      case 23: { // Bass Drop – full-bright flash every 4 frames, then off
          static int dropCounter = 0;
          if (dropCounter % 4 == 0) {
              // On drop frame: fill both rings with a full, vivid color.
              fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
          } else {
              // Otherwise, total blackout.
              fill_solid(leds, NUM_LEDS, CRGB::Black);
          }
          FastLED.show();
          vTaskDelay(100 / portTICK_PERIOD_MS);
          dropCounter++;
          gHue += 3;
      }
        break;

      case 24: { // Dynamic Bar Graph – 3 massive full-bright bars per ring
          const int bars = 3;
          int barWidth = RING_LEDS / bars; // 16 LEDs per bar
          for (int b = 0; b < bars; b++) {
              // For each bar, choose a height between 12 and 16.
              uint8_t height1 = random8(1, barWidth + 1);
              uint8_t height2 = random8(1, barWidth + 1);
              // For Ring 1:
              for (int i = 0; i < barWidth; i++) {
                  int idx = b * barWidth + i;
                  if (i < height1)
                      leds[idx] = CHSV((gHue + b * 40) % 256, 255, 255);
                  else
                      leds[idx] = CRGB::Black;
              }
              // For Ring 2 (reverse order within the bar):
              for (int i = 0; i < barWidth; i++) {
                  int revIdx = b * barWidth + (barWidth - 1 - i);
                  if (i < height2)
                      leds[RING_LEDS + revIdx] = CHSV((gHue + 128 + b * 40) % 256, 255, 255);
                  else
                      leds[RING_LEDS + revIdx] = CRGB::Black;
              }
          }
          FastLED.show();
          vTaskDelay(80 / portTICK_PERIOD_MS);
          gHue++;
      }
        break;

      case 25: { // Sonic Slicer – a narrow, high-bright slice sweeping around the rings
          const int sliceWidth = 6;
          static int slicePos1 = 0;
          static int slicePos2 = 0;
          // Apply a slight fade to create a trailing effect.
          fadeToBlackBy(leds, NUM_LEDS, 80);
          
          // For Ring 1 (clockwise):
          for (int i = 0; i < sliceWidth; i++) {
              int pos = (slicePos1 + i) % RING_LEDS;
              leds[pos] = CHSV(gHue, 255, 255);
          }
          // For Ring 2 (counterclockwise; reverse mapping):
          for (int i = 0; i < sliceWidth; i++) {
              int pos = (slicePos2 + i) % RING_LEDS;
              int mapped = (RING_LEDS - 1) - pos;
              leds[RING_LEDS + mapped] = CHSV(gHue + 64, 255, 255);
          }
          FastLED.show();
          vTaskDelay(10 / portTICK_PERIOD_MS);
          slicePos1 = (slicePos1 + 2) % RING_LEDS;
          slicePos2 = (slicePos2 + 2) % RING_LEDS;
          gHue += 2;
      }
        break;
      
      case 26: { // Radial Surge – a dark gap sweeps around full-bright rings
          const int gapWidth = 4; // Width of the dark gap
          static int gapPos1 = 0;
          static int gapPos2 = 0;
          // Fill both rings fully with bright color.
          fill_solid(leds, RING_LEDS, CHSV(gHue, 255, 255));
          fill_solid(leds + RING_LEDS, RING_LEDS, CHSV(gHue + 64, 255, 255));
          
          // Carve a dark gap in Ring 1.
          for (int i = 0; i < gapWidth; i++) {
              int pos = (gapPos1 + i) % RING_LEDS;
              leds[pos] = CRGB::Black;
          }
          // Carve a dark gap in Ring 2 (reverse mapping).
          for (int i = 0; i < gapWidth; i++) {
              int pos = (gapPos2 + i) % RING_LEDS;
              int mapped = (RING_LEDS - 1) - pos;
              leds[RING_LEDS + mapped] = CRGB::Black;
          }
          
          FastLED.show();
          vTaskDelay(50 / portTICK_PERIOD_MS);
          gapPos1 = (gapPos1 + 3) % RING_LEDS;
          gapPos2 = (gapPos2 + 3) % RING_LEDS;
          gHue++;
      }
        break;
      
      default:
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        break;
    }
  }
}

// ------------------------------
// FreeRTOS Task: Status LED Heartbeat
// ------------------------------
void statusLEDTask(void * parameter) {
  // Variables for heartbeat effect.
  static int statusBrightness = 0;
  static bool increasing = true;
  static uint8_t statusHue = 0;
  const uint8_t NUM_PARTITIONS = 12;
  const uint8_t hueStep = 256 / NUM_PARTITIONS;
  static bool pulseRestarted = false;
  const int brightnessStep = 3;
  
  while (1) {
    if (WiFi.softAPgetStationNum() == 0) {
      // Not connected: flash RED every 200ms.
      static bool redState = false;
      redState = !redState;
      statusLED.setPixelColor(0, redState ? statusLED.Color(255, 0, 0) : statusLED.Color(0, 0, 0));
      statusLED.show();
      vTaskDelay(200 / portTICK_PERIOD_MS);
    } else {
      // WiFi connected: RGB heartbeat effect.
      if (increasing) {
        statusBrightness += brightnessStep;
        if (statusBrightness >= 255) {
          statusBrightness = 255;
          increasing = false;
        }
        pulseRestarted = false;
      } else {
        statusBrightness -= brightnessStep;
        if (statusBrightness <= 0) {
          statusBrightness = 0;
          if (!pulseRestarted) {
            statusHue += hueStep;
            pulseRestarted = true;
          }
          increasing = true;
        }
      }
      uint8_t correctedBrightness = gammaCorrect((uint8_t)statusBrightness);
      CHSV hsvStatus(statusHue, 255, correctedBrightness);
      CRGB rgbStatus;
      hsv2rgb_rainbow(hsvStatus, rgbStatus);
      statusLED.setPixelColor(0, rgbStatus.r, rgbStatus.g, rgbStatus.b);
      statusLED.show();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

// ------------------------------
// FreeRTOS Task: Button Scanning
// ------------------------------
void buttonTask(void * parameter) {
  const int debounceDelay = 50; // milliseconds
  int lastModeIncState   = HIGH;
  int lastModeDecState   = HIGH;
  int lastBrightIncState = HIGH;
  int lastBrightDecState = HIGH;
  
  while (1) {
    int stateModeInc   = digitalRead(BUTTON_MODE_INC);
    int stateModeDec   = digitalRead(BUTTON_MODE_DEC);
    int stateBrightInc = digitalRead(BUTTON_BRIGHT_INC);
    int stateBrightDec = digitalRead(BUTTON_BRIGHT_DEC);
    
    // Increment mode.
    if (stateModeInc == LOW && lastModeIncState == HIGH) {
      currentMode = (currentMode + 1) % numModes;
      Serial.print("Mode increased to ");
      Serial.println(currentMode);
      vTaskDelay(debounceDelay / portTICK_PERIOD_MS);
    }
    // Decrement mode.
    if (stateModeDec == LOW && lastModeDecState == HIGH) {
      currentMode = (currentMode - 1);
      if (currentMode < 0)
        currentMode = numModes - 1;
      Serial.print("Mode decreased to ");
      Serial.println(currentMode);
      vTaskDelay(debounceDelay / portTICK_PERIOD_MS);
    }
    // Increase brightness.
    if (stateBrightInc == LOW && lastBrightIncState == HIGH) {
      ringBrightness += 10;
      if (ringBrightness > 255)
        ringBrightness = 255;
      FastLED.setBrightness(ringBrightness);
      Serial.print("Brightness increased to ");
      Serial.println(ringBrightness);
      vTaskDelay(debounceDelay / portTICK_PERIOD_MS);
    }
    // Decrease brightness.
    if (stateBrightDec == LOW && lastBrightDecState == HIGH) {
      ringBrightness -= 10;
      if (ringBrightness < 0)
        ringBrightness = 0;
      FastLED.setBrightness(ringBrightness);
      Serial.print("Brightness decreased to ");
      Serial.println(ringBrightness);
      vTaskDelay(debounceDelay / portTICK_PERIOD_MS);
    }
    
    lastModeIncState   = stateModeInc;
    lastModeDecState   = stateModeDec;
    lastBrightIncState = stateBrightInc;
    lastBrightDecState = stateBrightDec;
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ------------------------------
// Setup & Loop
// ------------------------------
void setup() {
  Serial.begin(115200);
  
  // Initialize LED rings (FastLED) and status LED.
  FastLED.addLeds<WS2812B, RING_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(ringBrightness);
  
  statusLED.begin();
  statusLED.show();
  
  // Setup button pins (with internal pull-ups)
  pinMode(BUTTON_MODE_INC,   INPUT_PULLUP);
  pinMode(BUTTON_MODE_DEC,   INPUT_PULLUP);
  pinMode(BUTTON_BRIGHT_INC, INPUT_PULLUP);
  pinMode(BUTTON_BRIGHT_DEC, INPUT_PULLUP);
  
  // Seed random for Sparkle mode.
  randomSeed(analogRead(0));
  
  // Create FreeRTOS tasks (all pinned to core 0 for the ESP32-C3)
  xTaskCreatePinnedToCore(setupWiFi,      "WiFi Task",      4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledRingTask,    "LED Ring Task",  4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(statusLEDTask,  "Status LED Task",4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(buttonTask,     "Button Task",    2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(webServerTask,  "WebServer Task", 4096, NULL, 1, NULL, 0);
}

void loop() {
  // Empty – all work is done in FreeRTOS tasks.
}