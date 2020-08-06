#include <M5StickC.h>
#include "BugCControl.h"

uint8_t   BugCControl::speeds[4]  = { 0 };    // if speed > 0; clockwise rotation
uint32_t  BugCControl::lights[2]  = { 0 };



void BugCControl::set_speed(uint8_t pos, int8_t speed) {
  if(pos >= BUGC_NUM_MOTORS) return;
  speed = (speed > 100) ? 100 : ((speed < -100) ? -100 : speed);
  M5.I2C.writeByte(BUGC_ADDR, pos, speed);
  speeds[pos] = speed;
}


void BugCControl::set_all_speeds(int8_t speed_0, int8_t speed_1, int8_t speed_2, int8_t speed_3) {
  int8_t speed_out[4] = {speed_0, speed_1, speed_2, speed_3};
  for(uint8_t i = 0; i < 4; i++) {
    speeds[i] = (speed_out[i] > 100) ? 100 : ((speed_out[i] < -100) ? -100 : speed_out[i]);
  }
  M5.I2C.writeBytes(BUGC_ADDR, 0x00, (uint8_t*)speeds, 4);
}


void BugCControl::set_lights(uint32_t color_left, uint32_t color_right) {
  uint8_t color_out[4];
  color_out[0] = 0;
  color_out[1] = (color_left & 0xff0000) >> 16;
  color_out[2] = (color_left & 0x00ff00) >> 8;
  color_out[3] = (color_left & 0x0000ff);
  M5.I2C.writeBytes(BUGC_ADDR, 0x10,  color_out, 4);
  lights[BUGC_LEFT_LIGHT] = color_left;

  color_out[0] = 1;
  color_out[1] = (color_right & 0xff0000) >> 16;
  color_out[2] = (color_right & 0x00ff00) >> 8;
  color_out[3] = (color_right & 0x0000ff);
  M5.I2C.writeBytes(BUGC_ADDR, 0x10,  color_out, 4);
  lights[BUGC_RIGHT_LIGHT] = color_right;
}


int8_t BugCControl::get_speed(uint8_t pos) {
  if(pos >= BUGC_NUM_MOTORS) return 0;
  return speeds[pos];
}


int32_t BugCControl::get_color(uint8_t pos) {
  if(pos >= BUGC_NUM_LIGHTS) return 0;
}


// Display the motor speed in the appropriate position
// Speed is drawn in blue if stopped, green if forward
// and red if backward. The speed is drawn closest to
// the motor it describes.
//
void BugCControl::display_speed(uint8_t motor, int8_t speed) {
  if(0 == speed) M5.Lcd.setTextColor(TFT_BLUE);
  else M5.Lcd.setTextColor(0 < speed ? TFT_GREEN : TFT_RED);
  switch(motor) {
    case 0: M5.Lcd.fillRect(0, 64, 50, 16, TFT_BLACK);
            M5.Lcd.drawCentreString(String(speed),  25, 64, 2);
            break;
    case 1: M5.Lcd.fillRect(0, 0, 50, 16, TFT_BLACK);
            M5.Lcd.drawCentreString(String(speed),  25,  0, 2);
            break;
    case 2: M5.Lcd.fillRect(110, 64, 50, 16, TFT_BLACK);
            M5.Lcd.drawCentreString(String(speed), 135, 64, 2);
            break;
    case 3: M5.Lcd.fillRect(110, 0, 50, 16, TFT_BLACK);
            M5.Lcd.drawCentreString(String(speed), 135,  0, 2);
            break;
  }
}


// Stop the robot, turn off motors, turn off lights, display full stop.
//
void BugCControl::come_to_halt() {
  set_lights(0, 0);             // Turn off the NeoPixels on the front of the BugC
  set_all_speeds(0, 0, 0, 0);   // Stop the motors
  digitalWrite(M5_LED, true);   // turn off the red LED
  display_speed(0, 0);          // Display the speed of all four motors
  display_speed(1, 0);
  display_speed(2, 0);
  display_speed(3, 0);
}
