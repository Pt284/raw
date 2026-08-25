// ═══════════════════════════════════════════════════════════════
//  Common.h — Tiện ích dùng chung (chỉ include từ .ino files)
//  KHÔNG include file này từ .cpp file — dùng Pins.h thay thế
//  Lý do: COLORS[], TEST_LIST[], driverName() phải được định nghĩa
//  đúng 1 lần trong toàn chương trình (ODR — One Definition Rule)
// ═══════════════════════════════════════════════════════════════
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Pins.h"

// ─── Bảng 8 màu cơ bản ─────────────────────────────────────────
// static: mỗi translation unit có bản riêng — OK vì chỉ .ino include file này
struct Color { const char* name; uint16_t val; };
static const Color COLORS[] = {
  { "RED",     TFT_RED     },
  { "GREEN",   TFT_GREEN   },
  { "BLUE",    TFT_BLUE    },
  { "YELLOW",  TFT_YELLOW  },
  { "CYAN",    TFT_CYAN    },
  { "MAGENTA", TFT_MAGENTA },
  { "WHITE",   TFT_WHITE   },
  { "BLACK",   TFT_BLACK   },
};
static const int NC = sizeof(COLORS) / sizeof(COLORS[0]);

// ─── Tự nhận tên driver từ User_Setup.h ────────────────────────
// inline: đảm bảo không bị "multiple definition" nếu lỡ include nhiều lần
inline const char* driverName() {
  #if defined(ILI9341_DRIVER)
    return "ILI9341";
  #elif defined(ILI9341_2_DRIVER)
    return "ILI9341_2";
  #elif defined(ST7789_DRIVER)
    return "ST7789";
  #elif defined(ST7789_2_DRIVER)
    return "ST7789_2";
  #elif defined(GC9A01_DRIVER)
    return "GC9A01";
  #elif defined(ST7735_DRIVER)
    return "ST7735";
  #else
    return "UNKNOWN";
  #endif
}

// ─── Màu chữ tương phản (luma) ─────────────────────────────────
// inline: an toàn khi include nhiều nơi
inline uint16_t fg(uint16_t bg) {
  uint8_t r = (bg >> 11) & 0x1F;
  uint8_t g = (bg >>  5) & 0x3F;
  uint8_t b =  bg        & 0x1F;
  return ((uint32_t)r * 77 + g * 38 + b * 29) > 4000 ? TFT_BLACK : TFT_WHITE;
}

// ─── Trạng thái ứng dụng ───────────────────────────────────────
enum AppState {
  STATE_CALIBRATION,
  STATE_MENU,
  STATE_TEST_RUNNING,
};

// ─── ID module test ─────────────────────────────────────────────
enum TestID {
  TEST_NONE = 0,
  TEST_PATTERN,
  TEST_DEFECTIVE_PIXELS,
  TEST_UNIFORMITY,
  TEST_COLOR_DISTANCE,
  TEST_GRADIENT,
  TEST_SHARPNESS,
  TEST_VIEWING_ANGLE,
  TEST_GAMMA,
  TEST_TOUCH_COVERAGE,
  TEST_TOUCH_CALIB,
  TEST_ROTATION_DISC,
  TEST_SD_CARD,
  TEST_BENCHMARK,
  TEST_RGB_SWATCH,
  TEST_BULLSEYE,
  TEST_SOAK,
  TEST_FREEDRAW,
  TEST_CHECKERBOARD,
  TEST_IMAGE_VIEWER,
  TEST_COUNT
};

// ─── Danh sách module có tên hiển thị ──────────────────────────
struct TestEntry { TestID id; const char* label; const char* serial; };
static const TestEntry TEST_LIST[] = {
  { TEST_PATTERN,          "Pattern TH",    "TestPattern"     },
  { TEST_DEFECTIVE_PIXELS, "Diem Chet",     "DefectPixels"    },
  { TEST_UNIFORMITY,       "Dong Deu",      "Uniformity"      },
  { TEST_COLOR_DISTANCE,   "Phan Giai Mau", "ColorDist"       },
  { TEST_GRADIENT,         "Gradient",      "Gradient"        },
  { TEST_SHARPNESS,        "Do Net",        "Sharpness"       },
  { TEST_VIEWING_ANGLE,    "Goc Nhin",      "ViewAngle"       },
  { TEST_GAMMA,            "Gamma",         "Gamma"           },
  { TEST_TOUCH_COVERAGE,   "Luoi Cam Ung",  "TouchCoverage"   },
  { TEST_TOUCH_CALIB,      "Hieu Chuan CU", "TouchCalib"      },
  { TEST_ROTATION_DISC,    "Do Xoay",       "RotDisc"         },
  { TEST_SD_CARD,          "The SD",        "SDCard"          },
  { TEST_BENCHMARK,        "Benchmark",     "Benchmark"       },
  { TEST_RGB_SWATCH,       "RGB Swatch",    "RGBSwatch"       },
  { TEST_BULLSEYE,         "Do Chinh Xac",  "Bullseye"        },
  { TEST_SOAK,             "Soak Test",     "SoakTest"        },
  { TEST_FREEDRAW,         "Ve Tu Do",      "FreeDraw"        },
  { TEST_CHECKERBOARD,     "Checker Mau",   "Checkerboard"    },
  { TEST_IMAGE_VIEWER,     "Xem Anh SD",    "ImageViewer"     },
};
static const int TEST_COUNT_LIST = sizeof(TEST_LIST) / sizeof(TEST_LIST[0]);

// ─── Hằng số UI ────────────────────────────────────────────────
#define MENU_ITEMS_PER_PAGE  6
#define MENU_BTN_H          40
#define MENU_BTN_MARGIN      4
#define MENU_HEADER_H       28
#define MENU_FOOTER_H       52
#define MENU_BTN_COLOR      0x2945
#define MENU_BTN_PRESS      0x4A69
#define MENU_HEADER_COLOR   0x0310
#define MENU_WARN_COLOR     TFT_RED
#define NAV_BTN_COLOR       0x3186
#define ROT_BTN_COLOR       0x6228
#define CALIB_BTN_COLOR     0x03A4
#define BACK_BTN_COLOR      0x8410
#define BACK_BTN_W          52
#define BACK_BTN_H          22

// ─── Prototype helper UI (implement trong TFT_Test_Suite.ino) ──
extern TFT_eSPI tft;

void drawBtn(int x, int y, int w, int h, const char* label,
             uint16_t bgColor, uint16_t textColor = TFT_WHITE,
             uint8_t textSize = 1);
bool inRect(int tx, int ty, int rx, int ry, int rw, int rh);
void drawScreenTitle(const char* title);
void drawBackBtn();
bool isBackBtn(int tx, int ty);
void savePrefs();

// ─── Biến toàn cục (định nghĩa trong TFT_Test_Suite.ino) ───────
extern uint8_t  screenRotation;
extern uint8_t  touchRotation;
extern uint8_t  ledBrightness;
extern AppState appState;
extern TestID   currentTest;
extern uint8_t  recommendedTouchRot[4];

// Preferences được khai báo extern trong Touch.h (đã include <Preferences.h>)
// Không khai báo ở đây để tránh kéo <Preferences.h> vào mọi nơi include Common.h
