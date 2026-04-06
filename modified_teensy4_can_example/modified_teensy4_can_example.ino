#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> myCan1;  // CAN1
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> myCan2;  // CAN2

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial Monitor to open
  pinMode(LED_BUILTIN, OUTPUT);          // LED to show loop activity
  // Initialize each CAN bus
  myCan1.begin();
  // Set the baud rate for each CAN bus
  myCan1.setBaudRate(125000);  // Set baud rate to 1 Mbps for CAN1

  // Initialize each CAN bus
  myCan2.begin();
  // Set the baud rate for each CAN bus
  myCan2.setBaudRate(125000);  // Set baud rate to 1 Mbps for CAN2

  Serial.println("CAN buses initialized.");
}
void loop() {
  digitalToggle(LED_BUILTIN);            // Toggle LED to show activity
  // Message to send
  CAN_message_t msg;
  msg.len = 8;
  msg.id = 1;
  msg.buf[0] = 1;
  msg.buf[1] = 2;
  msg.buf[2] = 3;
  msg.buf[3] = 4;
  msg.buf[4] = 5;
  msg.buf[5] = 6;
  msg.buf[6] = 7;
  msg.buf[7] = 8;
  // Send message on CAN1
  if (myCan1.write(msg)) {
    Serial.println("Message sent on CAN1");
  }
 
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

  delay(2000);  // Delay before sending the next message
}