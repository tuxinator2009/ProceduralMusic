#include <cstdint>
#include <Pokitto.h>
#include <File>
#include <LibAudio>
#include <LibSchedule>
#include "bitmaps.h"
#include "FMSynthSource.h"
#include "music.h"
#include "palette.h"

using PC=Pokitto::Core;
using PD=Pokitto::Display;
using PB=Pokitto::Buttons;

constexpr uint8_t BTN_MASK_UP = 1 << UPBIT;
constexpr uint8_t BTN_MASK_DOWN = 1 << DOWNBIT;
constexpr uint8_t BTN_MASK_LEFT = 1 << LEFTBIT;
constexpr uint8_t BTN_MASK_RIGHT = 1 << RIGHTBIT;
constexpr uint8_t BTN_MASK_A = 1 << ABIT;
constexpr uint8_t BTN_MASK_B = 1 << BBIT;
constexpr uint8_t BTN_MASK_C = 1 << CBIT;

uint8_t buttonsPreviousState = 0;
uint8_t buttonsJustPressed = 0;
uint8_t currentPacing = 0;
uint8_t currentTone = 0;
uint8_t cursorPacing = 0;
uint8_t cursorTone = 0;
bool firstUpdate = true;

uint8_t trebleClefLocations[] = {83, 79, 74, 70, 65, 61, 56, 52, 47, 43, 38, 34};
uint8_t bassClefLocations[] = {144, 140, 135, 131, 126, 122, 117, 113, 108, 104, 99, 95};

bool justPressed(uint8_t mask)
{
  if ((buttonsJustPressed & mask) != 0)
    return true;
  return false;
}

void printValue(uint16_t value)
{
  if (value < 10)
    PD::set_cursor(PD::cursorX + PD::fontWidth * 2, PD::cursorY);
  else if (value < 100)
    PD::set_cursor(PD::cursorX + PD::fontWidth, PD::cursorY);
  PD::print(value);
}

void drawNote(uint8_t x, uint8_t note, uint8_t index)
{
  for (int i = 0; i < 12; ++i)
  {
    if (note == ProceduralMusic::trebleNotes[i])
    {
      if (i == 0)
      {
        PD::setColor(1);
        PD::drawFastHLine(x - 2, trebleClefLocations[i] + 28, 13);
      }
      return PD::drawBitmap(x, trebleClefLocations[i], notesBMP, index);
    }
    else if (note == ProceduralMusic::bassNotes[i])
    {
      if (i == 0)
      {
        PD::setColor(1);
        PD::drawFastHLine(x - 2, bassClefLocations[i] + 28, 13);
      }
      return PD::drawBitmap(x, bassClefLocations[i], notesBMP, index);
    }
  }
}

void drawInfo()
{
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      if (currentPacing == i && currentTone == j)
        PD::setColor(3);
      else
        PD::setColor(1);
      PD::drawRect(i * 8, j * 8, 7, 7);
    }
  }
  PD::setColor(2);
  PD::drawRect(2 + cursorPacing * 8, 2 + cursorTone * 8, 3, 3);
  PD::setColor(2);
  PD::set_cursor(40, 0);
  PD::print("Tempo: ");
  PD::setColor(1);
  PD::print((4 * 60000) / ProceduralMusic::tempos[ProceduralMusic::parameter % 4]);
  PD::setColor(2);
  PD::print("  Chords: ");
  PD::setColor(1);
  if (ProceduralMusic::parameters[ProceduralMusic::parameter].bassChords)
    PD::print("Bass Clef");
  else
    PD::print("Treble Clef");
  PD::setColor(2);
  PD::set_cursor(40, 8);
  PD::print("Notes: 1=");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[0]);
  PD::print("%");
  PD::setColor(2);
  PD::print(" 2=");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[1] - (uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[0]);
  PD::print("%");
  PD::setColor(2);
  PD::print(" 4=");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[2] - (uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[1]);
  PD::print("%");
  PD::setColor(2);
  PD::print(" 8=");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[3] - (uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].durationChances[2]);
  PD::print("%");
  PD::setColor(2);
  PD::set_cursor(40, 16);
  PD::print("Step: ");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].movementChances[0]);
  PD::print("%");
  PD::setColor(2);
  PD::print(" Skip: ");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].movementChances[1] - (uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].movementChances[0]);
  PD::print("%");
  PD::setColor(2);
  PD::print(" Leap: ");
  PD::setColor(1);
  PD::print((uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].movementChances[2] - (uint16_t)ProceduralMusic::parameters[ProceduralMusic::parameter].movementChances[1]);
  PD::print("%");
}

void drawMeasure()
{
  uint8_t noteIndex;
  uint8_t slot = 0;
  PD::setColor(1);
  PD::drawFastVLine(0, 66, 97);
  for (int i = 0; i < 5; ++i)
  {
    PD::drawFastHLine(0, 66 + i * 9, 220);
    PD::drawFastHLine(0, 127 + i * 9, 220);
  }
  PD::drawBitmap(2, 46, trebleClefBMP, 0);
  PD::drawBitmap(2, 127, bassClefBMP, 0);
  PD::drawBitmap(36, 67, timeSignatureBMP, 0);
  PD::drawBitmap(36, 128, timeSignatureBMP, 0);
  drawNote(57, ProceduralMusic::measure.chord[0], 0);
  drawNote(57, ProceduralMusic::measure.chord[1], 0);
  drawNote(57, ProceduralMusic::measure.chord[2], 0);
  for (int i = 0; i < ProceduralMusic::measure.numNotes; ++i)
  {
    if (ProceduralMusic::measure.durations[i] == 1)
      noteIndex = 0;
    else if (ProceduralMusic::measure.durations[i] == 2)
      noteIndex = 1;
    else if (ProceduralMusic::measure.durations[i] == 4)
      noteIndex = 2;
    else if (ProceduralMusic::measure.durations[i] == 8)
      noteIndex = 3;
    if (i == ProceduralMusic::currentNote - 1)
      noteIndex += 4;
    drawNote(57 + slot * 18, ProceduralMusic::measure.melody[i], noteIndex);
    slot += 8 / ProceduralMusic::measure.durations[i];
  }
  PD::setColor(1);
  PD::drawFastVLine(201, 66, 97);
  drawNote(205, ProceduralMusic::nextMeasure.chord[0], 0);
  drawNote(205, ProceduralMusic::nextMeasure.chord[1], 0);
  drawNote(205, ProceduralMusic::nextMeasure.chord[2], 0);
  if (ProceduralMusic::nextMeasure.durations[0] == 1)
    noteIndex = 0;
  else if (ProceduralMusic::nextMeasure.durations[0] == 2)
    noteIndex = 1;
  else if (ProceduralMusic::nextMeasure.durations[0] == 4)
    noteIndex = 2;
  else if (ProceduralMusic::nextMeasure.durations[0] == 8)
    noteIndex = 3;
  drawNote(205, ProceduralMusic::nextMeasure.melody[0], noteIndex);
  PD::setColor(3);
  PD::drawFastVLine(57 + (PC::getTime() - ProceduralMusic::measureTime) * 144 / ProceduralMusic::tempos[ProceduralMusic::parameter % 4], 34, 142);
}

void init()
{
  PD::setColorDepth(4);
  PD::invisiblecolor = 0;
  PD::setFont(font5x7);
  PD::loadRGBPalette(palette);
  PB::pollButtons();
  while (PB::buttons_state != 0)
    PB::pollButtons();
}

void update()
{
  uint32_t startTime = PC::getTime();
  uint32_t endTime;
  if (firstUpdate)
    ProceduralMusic::begin();
  firstUpdate = false;
  PB::pollButtons();
  buttonsJustPressed = ~buttonsPreviousState & PB::buttons_state;
  buttonsPreviousState = PB::buttons_state;
  if (justPressed(BTN_MASK_LEFT) && cursorPacing > 0)
    --cursorPacing;
  else if (justPressed(BTN_MASK_RIGHT) && cursorPacing < 3)
    ++cursorPacing;
  else if (justPressed(BTN_MASK_UP) && cursorTone > 0)
    --cursorTone;
  else if (justPressed(BTN_MASK_DOWN) && cursorTone < 3)
    ++cursorTone;
  if (justPressed(BTN_MASK_A))
  {
    currentPacing = cursorPacing;
    currentTone = cursorTone;
    ProceduralMusic::desiredParameter = currentTone * 4 + currentPacing;
  }
  PD::clear();
  drawInfo();
  drawMeasure();
}
