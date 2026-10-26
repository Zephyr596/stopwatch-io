
#include <M5Unified.h>

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(80, 180);
    M5.Display.println("TESLA");
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setCursor(80, 180);
        M5.Display.println("HELLO!");
    }

    if (M5.BtnB.wasPressed()) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setCursor(80, 180);
        M5.Display.println("WORLD!");
    }

    delay(10);
}
