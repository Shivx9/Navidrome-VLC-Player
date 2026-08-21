
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>




void init_rfid() {
  Serial.println("Setting up RFID");
  // pinMode(RFID_CS, OUTPUT);
  // digitalWrite(RFID_CS, HIGH);
  Serial.println("Initializing MFRC522 Reader...");
  for(int i=0;i<10;i++){
    mfrc522.PCD_Init();  // Initialize the RC522 hardware
    Serial.println("Printing RFID Debug check");
    MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);
    delay(1000);
  }
  
  Serial.println(F("\nSetup complete. Scan an RFID card or tag..."));
}

bool read_rfid() {
  // Serial.println("Checking for RFID");
  // Check for new cards, reset loop if none present
  if (!mfrc522.PICC_IsNewCardPresent()) {
    // Serial.println("No new RFID present");
    return false;
  }
  

  // Select one of the cards and read its serial UID
  if (!mfrc522.PICC_ReadCardSerial()) {
    // Serial.println("No serial read");
    return false;
  }

  // Card detected! Print the UID type and payload
  Serial.print(F("Card UID:"));
  int pos = 0;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    // sprintf(rfidChar[i], "%02X", mfrc522.uid.uidByte[i]);
    pos += sprintf(rfidChar + pos, "%02X", mfrc522.uid.uidByte[i]);
    // rfidVal[10]="\0";
  }
  Serial.println();

  // Print the credential tag type (e.g., MIFARE 1K)
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.print(F("PICC type: "));
  Serial.println(mfrc522.PICC_GetType(piccType));

  // Halt communication with the current card to prevent repeated reads
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD (Reader)
  mfrc522.PCD_StopCrypto1();
  return true;
}