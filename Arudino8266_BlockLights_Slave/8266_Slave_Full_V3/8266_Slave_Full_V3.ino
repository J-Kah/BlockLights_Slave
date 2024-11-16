#include <FastLED.h>
#include <espnow.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>

#define NUM_LEDS 3
#define DATA_PIN 2
#define CHANNEL 1

CRGB leds[NUM_LEDS];

int blockNumber = 2;
uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t slaveMacAddress[6];

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Slave's MAC address (for scan response)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;

void writeBlockNumberToLittleFS(int newBlockNumber) {
  
    // Write to the file (overwriting previous content)
    FILE *file = fopen("/blockNumber.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Write the new value to the file
    fprintf(file, "%d", newBlockNumber);
    fclose(file);
    Serial.println("New block number written to LittleFS");
}

void readBlockNumberFromLittleFS() {
  
    FILE *file = fopen("/blockNumber.txt", "r");
    if (!file) {
        Serial.println("Failed to open file for reading, initializing value to 2");
        blockNumber = 2; // Initialize to a default value if file is not found
        return;
    }

    // Read a line from the file (enough for two digits and a null terminator)
    char buffer[3];  // Buffer to hold the number (2 digits + null terminator)
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        Serial.println("Error reading from file or file is empty.");
        fclose(file);
        return;
    }
    fclose(file);

    // Convert the string to an integer
    blockNumber = atoi(buffer);  // Convert from string to int

    Serial.printf("Block number read from LittleFS: %d\n", blockNumber);
}


void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Reply sent successfully");
  }
  else{
    Serial.println("Reply NOT sent successfully");
  }
}


// Callback when ESP-NOW message is received
void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {
    printf("Received data\n");
    // Optionally process the received data
    Message receivedMessage;
    memcpy(&receivedMessage, data, sizeof(receivedMessage));

    Message updateMessage;
    String colour;

    // switch case for the different types of message
    switch(receivedMessage.type) {
        case 0: // scan
            // return mac address and number            
            if(esp_now_add_peer(receivedMessage.mac, ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0) == 0) {
              printf("Peer added successfully\n");
            }else {
              printf("Peer NOT added successfully\n");
            }
            // send this block's MAC and number back
            
            updateMessage.number = blockNumber;
            updateMessage.type = 3; //make sure slaves ignore this message if using broadcast MAC
            for(int i=0; i < 6; i++) {
                updateMessage.mac[i] = slaveMacAddress[i];
            }


          
            // TEST: Change to just the normal broadcast MAC address for espnow
            if(esp_now_send(broadcastMAC, (uint8_t *)&updateMessage, sizeof(updateMessage)) == 0) {;
              printf("Reply ready to be sent via broadcastMAC\n");
            }else {
              printf("Reply NOT ready to be sent via broadcastMAC\n");
            }
            //
            
            delay(1000);
            
            if(esp_now_send(receivedMessage.mac, (uint8_t *)&updateMessage, sizeof(updateMessage)) == 0) {;
              printf("Reply sent via received MAC successfully\n");
            }else {
              printf("Reply NOT sent via received MAC successfully\n");
            }
            
   
            break;
        case 1: // LED colour change
            fill_solid(leds, NUM_LEDS, receivedMessage.colour);
            FastLED.show();
            if(receivedMessage.colour == CRGB::Black) {
                colour = "black";
            } else if(receivedMessage.colour == CRGB::Green) {
                colour = "green";
            } else if(receivedMessage.colour == CRGB::Red) {
                colour = "red";
            } else if(receivedMessage.colour == CRGB::Blue) {
                colour = "blue";
            } else if(receivedMessage.colour == CRGB::Orange) {
                colour = "orange";
            }
            Serial.printf("Received data from Master, updated LED colour to %s\n", colour.c_str());
            break;
        case 2: // block number update
            blockNumber = receivedMessage.number;
            writeBlockNumberToLittleFS(blockNumber);
            Serial.printf("Received data from Master, updated block number to %d\n", blockNumber);
            break;
        default:
            Serial.println("Received data from Master, wrong message type");
            // send error code back?
    }
}

void setupWiFi() {
    // initialise the ESP-NOW protocol
    WiFi.mode(WIFI_STA);  // Use station mode for ESP-NOW

    // Set the Wi-Fi channel
    wifi_set_channel(CHANNEL);

    if (esp_now_init() != ERR_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    
    // register the function that handles incoming ESP-NOW messages
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    WiFi.setSleep(false); // prevent the slave blocks from going into deepsleep
}

void setup() {
    Serial.begin(115200); // Initialize Serial communication
    //delay(5000); // 5 seconds to let you open the COM port if you need

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS initialization failed!");
        return;
    }

    // get the initial / previous saved block number
    readBlockNumberFromLittleFS();

    // Setup Wi-Fi and LED control
    printf("\nInitialising WiFi...\n");
    setupWiFi();

    WiFi.macAddress(slaveMacAddress);

    FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // Initialize FastLED

    // Make sure the leds are off after power on
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    printf("Setup complete, listening for ESP NOW messages...\n");

}

void loop() {

}
