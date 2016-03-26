#pragma once

/** This has been copied from TimeStylePebble (https://github.com/freakified/TimeStylePebble)*/

#include <pebble.h>

#define FONT_SETTING_DEFAULT 0
#define FONT_SETTING_BOLD    1

/*
 * Represents a single digit, as shown on the clock.
 */
typedef struct {
  int currentNum;
  GColor bgColor;
  GColor fgColor;
  GColor midColor1;
  GColor midColor2;
  GPoint position;
  uint32_t currentImageId;
  int currentFontId;
  GBitmap* currentImage;
  BitmapLayer* imageLayer;
} ClockDigit;

/*
 * Sets the number shown. Allocates the appropriate background image.
 */
void ClockDigit_setNumber(ClockDigit* this, int number, int fontId);
void ClockDigit_setBlank(ClockDigit* this);
void ClockDigit_setColor(ClockDigit* this, GColor fg, GColor bg);
void ClockDigit_offsetPosition(ClockDigit* this, int posOffset);

void ClockDigit_construct(ClockDigit* this, GPoint pos);
void ClockDigit_destruct(ClockDigit* this);
