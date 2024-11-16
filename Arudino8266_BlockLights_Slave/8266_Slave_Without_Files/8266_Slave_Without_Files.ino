#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <espnow.h>
//#include <SPIFFS.h> // I need to use the <littleFS.h> library instead eventually

#define NUM_LEDS 1
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

int blockNumber = 2;

String slaveMacAddressString;

uint8_t slaveMacAddress[6];

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Slave's MAC address (for scan response)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;

//void writeBlockNumberToSPIFFS(int newBlockNumber) {
//    File file = SPIFFS.open("/blockNumber.txt", FILE_WRITE);
//    if (!file) {
//        Serial.println("Failed to open file for writing");
//        return;
//    }
//
//    // Write the new value to the file
//    file.print(newBlockNumber);
//    file.close();
//}

// Callback when ESP-NOW message is received
void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {

    Serial.print("Received data from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Optionally process the received data
    Message receivedMessage;
    memcpy(&receivedMessage, data, sizeof(receivedMessage));

    Message updateMessage;

    // switch case for the different types of message
    switch(receivedMessage.type) {
        case 0: // scan
            //return mac address and number
            esp_now_add_peer(receivedMessage.mac, ESP_NOW_ROLE_SLAVE, 11, NULL, 0);
            // send this block's MAC and number back
            
            updateMessage.number = blockNumber;
            for(int i=0; i < 6; i++) {
                updateMessage.mac[i] = slaveMacAddress[i];
            }
            
            esp_now_send(receivedMessage.mac, (uint8_t *)&updateMessage, sizeof(updateMessage));

            break;
        case 1: // LED colour change
            leds[0] = receivedMessage.colour;
            FastLED.show();
            Serial.println("Updated LED colour");
            break;
        case 2: // block number update
            blockNumber = receivedMessage.number;
            //writeBlockNumberToSPIFFS(blockNumber);
            Serial.println("Updated block number");
            break;
        default:
            Serial.println("Wrong message type");
            // send error code back?
    }
      
}


void setupWiFi() {
    // initialise the ESP-NOW protocol
    WiFi.mode(WIFI_STA);  // Use station mode for web server and ESP-NOW

    // Set the Wi-Fi channel
    //esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);
    //WiFi.channel(11);
    wifi_set_channel(11);


    if (esp_now_init() != ERR_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_recv_cb(OnDataRecv);

    WiFi.setSleep(false); // prevent the slave blocks from going into deepsleep


}

//void readBlockNumberFromSPIFFS() {
//    File file = SPIFFS.open("/blockNumber.txt", FILE_READ);
//    if (!file) {
//        Serial.println("Failed to open file for reading, initializing value to 0");
//        return;
//    }
//
//    // Read the value from file
//    String blockNumberStr = file.readString();
//    blockNumber = blockNumberStr.toInt();
//    file.close();
//}



void setup() {
     //esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error  WARNING setting all logs to just error BREAKS FASTLED (3.8 prerelease)  

    

    Serial.begin(115200); // Initialize Serial communication
    //delay(5000); // 5 seconds to let you open the COM port if you need

    // Initialize SPIFFS
//    if (!SPIFFS.begin(true)) {
//        Serial.println("SPIFFS initialization failed!");
//        return;
//    }

    //readBlockNumberFromSPIFFS();

    // Setup Wi-Fi and LED control
    printf("\nInitialising......\n");
    setupWiFi();

    WiFi.macAddress(slaveMacAddress);
    slaveMacAddressString = WiFi.macAddress();
    Serial.println(slaveMacAddressString);

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED

}

void loop() {
  // loop through RGB for testing
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(1000);
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(1000);
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(1000);
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(1000);  
}
