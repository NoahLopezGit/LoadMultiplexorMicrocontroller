#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> myCan1;  // CAN1

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);          // LED to show loop activity

  // Initialize  CAN bus
  myCan1.begin();
  // Set the baud rate for each CAN bus
  myCan1.setBaudRate(500000);

  Serial.println("CAN bus initialized.");
}
void loop() {
  digitalToggle(LED_BUILTIN);            // Toggle LED to show activity
  // Message to send
  CAN_message_t msg;
 
  msg.id = 0x100;
  msg.len = 8;
  for (int i=0; i<8; i++){
    msg.buf[i] = i;
  }

  myCan1.write(msg);

  delay(2000);  // Delay before sending the next message
}