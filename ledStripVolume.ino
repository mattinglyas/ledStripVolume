#include <FastLED.h>
#define GLOBAL_LOW_TOLERANCE 16 //global lowest voltage that counts as a signal
#define HIGH_PINREAD 512 //high voltage analog pin
#define NUM_LEDS 150
#define DATA_PIN 8
#define AMP_DRIVER_PIN 12 //5v out pin
#define IN_PIN A2 //analog in pin
#define INDICATOR_PIN 13 //led debug pin
#define COLOR_SHIFT_VALUE 250 // change in audio level to shift colors
#define COLOR_SHIFT_TOLERANCE 70 // low audio level to count as a "valley"

CRGB leds[NUM_LEDS]; //initialize all led pins

const int readings = 1; //rolling average readings count

static int auxVoltage[readings]; //historical pin readings
static int runningAverage = 0; //self explanatory
static int total = 0; //running total of voltage readings
static int readIndex = 0; //which part of auxVoltage the program is removing/adding
static int colorShift = 0;
static int adcKeyVal[5] = {30, 150, 360, 535, 760};
static int colorIndex = 0; //needs to be global for rgbChoose to work as necessary
static int previousValue = 0; // previous read value
static CRGB targetColor = {255, 255, 255}; //color in RGB of high voltage
static CRGB baseColor = {0, 0, 0}; //color in RGB of low voltage
static int hue = 0;

static boolean verboseMode = false; //log inputs or not
static boolean cascade = false;
static boolean randomColors = false;

static float deltaColor[3];

long lastColorShiftTime = millis(); //time in millis of the last color change
boolean peakReset = true; //flag to reset peak detection

void indicatorBlink(int litDuration, int darkDuration, int iterations) {
  for (int i = 0; i < iterations; i++) {
    digitalWrite(INDICATOR_PIN, HIGH);
    delay(litDuration);
    digitalWrite(INDICATOR_PIN, LOW);
    delay(darkDuration);
  }
}

void initializeDeltaColor() {
  for (int i = 0; i < 3; i++) {
    deltaColor[i] = targetColor[i] - baseColor[i];
  }
}

int getKey(unsigned int input) {
  int k;
  for (k = 0; k < 5; k++)
  {
    if (input < adcKeyVal[k])
    {
      return k;
    }
  }
  if (k >= 5)
    k = -1; // No valid key pressed
  return k;
}

void rgbChoose(int keyVal) {
  int key = getKey(analogRead(A7));
  int cycleColors[][3] = {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}};
  while (key == keyVal) {
    hue++;
    if (hue == 256) {
      hue = 0;
    }
    targetColor = CHSV(hue, 255, 255);

    for (int i = 0; i < 10; i++) {
      leds[i].setRGB(targetColor[0], targetColor[1], targetColor[2]);
    }
    FastLED.show();
    key = getKey(analogRead(A7));
  }
}

void setup() {
  Serial.begin(9600); //debug
  Serial.println("Waiting for button input...");
  pinMode(AMP_DRIVER_PIN, HIGH);
  digitalWrite(AMP_DRIVER_PIN, HIGH);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  //button selection setup
  int adcKeyIn;
  int key;
  while (key != 1) {
    adcKeyIn = analogRead(A7);
    key = getKey(adcKeyIn);
    if (key == 2) {
      cascade = true;
    }

    if (key == 3) {
      randomColors = true;
    }

    if (key == 4) {
      rgbChoose(4);
    }

    if (key == 0) {
      verboseMode = true;
    }
    delay(100);
  }

  //intialize all of auxVoltage at 0
  for (int i = 0; i < readings; i++) {
    auxVoltage[i] = 0;
  }

  initializeDeltaColor();

  if (verboseMode == true) {
    Serial.println("Verbose mode enabled...");
    Serial.print("Cascade: ");
    Serial.println(cascade);
    Serial.print("Color Shift: ");
    Serial.println(randomColors);
    Serial.println("Beginning audio stream...");
  } else {
    Serial.println("Verbose mode disabled...");
    Serial.println("Beginning audio stream...");
    Serial.end();
  }
}

void loop() {
  float runningOverDelta;
  int sensorValue = 0; //absolute read voltage 0-1023
  int endColor[3] = {0, 0, 0};

  for (int i = 0; i < 3; i++) {
    endColor[i] = 0;
  }

  total -= auxVoltage[readIndex]; //remove last input at readIndex from total
  auxVoltage[readIndex] = analogRead(IN_PIN); //replace readIndex with current input
  total += auxVoltage[readIndex]; //add new reading to readIndex
  readIndex++;

  //wrap readIndex
  if (readIndex >= readings) {
    readIndex = 0;
  }

  runningAverage = total / readings;
  if (runningAverage <= GLOBAL_LOW_TOLERANCE) {
    for (int i = 0; i < 3; i++) {
      endColor[i] = baseColor[i];
    }
  } else if (runningAverage > HIGH_PINREAD) {
    for (int i = 0; i < 3; i++) {
      endColor[i] = targetColor[i];
    }
  } else  {
    if (runningAverage < COLOR_SHIFT_TOLERANCE) {
      peakReset = true;
    }
    for (int i = 0; i < 3; i++) {
      runningOverDelta = runningAverage / (float) HIGH_PINREAD;
      endColor[i] = baseColor[i] + round(runningOverDelta * runningOverDelta * deltaColor[i]); //pow() is around 300 uS less effecient per calc
    }
  }

  if (randomColors && previousValue > runningAverage && previousValue > COLOR_SHIFT_VALUE && peakReset) {
    long lastColorShiftDelta = round((millis() - lastColorShiftTime)/3);
    if (lastColorShiftDelta > 359) {
      targetColor = CHSV(0,0,255);
    } else {
      targetColor = CHSV(lastColorShiftDelta,255,255);
    }
    lastColorShiftTime = millis();
    initializeDeltaColor();
    peakReset = false;
  }
  previousValue = runningAverage;

  if (cascade) {
    leds[0] = CRGB(endColor[0], endColor[1], endColor[2]);
    for (int i = 1; i < NUM_LEDS; i++) {
      leds[NUM_LEDS - i] = leds[NUM_LEDS - i - 1];
    }
  } else {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].setRGB(endColor[0], endColor[1], endColor[2]);
    }
  }
  
  FastLED.show();

  if (verboseMode) {
    Serial.print(auxVoltage[readIndex]);
    Serial.print("    ");
    Serial.print(total);
    Serial.print("    ");
    Serial.print(runningAverage);
    Serial.print("    ");
    for (int i = 0; i < 3; i++) {
      Serial.print(endColor[i]);
      Serial.print("    ");
    }
    Serial.print(cascade);
    Serial.print("    ");
    Serial.print(randomColors);
    Serial.println();
  }
}
