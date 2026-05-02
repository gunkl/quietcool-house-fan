#include <esp_system.h>
#include <cc1101.h>

#define MOSI_PIN 6
#define MISO_PIN 16
#define CSN_PIN 4
#define CLK_PIN 18
#define GDO0_PIN 3
#define GDO2_PIN 15

using namespace CC1101;

Radio radio(CSN_PIN, CLK_PIN, MISO_PIN, MOSI_PIN, GDO0_PIN, GDO2_PIN);

const uint8_t syncWord[] = { 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa }; 
const uint8_t id[] =   { 0x2d, 0xd4, 0x06, 0xcb, 0x00, 0xd5, 0x12 };

const uint8_t syncLength = 9;
const uint8_t idLength = 7;

const uint8_t fanWake = 0x66;
const uint8_t fanOn =   0xbf;
const uint8_t fanLow =  0x1f;
const uint8_t fanHigh = 0x3f;
const uint8_t fanOff =  0x80;

const unsigned long repeatSpacing = 130;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (radio.begin() == STATUS_CHIP_NOT_FOUND) {
    Serial.println(F("Chip not found!"));
    while (true) { delay(1000); }
  }

  // Initialize the radio settings to match what QC remote sends
  radio.setModulation(MOD_2FSK);
  radio.setFrequency(433.92);
  radio.setFrequencyDeviation(10);
  radio.setDataRate(2.398);
  radio.setOutputPower(10);
  radio.setCrc(false);
  radio.setDataWhitening(false);
  radio.setManchester(false);
  radio.setFEC(false);
}

void getDeviceID() {
  uint8_t deviceID[idLength];

  memset(deviceID, 0, idLength);
  
	uint64_t chipid = ESP.getEfuseMac();
	deviceID[0] = 0;
	deviceID[1] = chipid;      
	deviceID[2] = chipid >> 8;
	deviceID[3] = chipid >> 16;
	deviceID[4] = chipid >> 24;
	deviceID[5] = chipid >> 32;
	deviceID[6] = chipid >> 40;

  Serial.print("Device ID is");
  dumpBytes(deviceID, idLength);
  Serial.println();
}

void sendCommand(uint8_t command) {
  const uint8_t fullMessageLength = syncLength + idLength + 2;
  uint8_t message[fullMessageLength];

  memset(message, 0, fullMessageLength);
  memcpy(message, syncWord, syncLength);
  memcpy(message + syncLength, id, idLength);
  memcpy(message + syncLength + idLength, &command, 1);
  memcpy(message + syncLength + idLength + 1, &command, 1);

  sendMessage(message, fullMessageLength);
}

void sendMessage(uint8_t *message, int len) {
  int i, j;

  // Force to fixed length packets based on current message
  Status status;
  radio.setPacketLengthMode(PKT_LEN_MODE_FIXED, len);

  radio.setSyncMode(SYNC_MODE_NO_PREAMBLE_CS);

  Serial.print("Sending message ");
  dumpBytes(message, len);
  Serial.print(" with length ");
  Serial.print(len);

  long now = 0;

  // QC expects each command sent three times
  for (i=0; i<3; i++) {
    now = millis();

    status = radio.transmit(message, len);

    // Check to see if there's an error
    if (status == STATUS_OK) {
      Serial.print(".");
    } else {
      Serial.print("[ERROR ");
      Serial.print(status);
      Serial.println("]");
    }

    // Delay until we hit the inter-message timout
    while ((millis()-now) < repeatSpacing) delay(10);
  }

  Serial.println("");
  Serial.println("Sent message 3 times");
}

void readMessage() {
  Serial.println("Ready to receive single command from remote");

  uint8_t buffer[255];
  size_t len = 255;

  radio.setSyncMode(SYNC_MODE_16_16_CS);
  radio.setPreambleLength(128);
  radio.setSyncWord(0xAAAA);
  radio.setPacketLengthMode(PKT_LEN_MODE_FIXED, 15);
  Status status = radio.receive((uint8_t*) buffer, len, &len);

  if (status == STATUS_OK ) {
    buffer[len] = '\0';

    Serial.print("Got packet:");
    dumpBytes(buffer, len);
    Serial.println("");

    // Still a few sync bits so read ID offsetting those
    uint8_t *id = buffer + 6;

    // Last two bytes are command repeated
    uint8_t command = buffer[len-1];
    
    Serial.print("Decodes as ID:");
    dumpBytes(id, idLength);
    Serial.println("");
    Serial.print("      Command:");

    if (command == fanWake) {
      Serial.println(" wake");
    } else if (command == fanOn) {
      Serial.println(" on");
    } else if (command == fanLow) {
      Serial.println(" low");
    } else if (command == fanHigh) {
      Serial.println(" high");
    } else if (command == fanOff) {
      Serial.println(" off");
    } else {
      Serial.println(" unrecognized");
    }
  } else {
    Serial.print("Nope: ");
    Serial.println(status);
  }
}

void dumpBytes(uint8_t *buffer, size_t len) {
  for (int i = 0; i < len; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void loop() {
  while (Serial.available() == 0) {
    delay(100);
  }

  Serial.println("");
  Serial.println("Send message: (w)ake, (o)n, (s)top, (h)igh, (l)ow OR (r)eceive, get (i)d");
  char input = Serial.read();

  if (input == 'w') {
    Serial.println("Sending 'wake'...");

    sendCommand(fanWake);
  } else if (input == 'o') {
    Serial.println("Sending 'on'...");

    sendCommand(fanOn);
  } else if (input == 's') {
    Serial.println("Sending 'stop'...");

    sendCommand(fanOff);
  } else if (input == 'l') {
    Serial.println("Sending 'low'...");

    sendCommand(fanLow);
  } else if (input == 'h') {
    Serial.println("Sending 'high'...");

    sendCommand(fanHigh);
  } else if (input == 'r') {
    readMessage();
  } else if (input == 'i') {
    getDeviceID();
  }
}
