#include <stdio.h>
#include <WiFi.h>
#include <FastLED.h>
#include <esp_now.h>
#include <SPIFFS.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

#define NUM_LEDS 1
#define DATA_PIN 8

CRGB leds[NUM_LEDS];

int blockNumber;

String slaveMacAddressString;

uint8_t slaveMacAddress[6];

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Slave's MAC address (for scan response)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;


// Task function to continuously run
void espNowTask(void *pvParameters) {
    while (true) {
        // You can add any periodic non-blocking code here if needed
        vTaskDelay(pdMS_TO_TICKS(1000)); // Optional delay to prevent busy waiting
    }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void addPeer(uint8_t* peerMAC) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, peerMAC, 6);  // Use the specific MAC address
    peerInfo.channel = 0;  // Same channel as Wi-Fi
    peerInfo.encrypt = false;  // No encryption

    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("Peer added successfully");
    } else {
        Serial.println("Failed to add peer");
    }
}

void writeBlockNumberToSPIFFS(int newBlockNumber) {
    File file = SPIFFS.open("/blockNumber.txt", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Write the new value to the file
    file.print(newBlockNumber);
    file.close();
}

// Callback when ESP-NOW message is received
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    const uint8_t *mac_addr = esp_now_info->src_addr;

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
    esp_err_t result;

    // switch case for the different types of message
    switch(receivedMessage.type) {
        case 0: // scan
            //return mac address and number
            addPeer(receivedMessage.mac);
            // send this block's MAC and number back
            
            updateMessage.number = blockNumber;
            for(int i=0; i < 6; i++) {
                updateMessage.mac[i] = slaveMacAddress[i];
            }
            
            result = esp_now_send(receivedMessage.mac, (uint8_t *)&updateMessage, sizeof(updateMessage));
    
            if (result == ESP_OK) {
                Serial.println("Scan reply message sent successfully");
            } else {
                Serial.println("Error sending scan reply message");
            }

            break;
        case 1: // LED colour change
            leds[0] = receivedMessage.colour;
            FastLED.show();
            Serial.println("Updated LED colour");
            break;
        case 2: // block number update
            blockNumber = receivedMessage.number;
            writeBlockNumberToSPIFFS(blockNumber);
            Serial.println("Updated block number");
            break;
        default:
            Serial.println("Wrong message type");
            // send error code back?
    }
      
}


void setupWiFi() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);  // Ensure NVS is initialized successfully

    // Wait a moment for the AP to configure itself
    delay(1000);
    
    // initialise the ESP-NOW protocol
    WiFi.mode(WIFI_STA);  // Use AP + station mode for web server and ESP-NOW

    // Set the Wi-Fi channel
    esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);
    //WiFi.channel(11);


    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    WiFi.setSleep(false); // prevent the slave blocks from going into deepsleep


}

void readBlockNumberFromSPIFFS() {
    File file = SPIFFS.open("/blockNumber.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading, initializing value to 0");
        return;
    }

    // Read the value from file
    String blockNumberStr = file.readString();
    blockNumber = blockNumberStr.toInt();
    file.close();
}


extern "C" void app_main(void) {
    //esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error  WARNING setting all logs to just error BREAKS FASTLED (3.8 prerelease)  

    

    Serial.begin(115200); // Initialize Serial communication
    //delay(5000); // 5 seconds to let you open the COM port if you need

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        return;
    }

    readBlockNumberFromSPIFFS();

    // Setup Wi-Fi and LED control
    printf("\nInitialising......\n");
    setupWiFi();

    WiFi.macAddress(slaveMacAddress);
    slaveMacAddressString = WiFi.macAddress();
    Serial.println(slaveMacAddressString);

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED
    
    // Create a FreeRTOS task for handling ESP-NOW communication
    xTaskCreate(espNowTask, "ESP-NOW Task", 2048, NULL, 1, NULL);
}


