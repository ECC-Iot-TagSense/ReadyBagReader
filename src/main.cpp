/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Constantly reads and outputs any tags heard

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide
  switch is in the 'SW-UART' position
*/

// Library for controlling the RFID module
#include <Arduino.h>
#include "SparkFun_UHF_RFID_Reader.h"
#include <SoftwareSerial.h>

#define rfidSerial softSerial // Software serial (eg. Arudino Uno or SparkFun RedBoard)
#define rfidBaud 38400
#define moduleType ThingMagic_M6E_NANO

#define SLIP_END 0xC0
#define SLIP_ESC 0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD

boolean setupRfidModule(long baudRate);

// Create instance of the RFID module
RFID rfidModule;
SoftwareSerial softSerial(2, 3); // RX, TX
int keepAliveCounter = 0;
uint8_t tagList[64][12];
size_t tagListSize = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ; // Wait for the serial port to come online

  auto resultRFID = setupRfidModule(rfidBaud);
  if (!resultRFID)
  {
    while (1)
      ; // Freeze!
  }

  rfidModule.setRegion(REGION_NORTHAMERICA); // Set to North America

  rfidModule.setReadPower(2000); // 5.00 dBm. Higher values may caues USB port to brown out
  // Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  rfidModule.startReading(); // Begin scanning for tags
}
namespace Loop
{
  uint8_t tagTmp[12];

} // namespace Loop

void loop()
{
  using namespace Loop;
  if (rfidModule.check() == true) // Check to see if any new data has come in from module
  {
    byte responseType = rfidModule.parseResponse(); // Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      keepAliveCounter++;
      // if (keepAliveCounter >= 2)
      // {
      Serial.write(SLIP_END);
      keepAliveCounter = 0;
      tagListSize = 0;
      // }
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      // If we have a full record we can pull out the fun bits

      byte tagEPCBytes = rfidModule.getTagEPCBytes(); // Get the number of bytes of EPC from response
      byte tagEPCFirstIndex = 31;

      // Print EPC bytes, this is a subsection of bytes from the response/msg array
      for (byte x = 0; x < tagEPCBytes; x++)
      {
        // Serial.print(rfidModule.msg[tagEPCFirstIndex + x], HEX);
        auto data = rfidModule.msg[tagEPCFirstIndex + x];
        tagTmp[x] = data;
      }

      bool isExist = false;
      for (size_t i = 0; i < tagListSize; i++)
      {
        bool isSame = true;
        for (size_t j = 0; j < 12; j++)
        {
          if (tagList[i][j] != tagTmp[j])
          {
            isSame = false;
            break;
          }
        }
        if (isSame)
        {
          isExist = true;
          break;
        }
      }
      if (isExist)
      {
        return;
      }

      for (size_t i = 0; i < 12; i++)
      {
        auto data = tagTmp[i];
        tagList[tagListSize][i] = data;
        if (data == SLIP_END || data == SLIP_ESC)
        {
          Serial.write(SLIP_ESC);
          if (data == SLIP_END)
          {
            Serial.write(SLIP_ESC_END);
          }
          else
          {
            Serial.write(SLIP_ESC_ESC);
          }
        }
        else
        {
          Serial.write(data);
        }
      }

      tagListSize++;
      // Serial.print(" ");
    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println("Bad CRC");
    }
    else if (responseType == RESPONSE_IS_HIGHRETURNLOSS)
    {
      Serial.println("High return loss, check antenna!");
    }
    else
    {
      // Unknown response
      Serial.println("Unknown error");
    }
  }
}

// Gracefully handles a reader that is already configured and already reading continuously
// Because Stream does not have a .begin() we have to do this outside the library
boolean setupRfidModule(long baudRate)
{
  rfidModule.begin(rfidSerial, moduleType); // Tell the library to communicate over serial port

  // Test to see if we are already connected to a module
  // This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  rfidSerial.begin(baudRate); // For this test, assume module is already at our desired baud rate
  delay(100);                 // Wait for port to open

  // About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (rfidSerial.available())
    rfidSerial.read();

  rfidModule.getVersion();

  if (rfidModule.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    // This happens if the baud rate is correct but the module is doing a ccontinuous read
    rfidModule.stopReading();

    // Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    // The module did not respond so assume it's just been powered on and communicating at 115200bps
    rfidSerial.begin(115200); // Start serial at 115200

    rfidModule.setBaud(baudRate); // Tell the module to go to the chosen baud rate. Ignore the response msg

    rfidSerial.begin(baudRate); // Start the serial port, this time at user's chosen baud rate

    delay(250);
  }

  // Test the connection
  rfidModule.getVersion();
  if (rfidModule.msg[0] != ALL_GOOD)
    return false; // Something is not right

  // The module has these settings no matter what
  rfidModule.setTagProtocol(); // Set protocol to GEN2

  rfidModule.setAntennaPort(); // Set TX/RX antenna ports to 1

  return true; // We are ready to rock
}
