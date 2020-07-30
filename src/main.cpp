// BugC Robot control with an M5StickC using ESP-Now protocol
// Pairs with the companion project BugController
// By Van Kichline
// In the year of the plague


#include <esp_now.h>
#include <WiFi.h>
#include "M5StickC.h"
#include "bugC.h"
#include "BugCommunications.h"
// M5StickCMacAddresses.h includes some defines, including a line like:
// #define M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER   {0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN}
// Replace the N's with the digits of your M5StickC's MAC Address (displayed on screen)
// NEVER check your secrets into a source control manager!
#include "../../Secrets/M5StickCMacAddresses.h"

#define BATTERY_UPDATE_DELAY  30000
#define LOW_BATTERY_VOLTAGE   3.7

struct_message  datagram;
struct_response response;
uint8_t         conrollerAddress[]  = M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER;    // The MAC address displayed by BugController
bool            data_received       = false;
bool            data_valid          = false;
bool            connected           = false;
unsigned long   lastBatteryUpdate   = -30000;


// Display the mac address on the screen in a diagnostic color
// Red indicates no ESP-Now connection, Green indicates connection established.
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugC", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  M5.Lcd.drawCentreString(mac, 80, 30, 2);
}


// Callback executed by ESP-Now when data is sent
//
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// Display the motor speed in the appropriate position
// Speed is drawn in blue if stopped, green if forward
// and red if backward. The speed is drawn closest to
// the motor it describes.
//
void display_speed(uint8_t motor, int8_t speed) {
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


// Callback function that will be executed by ESP-Now when data is received.
// Move the data into datagram storage and set data_received.
//
void on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
  data_valid = (sizeof(struct_message) == len);   // currently, validated by message size.
  if(data_valid) {
    data_received = true;
    if(!connected) {
      connected = true;
      print_mac_address(TFT_GREEN); // change the displayed color from red to green to indicated connection
    }
    memcpy(&datagram, incomingData, sizeof(struct_message));
    Serial.println("Received Packet:");
    Serial.printf("speed_0: %d\n",      datagram.speed_0);
    Serial.printf("speed_1: %d\n",      datagram.speed_0);
    Serial.printf("speed_2: %d\n",      datagram.speed_0);
    Serial.printf("speed_3: %d\n",      datagram.speed_0);
    Serial.printf("color_left: %d\n",   datagram.color_left);
    Serial.printf("color_right: %d\n",  datagram.color_right);
  }
}


// Send a status response back to the BugController to let it know how the last message was handled.
//
void send_response(response_status status) {
  response.status = status;
  esp_err_t result = esp_now_send(conrollerAddress, (uint8_t *) &response, sizeof(struct_response));
  Serial.printf("send_response result = %d\n", result);
}


// Set up ESP-Now. Return true if successful.
//
bool initialize_esp_now() {
  esp_now_peer_info_t peerInfo;
  WiFi.mode(WIFI_STA);
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  esp_now_register_send_cb(on_data_sent);
  esp_now_register_recv_cb(on_data_received);

  memcpy(peerInfo.peer_addr, conrollerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
    if (ESP_OK != esp_now_add_peer(&peerInfo)) {
    Serial.println("Failed to add peer");
    return false;
  }
  return true;
}


// See if valid data has been received, and if so set all data outputs (2 NeoPixels and 4 speeds)
// Send a response indicating whether or not the data received was valid.
//
bool test_and_handle_incoming_data() {
  if(data_received) {
    data_received = false;                                      // clear the flag for next command
    if(data_valid) {
      send_response(RESP_NOERR);                                // let controller know that we accept the data
      BugCSetColor(datagram.color_left, datagram.color_right);  // set the NeoPixels on the front of the BugC
      BugCSetAllSpeed(datagram.speed_0, datagram.speed_1, datagram.speed_2, datagram.speed_3);
      display_speed(0, datagram.speed_0);                       // Display the speed of all four motors
      display_speed(1, datagram.speed_1);                       // close to the motors themselves
      display_speed(2, datagram.speed_2);                       // (because layout and connection are fixed.)
      display_speed(3, datagram.speed_3);                       // This assumes setRotation(1)
      Serial.printf("color_left = %d\tcolor_right = %d\n", datagram.color_left, datagram.color_right);
      Serial.printf("speed_0    = %d\tspeed_1     = %d\n", datagram.speed_0, datagram.speed_1);
      Serial.printf("speed_2    = %d\tspeed_3     = %d\n", datagram.speed_2, datagram.speed_3);
      return true;
    }
    else {
      send_response(RESP_ERROR);                                // let controller know that we reject the data
      return false;
    }
  }
  return false;
}


// See if enough time has elapsed to update the battery voltage display.
// If the voltage is OK, display in green. If dangerously low, display in red.
//
void test_and_display_battery_voltage() {
  if(lastBatteryUpdate + BATTERY_UPDATE_DELAY < millis()) {
    float vBat = M5.Axp.GetBatVoltage();
    vBat = int(vBat * 10.0) / 10.0;
    M5.Lcd.setTextColor(LOW_BATTERY_VOLTAGE < vBat ? TFT_GREEN : TFT_RED);
    M5.Lcd.fillRect(65, 64, 50, 16, TFT_BLACK);
    M5.Lcd.drawCentreString(String(vBat), 80,  64, 2);
    lastBatteryUpdate = millis();
  }
}


// Standard Arduino setup function, called once before start of program.
//
void setup() {
  M5.begin();                             // Gets the M5StickC library initialized
  Wire.begin(0, 26, 400000);              // Need Wire to communicate with BugC
  M5.Axp.SetChargeCurrent(CURRENT_360MA); // Needed for charging the 750 mAh battery on the BugC
  M5.Lcd.setRotation(1);
  print_mac_address(TFT_RED);             // Display address for convenience. Red indicates no connection yet.
  initialize_esp_now();                   // Get communications working
}


// Standard Arduino loop function, called continuously after setup.
//
void loop() {
  test_and_handle_incoming_data();
  test_and_display_battery_voltage();
}
