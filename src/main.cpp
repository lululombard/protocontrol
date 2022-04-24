#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <stdint.h>

//#define FASTLED_ESP32_I2S

#include <FastLED.h>
// #include "AsyncJson.h"
// #include "ArduinoJson.h"
// #include "wifi_credentials.h"
#include "facestorage.h"
// #include "webserver.h"
#include "matrix.h"
#include "matrixmanager.h"
// #include "max7219painter.h"

#include "displays/ws2812/ws2812display.h"
#include "displays/ws2812/ws2812matrix.h"
#include "displays/ws2812/ws2812string.h"

#include "scene/scene.h"
#include "scene/elements/bitmapelement.h"
#include "scene/elements/adagfxelement.h"
#include "scene/elements/targetfollowerelement.h"
#include "scene/visitors/elementprinter.h"
#include "scene/visitors/elementrgbbitmapsetter.h"
#include "scene/visitors/elementdrawer.h"
#include "scene/visitors/elementupdater.h"
#include "scene/modifiers/mirror.h"
#include "scene/modifiers/rainbow.h"

#include "bitmaps.h"

#define NUM_LEDS_SIDEPANEL 15 + 8 + 16 + 12
ProtoControl::BitmapManager<256> bitmapManager;

// Define the layout of our physical display
// The Strings need to be pointers to prevent slicing,
// since we treat them as their base class internally.
WS2812Display display {
    new WS2812StringPin<16> {
            WS2812Matrix(16, 8, 0),
            WS2812Matrix(32, 8, 0),
            WS2812Matrix(8, 8, 0),
    },
    new WS2812StringPin<17> {
            WS2812Matrix(16, 8, 2),
            WS2812Matrix(32, 8, 2),
            WS2812Matrix(8, 8, 2),
    },
    new WS2812StringPin<21> {
            WS2812Matrix(8, 8, 0),
    },
    new WS2812StringPin<22> {
            WS2812Matrix(8, 8, 0),
    }
};

Scene scene {
  new Rainbow<TargetFollowerElement> {"eye_r_follower", 16, 8, 0, 0, {
    new MirrorHorizontal<BitmapElement> {"eye_r",   0,  0},
  }},
  new Rainbow<TargetFollowerElement> {"mouth_r_follower", 32, 8, 16, 0, {
    new MirrorHorizontal<BitmapElement> {"mouth_r", 0, 0},
  }},
  new Rainbow<TargetFollowerElement> {"nose_r_follower", 8, 8, 48, 0, {
    new MirrorHorizontal<BitmapElement> {"nose_r",  0, 0},
  }},
  new Rainbow<TargetFollowerElement> {"eye_l_follower", 16, 8, 56, 0, {
    new BitmapElement {"eye_l",   0, 0},
  }},
  new Rainbow<TargetFollowerElement> {"mouth_l_follower", 32, 8, 72, 0, {
    new BitmapElement {"mouth_l", 0, 0},
  }},
  new Rainbow<TargetFollowerElement> {"nose_l_follower", 8, 8, 104, 0, {
    new BitmapElement {"nose_l",  0, 0},
  }},
  new Rainbow<BitmapElement> {"ear_l", 112, 0},
  new Rainbow<BitmapElement> {"ear_r", 120, 0},
};

// Used to draw to the display
ElementDrawer drawer(display);

// Used to set bitmaps in the scene
ElementRGBBitmapSetter bmpsetter;
ElementRGBBitmapSetter eyesclosedsetter;

// Used to print a text representation of the scene
ElementPrinter ep(Serial);

// Used to update the elements in the scene
ElementUpdater updater;

void setup()
{
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  Serial.begin(115200);

  // Set up littleFS
  if(!LITTLEFS.begin(true)){
      Serial.println("LittleFS Mount Failed");
      while(true) {}
  }

  // Collect and load the bitmaps in the LITTLEFS /565 folder
  bitmapManager.gather(LITTLEFS, "/565");

  ep.visit(&scene);

  bmpsetter
    .add("eye_r", bitmapManager.get("/565/proto_eye"))
    .add("eye_l", bitmapManager.get("/565/proto_eye"))
    .add("nose_r", bitmapManager.get("/565/proto_nose"))
    .add("nose_l", bitmapManager.get("/565/proto_nose"))
    .add("mouth_r", bitmapManager.get("/565/proto_mouth"))
    .add("mouth_l", bitmapManager.get("/565/proto_mouth"))
    .add("ear_l", bitmapManager.get("/565/proto_ear"))
    .add("ear_r", bitmapManager.get("/565/proto_ear"))
    .visit(&scene);

  eyesclosedsetter
    .add("eye_r", bitmapManager.get("/565/proto_eye_closed"))
    .add("eye_l", bitmapManager.get("/565/proto_eye_closed"));

  drawer.visit(&scene);

  display.setBrightness(255);
}

unsigned long prev = 0;
void loop()
{
  // For updating
  unsigned long now = micros();
  unsigned long updateDelta = now - prev;
  unsigned long prev = now;

  // For benchmarking
  unsigned long before;
  unsigned long after;
  unsigned long delta;

  // Update
  before = micros();

  updater.setTimeDelta(micros());
  updater.visit(&scene);

  if (now / 200000 % 2) {
    eyesclosedsetter.visit(&scene);
  } else {
    bmpsetter.visit(&scene);
  }

  after = micros();

  delta = after - before;
  Serial.print("Updating took ");
  Serial.print(delta);
  Serial.println(" us");

  // Clear
  before = micros();
  display.clear();
  after = micros();

  delta = after - before;
  Serial.print("Clearing took ");
  Serial.print(delta);
  Serial.println(" us");

  // Draw
  before = micros();
  drawer.visit(&scene);
  after = micros();

  delta = after - before;
  Serial.print("Drawing took ");
  Serial.print(delta);
  Serial.println(" us");

  display.show();
}