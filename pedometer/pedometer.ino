#include <M5StickCPlus2.h>
#include "ImgData.h"

// Pedometer variables
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;
float lastMag = 0.0F;
int stepCount = 0;
bool isStep = false;
unsigned long lastStepTime = 0;
const int minStepInterval = 200; // Minimum time (ms) between steps
const float stepThreshold = 1.3; // Threshold for step detection (tune as needed)

void setup() {
    auto cfg = M5.config();
    StickCP2.begin(cfg);
    StickCP2.Display.setRotation(0); // Vertical Mode (Portrait)
    StickCP2.Display.fillScreen(BLACK);

    // Initialize IMU
    StickCP2.Imu.init();

    // Display the image scaled up
    float zoom = 3.0; // Slightly larger for vertical width
    StickCP2.Display.pushImageRotateZoom(
        StickCP2.Display.width() / 2.0,  // Center X
        StickCP2.Display.height() / 3.0, // Top 1/3 Y
        IMG_WIDTH / 2.0, IMG_HEIGHT / 2.0, // Source Center (Rotate around center)
        0,                               // Angle
        zoom, zoom,                      // Zoom X, Zoom Y
        IMG_WIDTH, IMG_HEIGHT,           // Source Width, Height
        IMG_DATA[0]                      // Data
    );

    // Initial Text Display
    StickCP2.Display.setTextSize(3);
    StickCP2.Display.setTextColor(WHITE, BLACK);
    StickCP2.Display.setTextDatum(top_center); // Center text horizontally
    StickCP2.Display.drawString("00000", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 + 20);
}

void loop() {
    StickCP2.update();

    // Read Acceleration
    StickCP2.Imu.getAccelData(&accX, &accY, &accZ);
    
    // Calculate magnitude
    float magnitude = sqrt(accX * accX + accY * accY + accZ * accZ);

    // Step Detection Algorithm
    if (magnitude > stepThreshold && !isStep && (millis() - lastStepTime > minStepInterval)) {
        isStep = true;
        stepCount++;
        lastStepTime = millis();
        
        // Update Display
        StickCP2.Display.setTextDatum(top_center);
        // Use drawString with background color to overwrite previous value properly
        StickCP2.Display.setTextColor(WHITE, BLACK); 
        char buf[8];
        snprintf(buf, sizeof(buf), "%05d", stepCount);
        StickCP2.Display.drawString(buf, StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 + 20);
    } else if (magnitude < stepThreshold) {
        isStep = false;
    }

    delay(20);
}
