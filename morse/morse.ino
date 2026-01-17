#include <M5StickCPlus2.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>

// Unique Service UUID for our Morse app
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Variable Definitions
BLEScan* pBLEScan;
BLEAdvertising* pBLEAdvertising;

String currentSymbol = "";  // Buffers ./- for current character
String currentMessage = ""; // Buffers decoded characters
String lastReceivedMorse = "";
unsigned long lastReceivedTime = 0; // Track when last message was received
String receivedMessageDisplay = ""; // Message currently being displayed
unsigned long receivedDisplayStartTime = 0; // When we started displaying received message

unsigned long pressStartTime = 0;
unsigned long lastInputTime = 0;
bool isInputMode = false;
bool sending = false;
unsigned long sendStartTime = 0;

unsigned long lastDisplayUpdate = 0;
int scanDots = 0;

// Timeouts
const unsigned long RECEIVED_DISPLAY_DURATION = 5000; // Display received message for 5 seconds
const unsigned long CHAR_TIMEOUT = 1200; // Time to wait before settling a character
const unsigned long MSG_TIMEOUT  = 3000; // Time to wait before sending message
const unsigned long SEND_DURATION = 12000; // Duration to broadcast message (12 seconds for maximum reliability)
const unsigned long DUPLICATE_TIMEOUT = 10000; // Time to allow same message again

// Morse Decode Map
char decodeMorse(String morse) {
  if (morse == ".-") return 'A';
  if (morse == "-...") return 'B';
  if (morse == "-.-.") return 'C';
  if (morse == "-..") return 'D';
  if (morse == ".") return 'E';
  if (morse == "..-.") return 'F';
  if (morse == "--.") return 'G';
  if (morse == "....") return 'H';
  if (morse == "..") return 'I';
  if (morse == ".---") return 'J';
  if (morse == "-.-") return 'K';
  if (morse == ".-..") return 'L';
  if (morse == "--") return 'M';
  if (morse == "-.") return 'N';
  if (morse == "---") return 'O';
  if (morse == ".--.") return 'P';
  if (morse == "--.-") return 'Q';
  if (morse == ".-.") return 'R';
  if (morse == "...") return 'S';
  if (morse == "-") return 'T';
  if (morse == "..-") return 'U';
  if (morse == "...-") return 'V';
  if (morse == ".--") return 'W';
  if (morse == "-..-") return 'X';
  if (morse == "-.--") return 'Y';
  if (morse == "--..") return 'Z';
  return '?'; // Unknown
}

// BLE Scan Callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
        String data = advertisedDevice.getName().c_str();

        if (data.startsWith("M:")) {
          String m = data.substring(2);

          // Allow same message if enough time has passed or if it's different
          bool isDuplicate = (m == lastReceivedMorse) && (millis() - lastReceivedTime < DUPLICATE_TIMEOUT);

          if (!isDuplicate && m.length() > 0) {
             lastReceivedMorse = m;
             lastReceivedTime = millis();
             receivedMessageDisplay = m;
             receivedDisplayStartTime = millis();

             StickCP2.Display.fillScreen(BLACK);
             StickCP2.Display.setCursor(0, 0);
             StickCP2.Display.setTextSize(2);
             StickCP2.Display.println("RX MSG:");

             StickCP2.Display.setCursor(0, 40);
             StickCP2.Display.setTextSize(3);
             StickCP2.Display.print(m);

             // Long feedback tone (500ms)
             StickCP2.Speaker.tone(2000, 500);
          }
        }
      }
    }
};

void startAdvertising(String msg) {
  if (pBLEAdvertising == nullptr) {
      BLEDevice::init("M5Morse");
      pBLEAdvertising = BLEDevice::getAdvertising();
      // Set maximum advertising frequency: 20ms fixed for best reception
      pBLEAdvertising->setMinInterval(32);  // 32 * 0.625ms = 20ms
      pBLEAdvertising->setMaxInterval(32);  // 32 * 0.625ms = 20ms (fixed interval)
  }

  // Set maximum TX power for better range
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

  pBLEAdvertising->stop();

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  oAdvertisementData.setFlags(0x04);
  oAdvertisementData.setCompleteServices(BLEUUID(SERVICE_UUID));

  String payload = "M:" + msg;
  if(payload.length() > 29) payload = payload.substring(0, 29);

  oAdvertisementData.setName(payload.c_str());

  pBLEAdvertising->setAdvertisementData(oAdvertisementData);
  pBLEAdvertising->start();
  sending = true;
  sendStartTime = millis();
}

void stopAdvertising() {
  if (pBLEAdvertising) pBLEAdvertising->stop();
  sending = false;
}

void updateInputDisplay() {
    StickCP2.Display.fillScreen(BLUE);
    StickCP2.Display.setCursor(0,0);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.println("Input...");
    
    // Show current message
    StickCP2.Display.setCursor(0, 30);
    StickCP2.Display.setTextSize(3);
    StickCP2.Display.print(currentMessage.c_str());
    
    // Show current symbol buffer (e.g. ".-")
    StickCP2.Display.print(" ");
    StickCP2.Display.setTextColor(YELLOW, BLUE);
    StickCP2.Display.print(currentSymbol.c_str());
    StickCP2.Display.setTextColor(WHITE, BLUE);
}

void updateReadyDisplay() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setCursor(0, 10);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.println("READY");

  StickCP2.Display.setCursor(0, 40);
  StickCP2.Display.setTextSize(1);
  StickCP2.Display.setTextColor(GREEN, BLACK);
  StickCP2.Display.print("SCAN");

  // Animate dots to show scanning
  for (int i = 0; i < scanDots; i++) {
    StickCP2.Display.print(".");
  }
  StickCP2.Display.setTextColor(WHITE, BLACK);

  StickCP2.Display.setCursor(0, 55);
  StickCP2.Display.println("Press Btn to");
  StickCP2.Display.setCursor(0, 65);
  StickCP2.Display.println("start input");
}

void setup() {
  auto cfg = M5.config();
  StickCP2.begin(cfg);
  StickCP2.Display.setRotation(1);
  StickCP2.Display.setTextSize(2);
  updateReadyDisplay();

  BLEDevice::init("M5Morse");

  // Set maximum RX power for better reception
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100); // Scan interval 100ms
  pBLEScan->setWindow(99);    // Scan window 99ms (almost continuous)
}

void loop() {
  StickCP2.update();

  // Button A: Input Start / Typing
  if (StickCP2.BtnA.wasPressed()) {
      if (!isInputMode) {
          isInputMode = true;
          currentMessage = "";
          currentSymbol = "";
          updateInputDisplay();
      }
      pressStartTime = millis();
  }
  
  if (isInputMode) {
     if (StickCP2.BtnA.wasReleased()) {
        unsigned long duration = millis() - pressStartTime;
        
        if (duration < 250) {
           currentSymbol += ".";
           StickCP2.Speaker.tone(4000, 50);
        } else {
           currentSymbol += "-";
           StickCP2.Speaker.tone(4000, 150);
        }
        lastInputTime = millis();
        updateInputDisplay();
     }
     
     unsigned long idleTime = millis() - lastInputTime;
     
     // 1. Character Timeout: Commit currentSymbol to currentMessage
     if (currentSymbol.length() > 0 && idleTime > CHAR_TIMEOUT) {
         char c = decodeMorse(currentSymbol);
         currentMessage += c;
         currentSymbol = "";
         updateInputDisplay();
         StickCP2.Speaker.tone(3000, 50); 
     }
     
     // 2. Message Timeout: Send currentMessage
     if (currentMessage.length() > 0 && currentSymbol.length() == 0 && idleTime > MSG_TIMEOUT) {
         isInputMode = false;
         
         StickCP2.Display.fillScreen(GREEN);
         StickCP2.Display.setCursor(0, 20);
         StickCP2.Display.setTextSize(2);
         StickCP2.Display.println("TX String:");
         StickCP2.Display.setCursor(0, 50);
         StickCP2.Display.setTextSize(3);
         StickCP2.Display.print(currentMessage);
         
         startAdvertising(currentMessage);
         currentMessage = "";
     }
  }

  // Check if received message display time has expired
  if (receivedMessageDisplay.length() > 0 &&
      millis() - receivedDisplayStartTime > RECEIVED_DISPLAY_DURATION) {
    receivedMessageDisplay = "";
    scanDots = 0;
    lastDisplayUpdate = millis();
    updateReadyDisplay();
  }

  // Update sending display with countdown
  if (sending) {
    unsigned long elapsed = millis() - sendStartTime;
    if (elapsed % 1000 < 100) { // Update every second
      unsigned long remaining = (SEND_DURATION - elapsed) / 1000;
      StickCP2.Display.fillRect(0, 70, 160, 20, GREEN); // Clear countdown area
      StickCP2.Display.setCursor(0, 70);
      StickCP2.Display.setTextSize(2);
      StickCP2.Display.setTextColor(WHITE, GREEN);
      StickCP2.Display.print("TX: ");
      StickCP2.Display.print(remaining);
      StickCP2.Display.print("s");
      StickCP2.Display.setTextColor(WHITE, BLACK);
    }
  }

  // Scanning when not inputting or sending
  if (!isInputMode && !sending) {
     pBLEScan->start(2, false); // Scan for 2 seconds (increased for better reception)
     pBLEScan->clearResults();

     // Update scan animation every 500ms (only if not displaying received message)
     if (receivedMessageDisplay.length() == 0 && millis() - lastDisplayUpdate > 500) {
       lastDisplayUpdate = millis();
       scanDots = (scanDots + 1) % 4; // Cycle through 0, 1, 2, 3
       updateReadyDisplay();
     }
  }

  // Stop sending after SEND_DURATION
  if (sending && millis() - sendStartTime > SEND_DURATION) {
      stopAdvertising();
      scanDots = 0;
      lastDisplayUpdate = millis();
      updateReadyDisplay();
  }
}
