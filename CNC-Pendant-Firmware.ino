// CNC pendant interface to grblHAL
// D Crocker, started 2020-05-04

/* This Arduino sketch can be run on either Arduino Nano or Arduino Pro Micro. 
 * It should also work on an Arduino Uno (using the same wiring scheme as for the Nano) or Arduino Leonardo (using the same wiring scheme as for the Pro Micro).
 * The recommended board is the Arduino Pro Micro because the passthrough works without any modifications to the Arduino.

*/

// Configuration constants
const int PinA = 2;
const int PinB = 3;
const int PinX = 4;
const int PinY = 5;
const int PinZ = 6;
const int PinAxis4 = 7;
const int PinAxis5 = 8;
const int PinAxis6 = 9;
const int PinStop = A0;

#if defined(__AVR_ATmega32U4__)     // Arduino Micro, Pro Micro or Leonardo
const int PinTimes1 = A1;
const int PinTimes10 = A2;
const int PinTimes100 = A3;
const int PinLed = 10;
#endif

#if defined(__AVR_ATmega328P__)     // Arduino Nano or Uno
const int PinTimes1 = 10;
const int PinTimes10 = 11;
const int PinTimes100 = 12;
const int PinLed = 13;
#endif

const unsigned long BaudRate = 115200;
const int PulsesPerClick = 4;
const unsigned long MinCommandInterval = 50;

// Table of commands we send, one entry for each axis
const char* const MoveCommands[] =
{
  "G91 G0 F6000 X",     // X axis
  "G91 G0 F6000 Y",     // Y axis
  "G91 G0 F600 Z",      // Z axis
  "G91 G0 F6000 U",     // axis 4
  "G91 G0 F6000 V",     // axis 5
  "G91 G0 F6000 W"      // axis 6
};

#include "RotaryEncoder.h"
#include "GCodeSerial.h"

RotaryEncoder encoder(PinA, PinB, PulsesPerClick);

int serialBufferSize;
int distanceMultiplier;
int axis;
uint32_t whenLastCommandSent = 0;

const int axisPins[] = { PinX, PinY, PinZ, PinAxis4, PinAxis5, PinAxis6 };
const int feedAmountPins[] = { PinTimes1, PinTimes10, PinTimes100 };

#if defined(__AVR_ATmega32U4__)     // Arduino Leonardo or Pro Micro
# define UartSerial   Serial1
#elif defined(__AVR_ATmega328P__)   // Arduino Uno or Nano
# define UartSerial   Serial
#endif

GCodeSerial output(UartSerial);

void setup()
{
  pinMode(PinA, INPUT_PULLUP);
  pinMode(PinB, INPUT_PULLUP);
  pinMode(PinX, INPUT_PULLUP);
  pinMode(PinY, INPUT_PULLUP);
  pinMode(PinZ, INPUT_PULLUP);
  pinMode(PinAxis4, INPUT_PULLUP);
  pinMode(PinAxis5, INPUT_PULLUP);
  pinMode(PinAxis6, INPUT_PULLUP);
  pinMode(PinTimes1, INPUT_PULLUP);
  pinMode(PinTimes10, INPUT_PULLUP);
  pinMode(PinTimes100, INPUT_PULLUP);
  pinMode(PinStop, INPUT_PULLUP);
  pinMode(PinLed, OUTPUT);

  output.begin(BaudRate);

  serialBufferSize = output.availableForWrite();

#if defined(__AVR_ATmega32U4__)     // Arduino Leonardo or Pro Micro
  TX_RX_LED_INIT;
#endif
}

void loop()
{
  // 0. Poll the encoder. Ideally we would do this in the tick ISR, but after all these years the Arduino core STILL doesn't let us hook it.
  // We could possibly use interrupts instead, but if the encoder suffers from contact bounce then that isn't a good idea.
  // In practice this loop executes fast enough that polling it here works well enough
  encoder.poll();

  // 1. Check for emergency stop
  if (digitalRead(PinStop) == HIGH)
  {
    // Send emergency stop command every 2 seconds
    do
    {
      output.write(0x19);
      digitalWrite(PinLed, LOW);
      uint16_t now = (uint16_t)millis();
      while (digitalRead(PinStop) == HIGH && (uint16_t)millis() - now < 2000)
      {
        // Do nothing for now
      }
      encoder.getChange();      // ignore any movement
    } while (digitalRead(PinStop) == HIGH);

    output.write("$X\n");
  }

  digitalWrite(PinLed, HIGH);

  // 2. Poll the feed amount switch
  distanceMultiplier = 1;
  int localDistanceMultiplier = 1;
  for (int pin : feedAmountPins)
  {
    if (digitalRead(pin) == LOW)
    {
      distanceMultiplier = localDistanceMultiplier;
      break;
    }
    localDistanceMultiplier *= 10;
  }

  // 3. Poll the axis selector switch
  axis = -1;
  int localAxis = 0;
  for (int pin : axisPins)
  {
    if (digitalRead(pin) == LOW)
    {
      axis = localAxis;
      break;
    }
    ++localAxis;    
  }
  
  // 5. If the serial output buffer is empty, send a G0 command for the accumulated encoder motion.
  if (output.availableForWrite() == serialBufferSize)
  {
#if defined(__AVR_ATmega32U4__)     // Arduino Micro, Pro Micro or Leonardo
    TXLED1;                         // turn off transmit LED
#endif
    const uint32_t now = millis();
    if (now - whenLastCommandSent >= MinCommandInterval)
    {
      int distance = encoder.getChange() * distanceMultiplier;
      if (axis >= 0 && distance != 0)
      {
#if defined(__AVR_ATmega32U4__)     // Arduino Micro, Pro Micro or Leonardo
        TXLED0;                     // turn on transmit LED
#endif
        whenLastCommandSent = now;
        output.write(0x8B); // Toggle MPG mode on
        output.write(MoveCommands[axis]);
        if (distance < 0)
        {
          output.write('-');
          distance = -distance;
        }
        output.print(distance/10);
        output.write('.');
        output.print(distance % 10);
        output.write('\n');
        output.write(0x8B); // Toggle MPG mode off
      }
    }
  }
}

// End
