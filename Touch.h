// ═══════════════════════════════════════════════════════════════
//  Touch.h — Khởi tạo & đọc cảm ứng XPT2046 qua HSPI riêng
//  XPT2046 dùng bus HSPI độc lập (GPIO 4/32/33/23)
//  KHÔNG dùng chung với LCD SPI (VSPI: GPIO 25/26/34/13)
//
//  CHỈ include Pins.h (không include Common.h) để Touch.cpp
//  không bị kéo vào COLORS[], TEST_LIST[], driverName() — tránh ODR.
// ═══════════════════════════════════════════════════════════════
#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "Pins.h"

// ─── Cấu trúc hiệu chuẩn affine transform đầy đủ ──────────────
// Công thức: x_screen = ax*rx + bx*ry + dx
//            y_screen = ay*rx + by*ry + dy
struct TouchCalib {
  float ax, bx, dx;
  float ay, by, dy;
  bool  valid;
};

// ─── Cấu trúc điểm chạm đã quy đổi ────────────────────────────
struct TouchPoint {
  int16_t  x, y;    // toạ độ pixel (đã áp transform + touchRotation)
  int16_t  rx, ry;  // toạ độ ADC thô (0–4095)
  uint16_t z;        // áp lực
  bool     touched;
};

// ─── Biến toàn cục touch ───────────────────────────────────────
extern SPIClass            touchSPI;
extern XPT2046_Touchscreen ts;
extern TouchCalib          touchCalib;

// Preferences (NVS) — định nghĩa trong TFT_Test_Suite.ino
extern Preferences prefs;

// ─── Hàm khởi tạo ──────────────────────────────────────────────
void touchInit();

// ─── Đọc điểm chạm (áp transform + rotation, trả về kết quả) ──
TouchPoint touchRead();

// ─── Chờ điểm chạm hợp lệ (blocking) ──────────────────────────
TouchPoint touchWait(uint32_t timeout_ms = 0);

// ─── Lưu & đọc calibration từ NVS ──────────────────────────────
void touchSaveCalib(Preferences& prefs, const TouchCalib& c);
bool touchLoadCalib(Preferences& prefs, TouchCalib& c);

// ─── Giải hệ affine 3 điểm (khử Gauss) ────────────────────────
bool solveAffine(float rx[3], float ry[3], float sx[3], float sy[3],
                 float& ax, float& bx, float& dx,
                 float& ay, float& by, float& dy);

// ─── Áp affine transform và touchRotation ──────────────────────
// GIẢI THÍCH: touchRotation là biến độc lập với screenRotation.
// Calibration thực hiện ở rotation 0; công thức xoay áp theo touchRotation.
// GIẢI ĐỊNH: lắp đồng trục — module 4.11 sẽ xác nhận.
void applyTouchTransform(float rx, float ry, uint8_t touchRot,
                         const TouchCalib& calib,
                         int16_t& sx, int16_t& sy);
