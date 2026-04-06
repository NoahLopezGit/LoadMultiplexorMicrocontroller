#include <FlexCAN_T4.h>
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> myCan2;  // CAN2

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial Monitor to open
  pinMode(LED_BUILTIN, OUTPUT);          // LED to show loop activity

  // Initialize  CAN bus
  myCan2.begin();
  // Set the baud rate for each CAN bus
  myCan2.setBaudRate(500000);  // Set baud rate to 1 Mbps for CAN2

  Serial.println("CAN bus initialized.");

  pinMode(LED_BUILTIN, OUTPUT);  // turn on status LED to show successfull boot
  digitalWrite(LED_BUILTIN, HIGH);
}
void loop() {
  // Message to send
  CAN_message_t msg;
 
  // Check for received messages on CAN1
  if (myCan2.read(msg)) {
    Serial.print("Received on CAN2: ID=0x");
    Serial.print(msg.id, HEX);
    Serial.print(" Data=");
    for (int i = 0; i < msg.len; i++) {
      Serial.print(msg.buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

}