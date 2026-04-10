#include <FlexCAN_T4.h>
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> myCan2;  // CAN2

unsigned long wait_duration = 2000;  // Duration to wait before sending the next message (in milliseconds)
unsigned long last_send_time = 0;    // Variable to track the last time a message was sent
int relay_on_idx = 0;
int setpoint = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;  // Wait for Serial Monitor to open
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize CAN bus and set baud rate
  myCan2.begin();
  myCan2.setBaudRate(500000);

  Serial.println("CAN bus initialized.");

  pinMode(LED_BUILTIN, OUTPUT);  // turn on status LED to show successfull boot
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // Message to send/receive
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

  if (millis() - last_send_time > wait_duration) {  // Send a message every 2 seconds (with a small window to avoid multiple sends)
    // Message to send
    CAN_message_t msg;

    msg.id = 0x000;
    msg.len = 8;

    msg.buf[0] = 1;  // node id for command to be sent

    relay_on_idx += 1;  // Increment the relay index (4 total relays)
    relay_on_idx %= 4;
    setpoint = 0;
    setpoint = setpoint | (1 << relay_on_idx);  // Set the bit corresponding to the relay index
    msg.buf[1] = setpoint;                      // Set the first byte to the setpoint (which relay to turn on)

    myCan2.write(msg);
    last_send_time = millis();  // Update the last send time
  }
}
