/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Name: Board (C)                                                                                                 //
// Author: Jeffrey Bednar                                                                                                  //
// Copyright (c) Illusion Interactive, 2011 - 2025.                                                                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Date: Tuesday, October 3rd, 2023
// Description: Source for prototyping board.
//
// Fold all: Ctrl + K + 0
// Unfold all: Ctrl + K + J
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program constants.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8_t PIN_BUZZ = 8;
const uint8_t PIN_FAN = 9;
const uint8_t PIN_RELAY = 6;
const uint8_t PIN_PROGRAM_ACTIVE = 2;
const uint8_t PIN_EXTERNAL_CLOCK = A1;  // We will use this as a digital pin.
const uint8_t LCD_MAX_X = 16;
const uint8_t LCD_MAX_Y = 2;
const uint32_t RANDOM_SEED = 255;                      // Constant randomSeed() starting point.
const uint8_t PIN_THERMISTOR_SENSE = A0;               // A0
const uint8_t THERMISTOR_NOMINAL_TEMPERATURE = 25;     // Almost always 25 degrees C; check datasheet.
const uint8_t THERMISTOR_READ_SAMPLES = 5;             // How many times the voltage is read before deciding an average value.
const uint16_t THERMISTOR_NOMINAL_RESISTANCE = 10000;  // Ohms
const uint16_t THERMISTOR_BETA = 3950;                 // Datasheet
const uint16_t THERMISTOR_REFERENCE = 10000;           // Ohms; resistor in the voltage divider.
const uint16_t LOOP_COMPLETED_DELAY = 750;             // MS
const float ZERO_KELVIN = 273.15;
const float TEMP_LOWERBOUND = 37.00;  // Fan off, single chirp.
const float TEMP_UPPERBOUND = 41.00;  // Fan on until lowerbound is reached, multiple chirps.
const float TEMP_MAXIMUM = 50.0;      // Fan on, different chirp tone. If enough cycles at or above this temperature, the board will shut down.
const uint8_t TEMP_MAXIMUM_COUNT = 10;
const int32_t EXTERNAL_CLOCK_LOWERBOUND = 0;
const int32_t EXTERNAL_CLOCK_UPPERBOUND = 99;
const uint8_t ERROR_CODES[3] = {
  0,  // No error, ignore.
  1,  // Invalid
  2,  // Too many cycles at or above maximum temperature.
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program globals and counters.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float tempAccumulatedSamples = 0;
float tempLifetimeMin = 999.9;                                         // Inverted to normalize during runtime.
float tempLifetimeMax = -999.9;                                        // Inverted to normalize during runtime.
int32_t externalClockLifetimeMin = EXTERNAL_CLOCK_UPPERBOUND << 1;     // Inverted to normalize during runtime.
int32_t externalClockLifetimeMax = -(EXTERNAL_CLOCK_LOWERBOUND << 1);  // Inverted to normalize during runtime.
bool tempFanHandled = false;
bool showSplashScreen = true;
uint8_t tempHighCurrentCount = 0;
uint32_t loopCount = 0;
uint8_t allPixels[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
};
char buffer[16];
char floatString[16];
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);  // SDA -> A4, SCL -> A5
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main program initialization.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // Set pin modes.
  pinMode(PIN_PROGRAM_ACTIVE, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_EXTERNAL_CLOCK, OUTPUT);

  randomSeed(RANDOM_SEED);

  // Init LCD.
  lcd.init();
  lcd.clear();
  delay(500);

  // Test LCD.
  testLcd(3);

  // Test buzzer.
  testBuzzer();

  // Test thermistor fan.
  testThermistorFan(3);

  // If tests are OK, latch power-on relay.
  setPowerOnLatch(HIGH);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main program loop.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  handleSplashScreen();
  handleLoopStart(1000);

  handleExternalClock(EXTERNAL_CLOCK_LOWERBOUND, EXTERNAL_CLOCK_UPPERBOUND, 0, 1000);

  float currentTemperature = handleThermistor(true, true, 1000);
  handleThermistorFan(currentTemperature, TEMP_LOWERBOUND, TEMP_UPPERBOUND, TEMP_MAXIMUM, 1000);

  handleLoopEnd();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generates a random number of clock cycles to be output to the external CC.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleExternalClock(int32_t rangeLowerbound, int32_t rangeUpperbound, uint8_t clockLengthMs, uint16_t cycleDelayMs) {
  int32_t randomNumber = random(rangeLowerbound, rangeUpperbound + 1);

  if (randomNumber < externalClockLifetimeMin) {
    externalClockLifetimeMin = randomNumber;
  }

  if (randomNumber > externalClockLifetimeMax) {
    externalClockLifetimeMax = randomNumber;
  }

  if (randomNumber > 0) {
    for (int32_t i = 0; i < randomNumber; i++) {
      digitalWrite(PIN_EXTERNAL_CLOCK, HIGH);
      if (clockLengthMs > 0) {
        delay(clockLengthMs);
      }
      digitalWrite(PIN_EXTERNAL_CLOCK, LOW);
      if (i < randomNumber - 1) {
        if (clockLengthMs > 0) {
          delay(clockLengthMs);
        }
      }
    }
  }

  // Write the random number.
  sprintf(buffer, "C: %ld", randomNumber);
  write(0, 0, buffer, true);
  delay(cycleDelayMs);

  // Write the minimum and maximum noticed.
  sprintf(buffer, "Cmin: %ld", externalClockLifetimeMin);
  write(0, 0, buffer, true);
  sprintf(buffer, "Cmax: %ld", externalClockLifetimeMax);
  write(0, 1, buffer, false);
  delay(cycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Engages the power-on latch to keep the board powered automatically.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setPowerOnLatch(uint8_t state) {
  digitalWrite(PIN_RELAY, state);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycles the power regulator cooling fan for test.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testThermistorFan(uint8_t cycles) {
  for (uint8_t i = 0; i < cycles; i++) {
    digitalWrite(PIN_FAN, HIGH);
    delay(750);
    digitalWrite(PIN_FAN, LOW);
    if (i < cycles - 1) {
      // Let it come to a stop.
      delay(1500);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plays a test tone for the buzzer.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testBuzzer() {
  playTone(1000, 50, 3, 250, true);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests the board LCD.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testLcd(uint8_t backlightCycles) {
  for (uint8_t i = 0; i < backlightCycles; i++) {
    lcd.backlight();
    delay(250);
    if (i < backlightCycles - 1) {
      lcd.noBacklight();
      delay(250);
    }
  }

  lcd.cursor_on();
  lcd.blink_on();

  lcd.setBacklight(255);
  lcd.setContrast(255);

  lcd.createChar(0, allPixels);

  // Test pixels.
  for (uint8_t i = 0; i < LCD_MAX_Y; i++) {
    for (uint8_t j = 0; j < LCD_MAX_X; j++) {
      lcd.setCursor(j, i);
      lcd.write(byte(0));
      delay(50);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Writes content to the board's LCD.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void write(uint8_t x, uint8_t y, const char* text, bool clearBeforeWrite) {
  if (x < LCD_MAX_X && y < LCD_MAX_Y) {
    if (clearBeforeWrite) {
      lcd.clear();
    }

    lcd.setCursor(x, y);
    lcd.print(text);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plays a tone on the board's buzzer.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void playTone(uint32_t frequency, int32_t durationMs, uint8_t cycles, uint16_t cycleDelayMs, bool pauseBeforeReturn) {
  for (uint8_t i = 0; i < cycles; i++) {
    tone(PIN_BUZZ, frequency, durationMs);
    if (i < cycles - 1) {
      // Wait until the tone finishes.
      delay(durationMs);
      delay(cycleDelayMs);
    }
  }

  // Avoid buzzer chirp during other transistions.
  if (pauseBeforeReturn) {
    delay(100);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main error handler.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void error(uint8_t errorCode, int frequency, long durationMs, uint8_t cycles, uint16_t cycleDelayMs, bool pauseBeforeReturn) {
  sprintf(buffer, "E: %d", errorCode);
  write(0, 0, buffer, true);
  playTone(frequency, durationMs, cycles, cycleDelayMs, pauseBeforeReturn);
  delay(10000);

  // Self terminate.
  setPowerOnLatch(LOW);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Displays a splash screen on the LCD.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleSplashScreen() {
  if (showSplashScreen) {
    write(0, 0, "Illusion", true);
    write(0, 1, "Interactive", false);
    showSplashScreen = false;
    delay(1000);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles actions at the loop start.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleLoopStart(uint16_t cycleDelayMs) {
  digitalWrite(PIN_PROGRAM_ACTIVE, HIGH);
  ++loopCount;

  // Write the random number.
  sprintf(buffer, "L: %ld", loopCount);
  write(0, 0, buffer, true);
  delay(cycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles actions at the loop end.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleLoopEnd() {
  digitalWrite(PIN_PROGRAM_ACTIVE, LOW);
  delay(LOOP_COMPLETED_DELAY);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles the interaction with the board's temperature probe. Returns temperature in Celcius.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float handleThermistor(uint8_t writeAdc, uint8_t writeResistance, uint16_t cycleDelayMs) {
  float tempAverage;

  tempAccumulatedSamples = 0;
  for (uint8_t i = 0; i < THERMISTOR_READ_SAMPLES; i++) {
    tempAccumulatedSamples += analogRead(PIN_THERMISTOR_SENSE);
    delay(10);
  }

  tempAverage = tempAccumulatedSamples / THERMISTOR_READ_SAMPLES;

  // Write the ADC average.
  if (writeAdc) {
    dtostrf(tempAverage, 1, 2, floatString);
    sprintf(buffer, "A: %s", floatString);
    write(0, 0, buffer, true);
    delay(cycleDelayMs);
  }

  // Calculate NTC resistance.
  tempAverage = 1023 / tempAverage - 1;
  tempAverage = THERMISTOR_REFERENCE / tempAverage;

  // Write the calculated resistance.
  if (writeResistance) {
    dtostrf(tempAverage, 1, 2, floatString);
    sprintf(buffer, "R: %s", floatString);
    write(0, 0, buffer, true);
    delay(cycleDelayMs);
  }

  float tempNow = tempAverage / THERMISTOR_NOMINAL_RESISTANCE;      // (R / Ro)
  tempNow = log(tempNow);                                           // ln(R / Ro)
  tempNow /= THERMISTOR_BETA;                                       // 1 / B * ln(R / Ro)
  tempNow += 1.0 / (THERMISTOR_NOMINAL_TEMPERATURE + ZERO_KELVIN);  // + (1 / To)
  tempNow = 1.0 / tempNow;                                          // Invert
  tempNow -= ZERO_KELVIN;                                           // Convert absolute temperature to Celcius.

  if (tempNow < tempLifetimeMin) {
    tempLifetimeMin = tempNow;
  }

  if (tempNow > tempLifetimeMax) {
    tempLifetimeMax = tempNow;
  }

  // Write the calculated temperature.
  dtostrf(tempNow, 1, 2, floatString);
  sprintf(buffer, "T: %s", floatString);
  write(0, 0, buffer, true);
  delay(cycleDelayMs);

  // Write the minimum and maximum noticed.
  dtostrf(tempLifetimeMin, 1, 2, floatString);
  sprintf(buffer, "Tmin: %s", floatString);
  write(0, 0, buffer, true);
  dtostrf(tempLifetimeMax, 1, 2, floatString);
  sprintf(buffer, "Tmax: %s", floatString);
  write(0, 1, buffer, false);
  delay(cycleDelayMs);

  return tempNow;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles the board's fan considering the current temperature and bounds.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleThermistorFan(float currentTemperature, float rangeLowerbound, float rangeUpperbound, float rangeMax, uint16_t cycleDelayMs) {
  if (currentTemperature >= rangeUpperbound) {
    // Different warning.
    if (currentTemperature >= rangeMax) {
      playTone(3000, 50, 8, 100, true);
      ///*
      tempHighCurrentCount++;
      // Turn power off if we're consistently running hot.
      if (tempHighCurrentCount >= TEMP_MAXIMUM_COUNT) {
        error(ERROR_CODES[2], 4000, 1000, 5, 250, true);
      }
      //*/
    } else {
      playTone(2000, 50, 3, 100, true);
    }
    digitalWrite(PIN_FAN, HIGH);
    tempFanHandled = false;
  } else if (currentTemperature <= rangeLowerbound) {
    // Avoid constantly chirping when below lowerbound.
    if (!tempFanHandled) {
      playTone(2000, 50, 1, 100, true);
      tempFanHandled = true;
    }
    digitalWrite(PIN_FAN, LOW);
  }

  dtostrf(rangeLowerbound, 1, 2, floatString);
  sprintf(buffer, "Fmin: %s", floatString);
  write(0, 0, buffer, true);
  dtostrf(rangeUpperbound, 1, 2, floatString);
  sprintf(buffer, "Fmax: %s", floatString);
  write(0, 1, buffer, false);
  delay(cycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////