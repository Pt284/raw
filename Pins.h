// ═══════════════════════════════════════════════════════════════
//  Pins.h — Khai báo tập trung toàn bộ chân GPIO & hằng số cấu hình
//  Include file này thay vì Common.h khi chỉ cần pin defines
//  (không kéo theo TFT_eSPI, COLORS[], TEST_LIST[], v.v.)
// ═══════════════════════════════════════════════════════════════
#pragma once

// ─── Đèn nền LCD ───────────────────────────────────────────────
// ⚠ GPIO12 là strapping pin (phải LOW lúc boot). Xem Common.h.
// Khuyến nghị đổi sang GPIO19 nếu gặp boot lỗi ngẫu nhiên.
#define TFT_LED_PIN     12

// ─── Touch XPT2046 — HSPI riêng, KHÔNG dùng chung LCD SPI ─────
#define T_CLK_PIN       33
#define T_DO_PIN        32
#define T_DIN_PIN        4
#define T_CS_PIN        23
#define T_IRQ_PIN       35  // input-only pin

// ─── SD card — dùng chung SPI với LCD (VSPI), CS riêng ─────────
#define SD_CS_PIN       21
#define SPI_MISO_PIN    34  // input-only pin

// ─── Kích thước panel gốc ở rotation 0 ────────────────────────
#define PANEL_W_NATIVE  240
#define PANEL_H_NATIVE  320

// ─── Cấu hình PWM đèn nền ──────────────────────────────────────
#define LED_PWM_FREQ    5000
#define LED_PWM_RES     8
#define LED_PWM_CHANNEL 0
#define LED_BRIGHTNESS_MIN  13
#define LED_BRIGHTNESS_MAX  255
#define LED_BRIGHTNESS_DEF  200

// ─── Cấu hình lấy mẫu cảm ứng ─────────────────────────────────
#define TOUCH_SAMPLES       5
#define TOUCH_Z_THRESHOLD   300
#define TOUCH_DEBOUNCE_MS   30
#define TOUCH_CALIB_MARGIN  20
