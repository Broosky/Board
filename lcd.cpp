/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Name: Patchworks (C)                                                                                            //
// Author: Jeffrey Bednar                                                                                                  //
// Copyright (c) Illusion Interactive, 2011 - 2025.                                                                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Headers/lcd.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);  // SDA -> A4, SCL -> A5
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printLabeledFixed(uint8_t x, uint8_t y, const char* const label, fixed16_t value, bool clearBeforeWrite, uint16_t lcdPageCycleDelayMs, uint8_t shift) {
  char buffer[32];

  fixed16_t fixedOne = 1 << shift;

  int32_t component = value >> shift;
  int32_t fractionalRaw = value & (fixedOne - 1);  // Subtract for 0xFF... to mask out fractional.

  // Convert fraction to 3 decimal digits (multiply by 1000 then shift).
  int32_t fractionalPart = (fractionalRaw * 1000) >> shift;

  // Keep fractional part positive.
  if (value < 0) {
    fractionalPart = abs(fractionalPart);
  }

  snprintf(buffer, sizeof(buffer), "%s%ld.%02ld", label, (long)component, (long)fractionalPart);
  write(x, y, buffer, clearBeforeWrite);

  delay(lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printLabeledFloat(uint8_t x, uint8_t y, const char* const label, float value, bool clearBeforeWrite, uint16_t lcdPageCycleDelayMs) {
  char buffer[32], floatString[32];

  // Same precision as fixed print.
  dtostrf(value, 1, 2, floatString);
  snprintf(buffer, sizeof(buffer), "%s%s", label, floatString);
  write(x, y, buffer, clearBeforeWrite);

  delay(lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printLabeledInt(uint8_t x, uint8_t y, const char* const label, int32_t value, bool clearBeforeWrite, uint16_t lcdPageCycleDelayMs) {
  char buffer[32];

  snprintf(buffer, sizeof(buffer), "%s%ld", label, (long)value);
  write(x, y, buffer, clearBeforeWrite);

  delay(lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printLabeledString(uint8_t x, uint8_t y, const char* const label, const char* const text, bool clearBeforeWrite, uint16_t lcdPageCycleDelayMs) {
  char buffer[32];

  snprintf(buffer, sizeof(buffer), "%s%s", label, text);
  write(x, y, buffer, clearBeforeWrite);

  delay(lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printString(uint8_t x, uint8_t y, const char* const text, bool clearBeforeWrite, uint16_t lcdPageCycleDelayMs) {
  write(x, y, text, clearBeforeWrite);

  delay(lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printUptime(uint32_t days, uint32_t hours, uint32_t minutes, uint32_t seconds, uint16_t lcdPageCycleDelayMs) {
  char buffer1[32], buffer2[32];

  sprintf(buffer1, "UP: %02lu d %02lu h", days, hours);
  sprintf(buffer2, "UP: %02lu m %02lu s", minutes, seconds);

  printString(0, 0, buffer1, true, 0);
  printString(0, 1, buffer2, false, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void write(uint8_t x, uint8_t y, const char* const text, bool clearBeforeWrite) {
  if (x < LCD_MAX_X && y < LCD_MAX_Y) {
    if (clearBeforeWrite) {
      lcd.clear();
    }

    lcd.setCursor(x, y);
    lcd.print(text);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
