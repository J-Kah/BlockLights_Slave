/*
 * BlockLights
 * Author: Joshua Kah
 * Contact: joshk995@gmail.com
 * Year: 2024
 *
 * This code is written by Joshua Kah and is provided under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 * 
 * You are free to:
 *  - Share: Copy and redistribute the material in any medium or format
 *  - Adapt: Remix, transform, and build upon the material, provided it is for non-commercial purposes.
 * 
 * Under the following terms:
 *  - Attribution: You must give appropriate credit, provide a link to the license, and indicate if changes were made.
 *  - NonCommercial: You may not use the material for commercial purposes.
 *  - ShareAlike: If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
 * 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
 * 
 * This code is provided "as-is" without any warranties or guarantees.
 */

#include <FastLED.h>
#include <esp_now.h>
#include <esp_littlefs.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#define NUM_LEDS 3
#define DATA_PIN 8
#define CHANNEL 1

static const char *TAG = "BlockLights Slave";

CRGB leds[NUM_LEDS];

int blockNumber;

uint8_t slaveMacAddress[6];

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Slave's MAC address (for scan response)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Reply Success");
    } else {
        ESP_LOGE(TAG, "Reply Fail");
    }
}

void addPeer(uint8_t* peerMAC) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, peerMAC, 6);  // Use the specific MAC address

    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        ESP_LOGI(TAG, "Peer added successfully");
    } else {
        ESP_LOGI(TAG, "Failed to add peer, Already added?");
    }
}

void writeBlockNumberToLittleFS(int newBlockNumber) {
    // Write to the file (overwriting previous content)
    FILE *file = fopen("/partition/blockNumber.txt", "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the new value to the file
    fprintf(file, "%d", newBlockNumber);
    fclose(file);
    ESP_LOGI(TAG, "New block number written to LittleFS");
}

void readBlockNumberFromLittleFS() {
    FILE *file = fopen("/partition/blockNumber.txt", "r");
    if (!file) {
        ESP_LOGI(TAG, "Failed to open file for reading, initializing value to 2");
        blockNumber = 2; // Initialize to a default value if file is not found
        return;
    }

    // Read a line from the file (enough for two digits and a null terminator)
    char buffer[3];  // Buffer to hold the number (2 digits + null terminator)
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        ESP_LOGI(TAG, "Error reading from file or file is empty.");
        
        fclose(file);
        return;
    }
    fclose(file);

    // Convert the string to an integer
    blockNumber = atoi(buffer);  // Convert from string to int

    ESP_LOGI(TAG, "Block number read from LittleFS: %d", blockNumber);
}

// Callback when ESP-NOW message is received
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    const uint8_t *mac_addr = esp_now_info->src_addr;

    // Optionally process the received data
    Message receivedMessage;
    memcpy(&receivedMessage, data, sizeof(receivedMessage));

    Message updateMessage;
    esp_err_t result;
    String colour;

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
                ESP_LOGI(TAG, "Received data from Master, scan reply message sent successfully");
            } else {
                ESP_LOGI(TAG, "Received data from Master, error sending scan reply message");
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
            ESP_LOGI(TAG, "Received data from Master, updated LED colour to %s", colour.c_str());
            break;
        case 2: // block number update
            blockNumber = receivedMessage.number;
            writeBlockNumberToLittleFS(blockNumber);
            ESP_LOGI(TAG, "Received data from Master, updated block number to %d", blockNumber);
            break;
        default:
            ESP_LOGE(TAG, "Received data from Master, wrong message type");
            // send error code back?
    }
      
}


void setupWiFi() {
    // Initialize NVS (needed for wifi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Retry NVS initialization
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret); // Check for errors in initialization    

    // Initialize the Wi-Fi stack in Station mode (STA)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set Wi-Fi mode to Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Disable Wi-Fi power-saving mode to prevent deep sleep (similar to WiFi.setSleep(false))
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // Start the Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set wifi channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Delay for 1 second to allow AP configuration to settle
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
}

extern "C" void app_main(void) {

    //delay(5000);

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/partition",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    readBlockNumberFromLittleFS();

    // Setup Wi-Fi and LED control    
    setupWiFi();

    // Get the MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, slaveMacAddress));

    // Initialize FastLED
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    // make sure the LEDS are off after starting.
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}


