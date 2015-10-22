#include <FastLED.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
MPU6050 accelgyro;

int16_t ax, ay, az;

#define NUM_STRIPS 6
#define LED_PIN1    2
#define LED_PIN2    3
#define LED_PIN3    4
#define LED_PIN4    5
#define LED_PIN5    6
#define LED_PIN6    7
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    5

#define BRIGHTNESS  200
#define FRAMES_PER_SECOND 60

// The display size and color to use
const unsigned int myColor[NUM_STRIPS] = {
  0x400020, 0x004020, 0x402000, 0x000040, 0x400000, 0x004000
};

// These parameters adjust the vertical thresholds
const float maxLevel = 0.3;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 40.0; // total range to display, in decibels
const float linearBlend = 0.3;   // useful range is 0 to 0.7

// Audio library objects
AudioInputAnalog         adc1(A2);       //xy=99,55
AudioAnalyzeFFT256       fft;            //xy=265,75
AudioConnection          patchCord1(adc1, fft);

// This array holds the volume level (0 to 1.0) for each
// vertical pixel to turn on.  Computed in setup() using
// the 3 parameters above.
float thresholdVertical[NUM_LEDS];

// This array specifies how many of the FFT frequency bin
// to use for each horizontal pixel.  Because humans hear
// in octaves and FFT bins are linear, the low frequencies
// use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[NUM_STRIPS] = {
   11,  11,  21,  21,  22,  22
};

bool gReverseDirection = false;

CRGBPalette16 gPal;

CRGB leds[NUM_STRIPS][NUM_LEDS];

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  delay(3000); // sanity delay
  
  // the audio library needs to be given memory to start working
  AudioMemory(12);

  // compute the vertical thresholds before starting
  computeVerticalLevels();

  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
      Fastwire::setup(400, true);
  #endif
  
  // initialize serial communication
  Serial.begin(115200);

  // initialize device
  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();
    
  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // initialize LEDs
  Serial.println("Initializing LEDs...");
  FastLED.addLeds<CHIPSET, LED_PIN1, COLOR_ORDER>(leds[0], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN2, COLOR_ORDER>(leds[1], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN3, COLOR_ORDER>(leds[2], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN4, COLOR_ORDER>(leds[3], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN5, COLOR_ORDER>(leds[4], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN6, COLOR_ORDER>(leds[5], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );

  gPal = HeatColors_p;
}

enum eState {
  FIRE_2012,
  SOUND_REACT,
  FADE_RED,
  FADE_GREEN,
  FADE_BLUE,
  FADE_RGB
};

eState currentState = FIRE_2012;

void loop()
{
  // these methods (and a few others) are also available
  accelgyro.getAcceleration(&ax, &ay, &az);

  /*
  // display tab-separated accel x/y/z values
  Serial.print(ax); Serial.print("\t");
  Serial.print(ay); Serial.print("\t");
  Serial.println(az);
  */

  if (ax > 10000) {
    currentState = SOUND_REACT;
  } else if (ay > 10000) {
    currentState = FIRE_2012;
    gPal = HeatColors_p;
  } else if (az > 10000) {
    currentState = FADE_RED;
    gPal = LavaColors_p;
  } else if (ax < -10000) {
    currentState = FADE_GREEN;
    gPal = ForestColors_p;
  } else if (ay < -10000) {
    currentState = FADE_BLUE;
    gPal = OceanColors_p;
  } else if (az < -10000) {
    currentState = FADE_RGB;
    gPal = RainbowColors_p;
  }

  switch (currentState) {
    default:
    case FIRE_2012:
    case FADE_RED:
    case FADE_GREEN:
    case FADE_BLUE:
    case FADE_RGB:
      Fire2012();
      break;
    case SOUND_REACT:
      soundReact();
      break;
  }
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void soundReact() {
  unsigned int x, y, freqBin;
  float level;

  if (fft.available()) {
    // freqBin counts which FFT frequency data has been used,
    // starting at low frequency
    freqBin = 20;

    for (x=0; x < NUM_STRIPS; x++) {
      // get the volume for each horizontal pixel position
      level = fft.read(freqBin, freqBin + frequencyBinsHorizontal[x] - 1);
      // uncomment to see the spectrum in Arduino's Serial Monitor
      for (y=0; y < NUM_LEDS; y++) {
        // for each vertical pixel, check if above the threshold
        // and turn the LED on or off
        if (level >= thresholdVertical[y]) {
          leds[x][y] = CRGB(myColor[x]);
        } else {
          leds[x][y] = CRGB::Black;
        }
      }
      // increment the frequency bin count, so we display
      // low to higher frequency from left to right
      freqBin = freqBin + frequencyBinsHorizontal[x];
    }
  }  
}

// Run once from setup, the compute the vertical levels
void computeVerticalLevels() {
  unsigned int y;
  float n, logLevel, linearLevel;

  for (y=0; y < NUM_LEDS; y++) {
    n = (float)y / (float)(NUM_LEDS - 1);
    logLevel = pow10f(n * -1.0 * (dynamicRange / 20.0));
    linearLevel = 1.0 - n;
    linearLevel = linearLevel * linearBlend;
    logLevel = logLevel * (1.0 - linearBlend);
    thresholdVertical[y] = (logLevel + linearLevel) * maxLevel;
  }
}

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100 
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120


void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }
  
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }
    
  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160,255) );
  }

  // This outer loop will go over each strip, one at a time
  for(int x = 0; x < NUM_STRIPS; x++) {
    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      //CRGB color = HeatColor( heat[j]);
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( gPal, colorindex);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[x][pixelnumber] = color;
    }
  }
}

