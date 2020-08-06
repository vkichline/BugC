// BugNow Robot with an M5StickC using ESP-Now protocol
// Pairs with the companion project BugNowController
// By Van Kichline
// In the year of the plague
//
// Ver 2: Automatic discovery. Secrets file is eliminated.
// Controller:
// When turned on, select a channel: 1 - 14. Default is random.
// Add a broadcast peer and broadcast a discovery packet until ACK received.
// Remove broadcast peer.
// Add the responder as a peer.
// Go into controller mode.
// Receiver:
// When turned on, select a channel: 1 - 14. Choose the same one as the Controller.
// Add a broadcast peer and listen for discovery packet until detected. Send ACK.
// Remove broadcast peer.
// Go into receiver mode.


#include <WiFi.h>
#include "M5StickC.h"
#include "BugCControl.h"
#include "BugComm.h"

#define BG_COLOR    NAVY
#define FG_COLOR    LIGHTGREY

BugCControl         bug;
BugComm             bug_comm;
bool                comp_mode             = false;    // Competition mode: manually select a channel


// Display the mac address of the device, and if connected, of its paired device.
// Also show the channel in use.
// Red indicates no ESP-Now connection, Green indicates connection established.
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugNow", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  mac = String("R ") + mac;
  M5.Lcd.drawCentreString(mac, 80, 22, 2);
  String chan = "Chan " + String(bug_comm.get_channel());
  M5.Lcd.drawCentreString(chan, 80, 60, 2);
  if(bug_comm.is_connected()) {
    char  buffer[32];
    uint8_t*  pa = bug_comm.get_peer_address();
    sprintf(buffer, "C %02X %02X %02X %02X %02X %02X", pa[0], pa[1], pa[2], pa[3], pa[4], pa[5]);
    M5.Lcd.drawCentreString(buffer, 80, 40, 2);
  }
}


// If we're not in competition mode, simply return 1.
// Else, select and return the channel that we're going to use for communications. This enables racing, etc.
//
uint8_t select_comm_channel() {
  if(!comp_mode) return 1;
  uint8_t chan  = random(13) + 1;
  M5.Lcd.drawString("CHAN",    8,  4, 2);
  M5.Lcd.drawString("1 - 14",  8, 28, 1);
  M5.Lcd.drawString("A = +",   8, 46, 1);
  M5.Lcd.drawString("B = Set", 8, 64, 1);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(String(chan), 160, 2, 8);

  // Before transmitting, select a channel
  while(true) {
    M5.update();
    if(M5.BtnB.wasReleased()) break;  // EXIT THE LOOP BY PRESSING B
    if(M5.BtnA.wasReleased()) {
      chan++;
      if(14 < chan) {
        chan = 1;
        M5.Lcd.fillRect(60, 2, 100, 80, BG_COLOR);
      }
      M5.Lcd.drawString(String(chan), 160, 2, 8);
    }
  }
  M5.Lcd.setTextDatum(TL_DATUM);
  return chan;
}


// See if valid data has been received, and if so set all data outputs (2 NeoPixels and 4 speeds)
// Send a response indicating whether or not the data received was valid.
//
void handle_incoming_data() {
  if(bug_comm.is_data_ready()) {
    bug_comm.clear_data_ready();
    bug_comm.send_response(RESP_NOERR);                   // let controller know that we accept the data
    bug.set_lights(bug_comm.get_light_color(0), bug_comm.get_light_color(1));  // set the NeoPixels on the front of the BugC
    bug.set_all_speeds(bug_comm.get_motor_speed(0), bug_comm.get_motor_speed(1), bug_comm.get_motor_speed(2), bug_comm.get_motor_speed(3));
    digitalWrite(M5_LED, !bug_comm.get_button());         // Turn on the LED if button is True
    bug.display_speed(0, bug_comm.get_motor_speed(0));    // Display the speed of all four motors
    bug.display_speed(1, bug_comm.get_motor_speed(1));    // close to the motors themselves
    bug.display_speed(2, bug_comm.get_motor_speed(2));    // (because layout and connection are fixed.)
    bug.display_speed(3, bug_comm.get_motor_speed(3));    // This assumes setRotation(1)
  }
}


// Listen for a broadcast BugComm_Discovery and respond.
//
void pair_with_controller() {
  if(comp_mode) {
    M5.Lcd.fillScreen(BG_COLOR);
    M5.Lcd.drawCentreString("Waiting for Pairing", 80, 20, 2);
    M5.Lcd.drawCentreString("on channel " + String(bug_comm.get_channel()), 80, 40, 2);
  }
  else {
    M5.Lcd.fillScreen(TFT_BLACK);
    print_mac_address(TFT_RED);
  }
  while(!bug_comm.is_connected()) {
    bug_comm.process_discovery_response();
    delay(500);
  }
}


// Standard Arduino setup function, called once before start of program.
//
void setup() {
  if(digitalRead(BUTTON_A_PIN) == 0) {                // Test to see if we're in Competition Mode
    comp_mode = true;                                 // If so, user selects a channel
  }                                                   // By default, we use channel 1.
  M5.begin();                                         // Gets the M5StickC library initialized
  Wire.begin(0, 26, 400000);                          // Need Wire to communicate with BugC
  pinMode(M5_LED, OUTPUT);                            // Enables the internal LED as an output
  digitalWrite(M5_LED, true);                         // Turn LED off
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.fillScreen(BG_COLOR);
  M5.Axp.SetChargeCurrent(CURRENT_360MA);             // Needed for charging the 750 mAh battery on the BugC
  M5.Lcd.setRotation(1);
  bug_comm.begin(MODE_RECEIVER);                      // Establish the mode we run in
  bug_comm.initialize_esp_now(select_comm_channel()); // Get communications working
  pair_with_controller();                             // Determine who we'll be working with
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


// Standard Arduino loop function, called continuously after setup.
//
void loop() {
  M5.update();                                // So M5.BtnA.isPressed() works
  if(M5.BtnA.isPressed()) bug.come_to_halt(); // In case the transmitter dies, pressing the button turns everything off.
  handle_incoming_data();                     // Handle ESP-Now communications
}
