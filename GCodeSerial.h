#ifndef GCODESERIAL_INCLUDED
#define GCODESERIAL_INCLUDED

#include "Arduino.h"
#include "CRC16.h"

// Set both to 0 for no error checking
#define USE_CRC16   (0)  // 1 to append CRC16 to commands
#define USE_CHECKSUM (0) // 1 to append checksum

// Class to output to serial, adding line numbers and checksums
class GCodeSerial : public Print
{
public:
  GCodeSerial(HardwareSerial& device) : serial(device) { }

  void begin(unsigned long baud);
  size_t write(uint8_t) override;
  int availableForWrite() override { return serial.availableForWrite(); }
  using Print::write;

private:
  HardwareSerial& serial;
#if USE_CRC16 || USE_CHECKSUM
  uint16_t lineNumber;
#endif
#if USE_CRC16
  CRC16 crc;
#elif USE_CHECKSUM
  uint8_t checksum;
#endif
#if USE_CRC16 || USE_CHECKSUM
  bool emptyLine;
#endif
};

#endif
