#include <WiFi.h>
#include "BugComm.h"


bool                BugComm::data_ready           = false;
bool                BugComm::data_valid           = false;
bool                BugComm::connected            = false;
uint8_t             BugComm::channel              = 0;
uint8_t             BugComm::response_len         = 0;
uint8_t             BugComm::responseAddress[6]   = { 0 };
uint8_t             BugComm::broadcastAddress[]   = BROADCAST_MAC_ADDRESS;
uint8_t             BugComm::peerAddress[6]       = { 0 };
esp_now_peer_info_t BugComm::peerInfo;
BugComm_Command     BugComm::datagram;
BugComm_Response    BugComm::response;
BugComm_Discovery   BugComm::discovery;


bool BugComm::initialize_esp_now(uint8_t chan, uint8_t* mac_address) {
  channel = chan;
  WiFi.disconnect();
  WiFi.softAP(AP_NAME, "", channel, 1);   // Create a hidden AP on given channel
  WiFi.mode(WIFI_STA);                    // ...and switch to station mode
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


// Static function: ESP-Now callback function that will be executed when data is sent
//
void BugComm::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// Static function: ESP-Now callback function that will be executed when data is received
// This is on a high-priority system thread. Do as little as possible.
//
void BugComm::on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("Incoming packet received.");
  if(connected) {
    memcpy(&datagram, incomingData, sizeof(BugComm_Command));
    data_valid = (sizeof(BugComm_Command) == len);  // In discovery phase until connected.
    if(data_valid) data_valid = BUGCOMM_SIGNATURE == datagram.signature &&
                                BUGCOMM_VERSION   == datagram.version;
  }
  else {
    memcpy(&response, incomingData, sizeof(BugComm_Response));
    data_valid = (sizeof(BugComm_Response) == len);  // In discovery phase until connected.
    if(data_valid) data_valid = BUGCOMM_SIGNATURE == response.signature &&
                                BUGCOMM_VERSION   == response.version;
  }
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = true;
}


// When waiting for paring, process incoming discovery packet. If valid, remove broadcast peer
// and reinitialize with new peer.
//
void BugComm::process_pairing_response() {
  data_ready = false;
  data_valid = (sizeof(BugComm_Discovery) == response_len);  // In discovery phase until connected.
  if(data_valid) data_valid = BUGCOMM_SIGNATURE == response.signature &&
                              BUGCOMM_VERSION   == response.version;
  if(data_valid) {
    Serial.println("Incoming discovery packet validated.");
    connected = true;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &response, sizeof(BugComm_Response));
    Serial.printf("pairing send_response result = %d\n", result);
    memcpy(peerAddress, responseAddress, 6);    // This is who we will be talking to.
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
    if(0 == response_len) return;
    Serial.print("COMM FAILURE: Incoming packet rejected. ");
    if(sizeof(BugComm_Response) != response_len) Serial.printf("Expected size: %d. Actual size: %d\n", sizeof(BugComm_Response), response_len);
    else if(BUGCOMM_SIGNATURE != response.signature) Serial.printf("Expected signature: %lu. Actual signature: %lu\n", BUGCOMM_SIGNATURE, response.signature);
    else if(BUGCOMM_VERSION   != response.version)   Serial.printf("Expected version: %d. Actual version: %d\n", BUGCOMM_VERSION, response.version);
    else Serial.println("Coding error.\n");
  }
}


// Send a status response back to the BugController to let it know how the last message was handled.
//
void BugComm::send_response(BugComm_Status status) {
  response.status = status;
  esp_err_t result = esp_now_send(peerAddress, (uint8_t *) &response, sizeof(BugComm_Response));
  Serial.printf("send_response result = %d\n", result);
}


// Assumes (but does not test because you may have cleared it) that datagram data is valid.
//
uint32_t BugComm::get_light_color(uint8_t pos) {
  if(pos > 2) return 0;
  return (pos == 0) ? datagram.color_left : datagram.color_right;
}


// Assumes (but does not test because you may have cleared it) that datagram data is valid.
//
uint8_t BugComm::get_motor_speed(uint8_t pos) {
  switch(pos) {
    case 0:   return datagram.speed_0;
    case 1:   return datagram.speed_1;
    case 2:   return datagram.speed_2;
    case 3:   return datagram.speed_3;
    default:  return 0;
  }
}


// Assumes (but does not test because you may have cleared it) that datagram data is valid.
//
uint8_t BugComm::get_button() {
  return datagram.button;
}
