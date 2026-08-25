// ═══════════════════════════════════════════════════════════════
//  Touch.cpp — Implement cảm ứng XPT2046 qua HSPI riêng
// ═══════════════════════════════════════════════════════════════
#include "Touch.h"

// ─── Khởi tạo SPIClass riêng trên HSPI ─────────────────────────
// VSPI (mặc định TFT_eSPI): SCK=25, MOSI=26, MISO=34
// HSPI (touch): SCK=33, MISO=32, MOSI=4, CS=23
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(T_CS_PIN, T_IRQ_PIN);
TouchCalib touchCalib = { 0,0,0, 0,0,0, false };

// ─── Khởi tạo ──────────────────────────────────────────────────
void touchInit() {
  Serial.printf("[Touch] Khoi tao HSPI: SCK=%d MISO=%d MOSI=%d CS=%d IRQ=%d\n",
                T_CLK_PIN, T_DO_PIN, T_DIN_PIN, T_CS_PIN, T_IRQ_PIN);
  // begin(sck, miso, mosi, ss) — ss không dùng bởi HSPI vì ts tự quản CS
  touchSPI.begin(T_CLK_PIN, T_DO_PIN, T_DIN_PIN, T_CS_PIN);
  // Khởi tạo touchscreen với SPI riêng
  ts.begin(touchSPI);
  ts.setRotation(0);  // XPT2046 rotation — sẽ ghi đè bằng touchRotation thủ công
  Serial.printf("[Touch] Khoi tao OK | IRQ pin=%d (input-only, active LOW)\n", T_IRQ_PIN);
}

// ─── Đọc ADC thô (trung bình nhiều lần để giảm nhiễu) ──────────
static bool readRaw(int16_t& rx, int16_t& ry, uint16_t& z) {
  if (!ts.touched()) return false;
  long sumX = 0, sumY = 0, sumZ = 0;
  int valid = 0;
  for (int i = 0; i < TOUCH_SAMPLES; i++) {
    TS_Point p = ts.getPoint();
    if (p.z > TOUCH_Z_THRESHOLD) {
      sumX += p.x; sumY += p.y; sumZ += p.z;
      valid++;
    }
    delayMicroseconds(500);
  }
  if (valid == 0) return false;
  rx = (int16_t)(sumX / valid);
  ry = (int16_t)(sumY / valid);
  z  = (uint16_t)(sumZ / valid);
  return true;
}

// ─── Áp affine transform + touchRotation ───────────────────────
void applyTouchTransform(float rx, float ry, uint8_t touchRot,
                          const TouchCalib& c,
                          int16_t& sx, int16_t& sy) {
  // Bước 1: áp affine calibration (tính ở rotation 0)
  float x0 = c.ax * rx + c.bx * ry + c.dx;
  float y0 = c.ay * rx + c.by * ry + c.dy;

  // Bước 2: áp rotation (theo touchRotation — KHÔNG phải screenRotation)
  // GIẢI THÍCH: calibration xong ở rotation 0 (portrait 240×320).
  // W0=240 (chiều rộng rotation 0), H0=320 (chiều cao rotation 0).
  // GHI CHÚ: đây là công thức giả định lắp đồng trục — module 4.11 sẽ xác nhận.
  const int16_t W0 = PANEL_W_NATIVE;
  const int16_t H0 = PANEL_H_NATIVE;
  switch (touchRot) {
    case 0:  // không đổi
      sx = (int16_t)x0;
      sy = (int16_t)y0;
      break;
    case 1:  // 90° CW: (x,y) → (y, W-x)
      sx = (int16_t)y0;
      sy = (int16_t)(W0 - x0);
      break;
    case 2:  // 180°: (x,y) → (W-x, H-y)
      sx = (int16_t)(W0 - x0);
      sy = (int16_t)(H0 - y0);
      break;
    case 3:  // 270° CW: (x,y) → (H-y, x)
      sx = (int16_t)(H0 - y0);
      sy = (int16_t)x0;
      break;
  }
}

// ─── Đọc điểm chạm đầy đủ ──────────────────────────────────────
TouchPoint touchRead() {
  TouchPoint tp = { 0,0,0,0,0,false };
  int16_t rx, ry; uint16_t z;
  if (!readRaw(rx, ry, z)) return tp;

  tp.rx = rx; tp.ry = ry; tp.z = z; tp.touched = true;

  if (touchCalib.valid) {
    // Đọc touchRotation từ biến toàn cục (định nghĩa trong TFT_Test_Suite.ino)
    extern uint8_t touchRotation;
    applyTouchTransform((float)rx, (float)ry, touchRotation, touchCalib,
                        tp.x, tp.y);
  } else {
    // Chưa có calibration: trả về toạ độ thô (scale về 0–240/0–320 đơn giản)
    tp.x = map(rx, 200, 3900, 0, PANEL_W_NATIVE - 1);
    tp.y = map(ry, 200, 3900, 0, PANEL_H_NATIVE - 1);
  }
  return tp;
}

// ─── Chờ điểm chạm hợp lệ (blocking) ──────────────────────────
TouchPoint touchWait(uint32_t timeout_ms) {
  uint32_t t0 = millis();
  TouchPoint tp;
  do {
    tp = touchRead();
    if (tp.touched) return tp;
    delay(10);
  } while (timeout_ms == 0 || (millis() - t0) < timeout_ms);
  return tp;  // touched = false nếu timeout
}

// ─── Lưu calibration vào NVS ───────────────────────────────────
void touchSaveCalib(Preferences& prefs, const TouchCalib& c) {
  prefs.putFloat("tc_ax", c.ax); prefs.putFloat("tc_bx", c.bx); prefs.putFloat("tc_dx", c.dx);
  prefs.putFloat("tc_ay", c.ay); prefs.putFloat("tc_by", c.by); prefs.putFloat("tc_dy", c.dy);
  prefs.putBool ("tc_ok", c.valid);
  Serial.printf("[Touch] Da luu calibration NVS:\n");
  Serial.printf("        ax=%.6f bx=%.6f dx=%.4f\n", c.ax, c.bx, c.dx);
  Serial.printf("        ay=%.6f by=%.6f dy=%.4f\n", c.ay, c.by, c.dy);
}

// ─── Đọc calibration từ NVS ────────────────────────────────────
bool touchLoadCalib(Preferences& prefs, TouchCalib& c) {
  c.valid = prefs.getBool("tc_ok", false);
  if (!c.valid) {
    Serial.printf("[Touch] NVS: chua co calibration hop le\n");
    return false;
  }
  c.ax = prefs.getFloat("tc_ax", 1.0f);
  c.bx = prefs.getFloat("tc_bx", 0.0f);
  c.dx = prefs.getFloat("tc_dx", 0.0f);
  c.ay = prefs.getFloat("tc_ay", 0.0f);
  c.by = prefs.getFloat("tc_by", 1.0f);
  c.dy = prefs.getFloat("tc_dy", 0.0f);
  Serial.printf("[Touch] Tai calibration tu NVS thanh cong:\n");
  Serial.printf("        ax=%.6f bx=%.6f dx=%.4f\n", c.ax, c.bx, c.dx);
  Serial.printf("        ay=%.6f by=%.6f dy=%.4f\n", c.ay, c.by, c.dy);
  return true;
}

// ─── Giải hệ affine 3 điểm bằng khử Gauss ─────────────────────
// Từ 3 cặp điểm (rx[i],ry[i]) → (sx[i],sy[i]):
//   [rx0 ry0 1] [ax]   [sx0]
//   [rx1 ry1 1] [bx] = [sx1]
//   [rx2 ry2 1] [dx]   [sx2]
// Giải tương tự cho trục Y.
bool solveAffine(float rx[3], float ry[3], float sx[3], float sy[3],
                  float &ax, float &bx, float &dx,
                  float &ay, float &by, float &dy) {
  // Ma trận A = [[rx0,ry0,1],[rx1,ry1,1],[rx2,ry2,1]]
  // Dùng khử Gauss với partial pivoting cho A·[ax,bx,dx]^T = sx
  // Sau đó giải lại cho [ay,by,dy]^T = sy (dùng chung ma trận A)

  // Tạo augmented matrix [A | sx | sy] (3×5)
  float m[3][5];
  for (int i = 0; i < 3; i++) {
    m[i][0] = rx[i]; m[i][1] = ry[i]; m[i][2] = 1.0f;
    m[i][3] = sx[i]; m[i][4] = sy[i];
  }

  // Khử Gauss với partial pivoting
  for (int col = 0; col < 3; col++) {
    // Tìm pivot (phần tử lớn nhất trong cột)
    int pivot = col;
    for (int row = col+1; row < 3; row++) {
      if (fabs(m[row][col]) > fabs(m[pivot][col])) pivot = row;
    }
    // Hoán đổi hàng
    if (pivot != col) {
      for (int k = 0; k < 5; k++) { float t = m[col][k]; m[col][k] = m[pivot][k]; m[pivot][k] = t; }
    }
    // Kiểm tra singular
    if (fabs(m[col][col]) < 1e-6f) {
      Serial.printf("[LOI] solveAffine: 3 diem thang hang, det~0, khong giai duoc!\n");
      return false;
    }
    // Chia hàng pivot
    float div = m[col][col];
    for (int k = col; k < 5; k++) m[col][k] /= div;
    // Khử các hàng khác
    for (int row = 0; row < 3; row++) {
      if (row == col) continue;
      float factor = m[row][col];
      for (int k = col; k < 5; k++) m[row][k] -= factor * m[col][k];
    }
  }

  // Đọc kết quả: sau khử Gauss, m[i][i]=1 và cột 3,4 là giá trị x/y
  ax = m[0][3]; bx = m[1][3]; dx = m[2][3];
  ay = m[0][4]; by = m[1][4]; dy = m[2][4];

  Serial.printf("[Touch] Giai affine xong:\n");
  Serial.printf("        X: ax=%.6f bx=%.6f dx=%.4f\n", ax, bx, dx);
  Serial.printf("        Y: ay=%.6f by=%.6f dy=%.4f\n", ay, by, dy);
  return true;
}
