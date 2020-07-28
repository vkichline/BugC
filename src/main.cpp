#include <esp_now.h>
#include <WiFi.h>
#include "M5StickC.h"
#include "bugC.h"
#include "BugCommunications.h"
// M5StickCMacAddresses.h includes some defines, including a line like:
// #define of M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER   {0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN}
// Replace the N's with the digits of your M5StickC's MAC Address (displayed on screen)
// NEVER check your secrets into a source control manager!
#include "../../Secrets/M5StickCMacAddresses.h"


struct_message  datagram;
struct_response response;
uint8_t         conrollerAddress[]  = M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER;    // Specific to controller M5StickC
bool            data_received       = false;
bool            data_valid          = false;
bool            connected           = false;
unsigned long   lastBatteryUpdate   = -30000;


// display the mac address on the screen in a diagnostic color
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugC", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  M5.Lcd.drawCentreString(mac, 80, 30, 2);
}


// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// show the motor speed in the appropriate position
void DisplaySpeed(uint8_t motor, int8_t speed) {
  M5.Lcd.setTextColor(0 < speed ? TFT_GREEN : TFT_RED);
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


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  data_valid = (sizeof(struct_message) == len);
  if(data_valid) {
    data_received = true;
    if(!connected) {
      connected = true;
      print_mac_address(TFT_GREEN);
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


void SendResponse(response_status status) {
  response.status = status;
  esp_err_t result = esp_now_send(conrollerAddress, (uint8_t *) &response, sizeof(struct_response));
  Serial.printf("SendResponse result = %d\n", result);
}


void setup() {
  M5.begin();
  Wire.begin(0, 26, 400000);
  M5.Lcd.setRotation(1);
  // if add battery, need increase charge current
  M5.Axp.SetChargeCurrent(CURRENT_360MA);
  print_mac_address(TFT_RED);

  esp_now_peer_info_t peerInfo;
  WiFi.mode(WIFI_STA);
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  memcpy(peerInfo.peer_addr, conrollerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
    if (ESP_OK != esp_now_add_peer(&peerInfo)) {
    Serial.println("Failed to add peer");
    return;
  }
}


void loop() {
  if(data_received) {
    data_received = false;
    SendResponse(data_valid ? RESP_NOERR : RESP_ERROR);
    BugCSetColor(datagram.color_left, datagram.color_right);
    BugCSetAllSpeed(datagram.speed_0, datagram.speed_1, datagram.speed_2, datagram.speed_3);
    DisplaySpeed(0, datagram.speed_0);
    DisplaySpeed(1, datagram.speed_1);
    DisplaySpeed(2, datagram.speed_2);
    DisplaySpeed(3, datagram.speed_3);
    Serial.printf("color_left = %d\tcolor_right = %d\n", datagram.color_left, datagram.color_right);
    Serial.printf("speed_0    = %d\tspeed_1     = %d\n", datagram.speed_0, datagram.speed_1);
    Serial.printf("speed_2    = %d\tspeed_3     = %d\n", datagram.speed_2, datagram.speed_3);
  }
  if(lastBatteryUpdate + 30000 < millis()) {
    float vBat = M5.Axp.GetBatVoltage();
    vBat = int(vBat * 10.0) / 10.0;
    M5.Lcd.setTextColor(3.8 < vBat ? TFT_GREEN : TFT_RED);
    M5.Lcd.fillRect(65, 64, 50, 16, TFT_BLACK);
    M5.Lcd.drawCentreString(String(vBat), 80,  64, 2);
    lastBatteryUpdate = millis();
  }
}