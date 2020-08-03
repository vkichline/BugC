// BugC Robot control with an M5StickC using ESP-Now protocol
// Pairs with the companion project BugController
// By Van Kichline
// In the year of the plague


#include <esp_now.h>
#include <WiFi.h>
#include "M5StickC.h"
#include "bugC.h"
#include "BugCommunications.h"

// The motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC

// Ver 2: Automatic discovery. Secrets file is eliminated.
// Controller:
// When turned on, select a channel: 1 - 14. Default is random.
// Set a special callback function to handle the pairing
// Add a broadcast peer and broadcast a controller frame until ACK received.
// Change the callback routine to operational callback
// Remove broadcast peer.
// Add the responder as a peer.
// Go into controller mode.
// Obeyer:
// When turned on, select a channel: 1 - 14. Choose the same one as the Controller.
// Add a broadcast peer and listen for controller frame until detected. Send ACK.
// Remove broadcast peer.
// Go into obeyer mode.

#define BATTERY_UPDATE_DELAY              30000
#define LOW_BATTERY_VOLTAGE               3.7
#define LED_PIN                           10
#define BG_COLOR                          NAVY
#define FG_COLOR                          LIGHTGREY
#define AP_NAME                           "BugNowAP"

struct_message      datagram;
struct_response     response;
discovery_message   discovery;
esp_now_peer_info_t peerInfo;
bool                data_ready            = false;
uint8_t             response_len          = 0;
uint8_t             channel               = 0;
uint8_t             responseAddress[6]    = { 0 };
uint8_t             broadcastAddress[]    = BROADCAST_MAC_ADDRESS;
uint8_t             controllerAddress[6]  = { 0 };
bool                data_received         = false;
bool                data_valid            = false;
bool                connected             = false;
unsigned long       lastBatteryUpdate     = -30000;


// Display the mac address on the screen in a diagnostic color
// Red indicates no ESP-Now connection, Green indicates connection established.
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugC", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  String chan = "Channel " + String(channel);
  M5.Lcd.drawCentreString(mac, 80, 22, 2);
  if(connected) M5.Lcd.drawCentreString(chan, 80, 44, 2);
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
// This is on a high-priority system thread. Do as little as possible.
//
void on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("Incoming packet received.");
  if(connected) {
    memcpy(&datagram, incomingData, sizeof(struct_message));
  }
  else {
    memcpy(&response, incomingData, sizeof(struct_response));
  }
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = true;
}


// Send a status response back to the BugController to let it know how the last message was handled.
//
void send_response(response_status status) {
  response.status = status;
  esp_err_t result = esp_now_send(controllerAddress, (uint8_t *) &response, sizeof(struct_response));
  Serial.printf("send_response result = %d\n", result);
}


// Set up ESP-Now. Return true if successful.
//
bool initialize_esp_now(uint8_t chan, uint8_t* mac_address) {
  channel = chan;
  WiFi.disconnect();
  WiFi.softAP(AP_NAME, "", channel);
  WiFi.mode(WIFI_STA);
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  esp_now_register_send_cb(on_data_sent);
  esp_now_register_recv_cb(on_data_received);

  memcpy(peerInfo.peer_addr, mac_address, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  if (ESP_OK == esp_now_add_peer(&peerInfo)) {
    Serial.println("Added peer");
  }
  else {
    Serial.println("Failed to add peer");
    return false;
  }
  return true;
}


// See if valid data has been received, and if so set all data outputs (2 NeoPixels and 4 speeds)
// Send a response indicating whether or not the data received was valid.
//
void test_and_handle_incoming_data() {
  data_ready = false;
  data_valid = (sizeof(struct_message) == response_len);
  if(data_valid) data_valid = COMMUNICATIONS_SIGNATURE == response.signature &&
                              COMMUNICATIONS_VERSION   == response.version;
  if(data_valid) {
    send_response(RESP_NOERR);                                // let controller know that we accept the data
    BugCSetColor(datagram.color_left, datagram.color_right);  // set the NeoPixels on the front of the BugC
    BugCSetAllSpeed(datagram.speed_0, datagram.speed_1, datagram.speed_2, datagram.speed_3);
    digitalWrite(LED_PIN, !datagram.button);                  // Turn on the LED if button is True
    display_speed(0, datagram.speed_0);                       // Display the speed of all four motors
    display_speed(1, datagram.speed_1);                       // close to the motors themselves
    display_speed(2, datagram.speed_2);                       // (because layout and connection are fixed.)
    display_speed(3, datagram.speed_3);                       // This assumes setRotation(1)
    Serial.println("Incoming control packet validated.");
    Serial.printf("speed_0: %d\n",      datagram.speed_0);
    Serial.printf("speed_1: %d\n",      datagram.speed_0);
    Serial.printf("speed_2: %d\n",      datagram.speed_0);
    Serial.printf("speed_3: %d\n",      datagram.speed_0);
    Serial.printf("color_left: %d\n",   datagram.color_left);
    Serial.printf("color_right: %d\n",  datagram.color_right);
    Serial.printf("button     = %s\n", datagram.button ? "true" : "false");
  }
  else {
    Serial.print("COMM FAILURE: Incoming packet rejected. ");
    if(sizeof(struct_message) != response_len) Serial.printf("Expected size: %d. Actual size: %d\n", sizeof(struct_message), response_len);
    else if(COMMUNICATIONS_SIGNATURE != response.signature) Serial.printf("Expected signature: %lu. Actual signature: %lu\n", COMMUNICATIONS_SIGNATURE, response.signature);
    else if(COMMUNICATIONS_VERSION   != response.version)   Serial.printf("Expected version: %d. Actual version: %d\n", COMMUNICATIONS_VERSION, response.version);
    else Serial.println("Coding error.\n");
  }
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


// Stop the robot, turn off motors, turn off lights, display full stop.
//
void come_to_halt() {
  BugCSetColor(0, 0);           // Turn off the NeoPixels on the front of the BugC
  BugCSetAllSpeed(0, 0, 0, 0);  // Stop the motors
  digitalWrite(LED_PIN, true);  // turn off the red LED
  display_speed(0, 0);          // Display the speed of all four motors
  display_speed(1, 0);
  display_speed(2, 0);
  display_speed(3, 0);
}


void process_pairing_response() {
  data_ready = false;
  data_valid = (sizeof(discovery_message) == response_len);  // In discovery phase until connected.
  if(data_valid) data_valid = COMMUNICATIONS_SIGNATURE == response.signature &&
                              COMMUNICATIONS_VERSION   == response.version;
  if(data_valid) {
    Serial.println("Incoming pairing packet validated.");
    esp_err_t           result;
    connected = true;
    result = esp_now_send(broadcastAddress, (uint8_t *) &response, sizeof(struct_response));
    Serial.printf("pairing send_response result = %d\n", result);
    memcpy(controllerAddress, responseAddress, 6);    // This is who we will be talking to.
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x, channel %d\n", responseAddress[0], responseAddress[1], responseAddress[2], responseAddress[3], responseAddress[4], responseAddress[5], channel);
    if (ESP_OK == esp_now_del_peer(broadcastAddress)) {   // We are finished with discovery
      Serial.println("Deleted broadcast peer");
    }
    else {
      Serial.println("Failed to delete broadcast peer");
      return;
    }
    // Reinitialize WiFi with the new channel, which must match ESP-Now channel
    initialize_esp_now(channel, responseAddress);
    return;
  }
  else {
    Serial.print("COMM FAILURE: Incoming packet rejected. ");
    if(sizeof(struct_response) != response_len) Serial.printf("Expected size: %d. Actual size: %d\n", sizeof(struct_response), response_len);
    else if(COMMUNICATIONS_SIGNATURE != response.signature) Serial.printf("Expected signature: %lu. Actual signature: %lu\n", COMMUNICATIONS_SIGNATURE, response.signature);
    else if(COMMUNICATIONS_VERSION   != response.version)   Serial.printf("Expected version: %d. Actual version: %d\n", COMMUNICATIONS_VERSION, response.version);
    else Serial.println("Coding error.\n");
  }
}


// Select and return the channel that we're going to use for communications. This enables racing, etc.
//
uint8_t select_comm_channel() {
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


// Listen for a broadcast discovery_message and respond.
//
void pair_with_controller() {
  M5.Lcd.fillScreen(BG_COLOR);
  M5.Lcd.drawCentreString("Waiting for Pairing", 80,  8, 2);
  M5.Lcd.drawCentreString("on channel " + String(channel), 80,  32, 2);
  while(!connected) {
    process_pairing_response();
    delay(500);
  }
}


// Standard Arduino setup function, called once before start of program.
//
void setup() {
  M5.begin();                                     // Gets the M5StickC library initialized
  Wire.begin(0, 26, 400000);                      // Need Wire to communicate with BugC
  pinMode(LED_PIN, OUTPUT);                       // Enables the internal LED as an output
  digitalWrite(LED_PIN, true);                    // Turn LED off
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.fillScreen(BG_COLOR);
  M5.Axp.SetChargeCurrent(CURRENT_360MA);         // Needed for charging the 750 mAh battery on the BugC
  M5.Lcd.setRotation(1);
  channel = select_comm_channel();                // Let user identify what channel we'll be using
  initialize_esp_now(channel, broadcastAddress);  // Get communications working
  pair_with_controller();                         // Determine who we'll be working with
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


// Standard Arduino loop function, called continuously after setup.
//
void loop() {
  M5.update();                            // So M5.BtnA.isPressed() works
  if(M5.BtnA.isPressed()) come_to_halt(); // In case the transmitter dies, pressing the button turns everything off.
  test_and_handle_incoming_data();        // Handle ESP-Now communications
  test_and_display_battery_voltage();     // Display battery voltage on screen
}
