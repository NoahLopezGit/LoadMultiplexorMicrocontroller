#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1; //CAN1 selects which can to use, for teensy 4.1 this would be CTRX1 and RTRX1
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2;
CAN_message_t rx_msg;
CAN_message_t tx_msg;



void setup(void) {
  can1.begin();
  can1.setBaudRate(250000);
  can2.begin();
  can2.setBaudRate(250000);
}

void loop() {
  
  // adding data to message
  tx_msg.id = 0x123;
  tx_msg.len = 8;
  tx_msg.flags.extended = false;
  tx_msg.buf[0] = 0; 
  tx_msg.buf[1] = 1;
  tx_msg.buf[2] = 2;
  tx_msg.buf[3] = 3;
  tx_msg.buf[4] = 4;
  tx_msg.buf[5] = 5;
  tx_msg.buf[6] = 6;
  tx_msg.buf[7] = 7;

  // write(const CAN_message_t &msg) -> int
  can1.write(tx_msg);

  if ( can2.read(rx_msg) ) {
    Serial.print("CAN2 "); 
    Serial.print("MB: "); Serial.print(rx_msg.mb);
    Serial.print("  ID: 0x"); Serial.print(rx_msg.id, HEX );
    Serial.print("  EXT: "); Serial.print(rx_msg.flags.extended );
    Serial.print("  LEN: "); Serial.print(rx_msg.len);
    Serial.print(" DATA: ");
    for ( uint8_t i = 0; i < 8; i++ ) {
      Serial.print(rx_msg.buf[i]); Serial.print(" ");
    }
    Serial.print("  TS: "); Serial.println(rx_msg.timestamp);
  }
}
