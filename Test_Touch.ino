// ═══════════════════════════════════════════════════════════════
//  Test_Touch.ino — Modules cảm ứng (Phase 1+2: 4.10, 4.11)
//  Phase 4 sẽ thêm: 4.9 (lưới toạ độ), 4.15 (bullseye), 4.18 (vẽ tự do)
// ═══════════════════════════════════════════════════════════════
#include "Common.h"
#include "Touch.h"

// ─── Hàm tiện ích nội bộ ───────────────────────────────────────
// Vẽ dấu + tại (cx,cy) với bán kính r, màu c (dùng làm target)
static void drawCrosshair(int cx, int cy, int r, uint16_t c) {
  tft.drawFastHLine(cx - r, cy, r*2+1, c);
  tft.drawFastVLine(cx, cy - r, r*2+1, c);
  tft.drawCircle(cx, cy, r/2, c);
  tft.drawCircle(cx, cy, r,   c);
}

// Lấy trung bình nhiều lần chạm liên tiếp, bỏ outlier đơn giản
static bool readAvgRaw(int16_t& rx, int16_t& ry, uint16_t& rz,
                        int sampleCount = 16, uint32_t timeoutMs = 5000) {
  Serial.printf("[Calib] Doc trung binh %d lan...\n", sampleCount);
  uint32_t t0 = millis();
  long sumX = 0, sumY = 0, sumZ = 0;
  int  cnt  = 0;
  while (cnt < sampleCount && (millis() - t0) < timeoutMs) {
    TS_Point p = ts.getPoint();
    if (p.z > TOUCH_Z_THRESHOLD) {
      sumX += p.x; sumY += p.y; sumZ += p.z;
      cnt++;
      delayMicroseconds(800);
    }
    delay(5);
  }
  if (cnt == 0) return false;
  rx = (int16_t)(sumX / cnt);
  ry = (int16_t)(sumY / cnt);
  rz = (uint16_t)(sumZ / cnt);
  Serial.printf("[Calib] Raw trung binh(%d mau): rx=%d ry=%d rz=%d\n", cnt, rx, ry, rz);
  return true;
}

// ═══════════════════════════════════════════════════════════════
//  4.10 — Hiệu chuẩn cảm ứng affine 3 điểm
// ═══════════════════════════════════════════════════════════════
void runTouchCalib() {
  Serial.printf("[TouchCalib] Bat dau\n");

  // Lưu rotation hiện tại — sẽ restore sau khi xong
  uint8_t savedRot = screenRotation;

  // Hiệu chuẩn LUÔN thực hiện ở rotation 0 (portrait 240×320)
  // để công thức xoay ở Touch.cpp luôn có điểm tham chiếu nhất quán
  applyScreenRotation(0);
  int W = tft.width();   // = 240
  int H = tft.height();  // = 320

  // ─── 3 điểm hiệu chuẩn (cách mép đủ xa để tránh vùng chết rìa cảm ứng) ─
  const int M = TOUCH_CALIB_MARGIN;
  // Điểm P0: góc trên-trái
  // Điểm P1: góc trên-phải
  // Điểm P2: dưới-giữa (không thẳng hàng với P0/P1)
  int ptX[3] = { M,       W - M,   W / 2  };
  int ptY[3] = { M,       M,       H - M  };

  float rawRX[3], rawRY[3];
  float scrX[3],  scrY[3];

  for (int pt = 0; pt < 3; pt++) {
    // Vẽ màn hướng dẫn
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);

    char buf[48];
    snprintf(buf, sizeof(buf), "Hieu chuan %d/3", pt + 1);
    tft.setCursor(W/2 - 40, H/2 - 40);
    tft.print(buf);
    tft.setCursor(W/2 - 60, H/2 - 25);
    tft.print("Cham chinh xac vao dau +");
    tft.setCursor(W/2 - 50, H/2 - 12);
    tft.print("(giu den khi hien tien do)");

    // Vẽ crosshair tại điểm target
    drawCrosshair(ptX[pt], ptY[pt], 14, TFT_RED);

    // Chờ nhấc tay trước (tránh đọc ngay điểm cũ)
    while (ts.touched()) delay(10);
    delay(200);

    // Chờ chạm vào
    tft.setCursor(ptX[pt] + 18, ptY[pt] + 4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("Cham!");

    int16_t rx=0, ry=0; uint16_t rz=0;
    bool got = false;

    // Chờ chạm (timeout 15 giây)
    uint32_t t0 = millis();
    while (millis() - t0 < 15000) {
      if (ts.touched()) {
        // Chờ ổn định 100ms rồi lấy trung bình
        delay(100);
        got = readAvgRaw(rx, ry, rz, 20, 3000);
        if (got) break;
      }
      // Vẽ thanh timeout đơn giản
      int elapsed = (int)(millis() - t0);
      tft.fillRect(M, H - 20, (W - M*2) * elapsed / 15000, 8, TFT_DARKGREY);
      delay(50);
    }

    if (!got) {
      // Timeout — thông báo lỗi và thoát
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(10, H/2);
      tft.print("[LOI] Timeout hieu chuan!");
      Serial.printf("[LOI] TouchCalib: timeout diem %d\n", pt);
      delay(2000);
      Serial.printf("[TouchCalib] Ket thuc (that bai)\n");
      applyScreenRotation(savedRot); // restore rotation
      return;
    }

    // Đổi màu crosshair sang xanh lá (xác nhận đã đọc)
    drawCrosshair(ptX[pt], ptY[pt], 14, TFT_GREEN);

    rawRX[pt] = (float)rx;
    rawRY[pt] = (float)ry;
    scrX[pt]  = (float)ptX[pt];
    scrY[pt]  = (float)ptY[pt];

    Serial.printf("[Calib] Diem %d: screen(%d,%d) <- raw(%d,%d)\n",
                  pt, ptX[pt], ptY[pt], rx, ry);
    delay(500);
  }

  // ─── Giải hệ affine ─────────────────────────────────────────
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 80); tft.print("Dang tinh he so...");

  TouchCalib newCalib;
  bool ok = solveAffine(rawRX, rawRY, scrX, scrY,
                        newCalib.ax, newCalib.bx, newCalib.dx,
                        newCalib.ay, newCalib.by, newCalib.dy);
  if (!ok) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 100); tft.print("[LOI] 3 diem thang hang!");
    tft.setCursor(10, 115); tft.print("Thu lai voi diem dat khac");
    Serial.printf("[LOI] TouchCalib: 3 diem thang hang, khong giai duoc affine\n");
    delay(3000);
    applyScreenRotation(savedRot); // restore rotation
    return;
  }
  newCalib.valid = true;
  touchCalib = newCalib;

  // ─── Xác nhận nhanh (điểm thứ 4 — không trùng 3 điểm calibrate) ─
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 40);  tft.print("Xac nhan calibration:");
  tft.setCursor(10, 55);  tft.print("Cham vao dau + xanh duoi day");

  int vx = W / 2, vy = H * 2 / 3;  // điểm xác nhận (không trùng P0/P1/P2)
  drawCrosshair(vx, vy, 12, TFT_CYAN);

  while (ts.touched()) delay(10);
  delay(200);

  int16_t vrx=0, vry=0; uint16_t vrz=0;
  bool vgot = false;
  uint32_t tVerify = millis();
  while (millis() - tVerify < 10000) {
    if (ts.touched()) {
      delay(100);
      vgot = readAvgRaw(vrx, vry, vrz, 16, 2000);
      if (vgot) break;
    }
    delay(30);
  }

  if (vgot) {
    int16_t cx=0, cy=0;
    applyTouchTransform((float)vrx, (float)vry, 0, newCalib, cx, cy);
    int errX = abs(cx - vx), errY = abs(cy - vy);
    int errTotal = (int)sqrt((float)(errX*errX + errY*errY));
    Serial.printf("[Calib] Xac nhan: target(%d,%d) got(%d,%d) sai_so=%dpx (%d,%d)\n",
                  vx, vy, cx, cy, errTotal, errX, errY);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(errTotal < 10 ? TFT_GREEN : (errTotal < 20 ? TFT_YELLOW : TFT_RED),
                     TFT_BLACK);
    tft.setCursor(10, 60);
    tft.printf("Sai so: %d px", errTotal);
    tft.setCursor(10, 76);
    if (errTotal < 10)      tft.print("Rat tot! (<10px)");
    else if (errTotal < 20) tft.print("Chap nhan duoc (<20px)");
    else                    tft.print("Kem - thu hieu chuan lai");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 100); tft.printf("Target: (%d,%d)", vx, vy);
    tft.setCursor(10, 115); tft.printf("Do: (%d,%d)", cx, cy);
    tft.setCursor(10, 130); tft.printf("Lech: (%d,%d)px", cx-vx, cy-vy);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 80); tft.print("Timeout xac nhan — luu calibration");
    Serial.printf("[WARN] TouchCalib: timeout xac nhan, van luu calibration\n");
  }

  // ─── Lưu calibration vào NVS ─────────────────────────────────
  touchSaveCalib(prefs, newCalib);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 155); tft.print("Da luu NVS!");
  tft.setCursor(10, 170); tft.print("Cham bat ky de tiep tuc...");
  Serial.printf("[TouchCalib] Ket thuc - calibration da luu\n");

  while (ts.touched()) delay(10);
  while (!ts.touched() && !Serial.available()) delay(50);
  while (ts.touched()) delay(10);

  // Restore rotation về trước khi calibrate
  applyScreenRotation(savedRot);
  Serial.printf("[TouchCalib] Restore screenRot=%d\n", savedRot);
}

// ═══════════════════════════════════════════════════════════════
//  4.11 — Auto-discovery bảng ghép cặp xoay (rotation × touchRotation)
// ═══════════════════════════════════════════════════════════════
void runRotationDiscovery() {
  Serial.printf("[RotDisc] Bat dau tu dong do touchRotation cho tung screenRotation\n");

  uint8_t discovered[4] = { 0xFF, 0xFF, 0xFF, 0xFF };  // 0xFF = chưa dò
  bool    allDone = true;

  for (int sRot = 0; sRot < 4; sRot++) {
    applyScreenRotation(sRot);
    int W = tft.width();
    int H = tft.height();

    // Đặt landmark tại góc trên-trái thực (cách mép 20px)
    // GHI CHÚ: cần đủ lệch khỏi trung tâm để phân biệt rõ 4 ứng viên touchRotation
    int LX = 20, LY = 20;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);

    char buf[48];
    snprintf(buf, sizeof(buf), "Do xoay %d/4 (screenRot=%d)", sRot+1, sRot);
    tft.setCursor(W/2 - 60, H/2 - 50); tft.print(buf);
    tft.setCursor(W/2 - 70, H/2 - 35); tft.print("Cham vao o vuong xanh (goc tren-trai)");

    // Vẽ hình vuông landmark
    tft.fillRect(LX - 12, LY - 12, 24, 24, TFT_BLUE);
    tft.drawRect(LX - 15, LY - 15, 30, 30, TFT_WHITE);
    tft.drawFastHLine(LX - 6, LY, 12, TFT_WHITE);
    tft.drawFastVLine(LX, LY - 6, 12, TFT_WHITE);

    // Hiển thị bảng kết quả đã dò được (cập nhật mỗi vòng)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    int tableY = H/2 + 10;
    for (int i = 0; i < sRot; i++) {
      snprintf(buf, sizeof(buf), "sRot%d -> tRot%d OK", i, discovered[i]);
      tft.setCursor(W/2 - 45, tableY + i * 12); tft.print(buf);
    }

    while (ts.touched()) delay(10);
    delay(300);

    // Chờ chạm (timeout 15s)
    int16_t vrx=0, vry=0; uint16_t vrz=0;
    bool got = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 15000) {
      if (ts.touched()) {
        delay(80);
        got = readAvgRaw(vrx, vry, vrz, 16, 2000);
        if (got) break;
      }
      delay(30);
    }

    if (!got) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(10, H/2 + 80); tft.print("[LOI] Timeout - bo qua rotation nay");
      Serial.printf("[WARN] RotDisc: timeout sRot=%d\n", sRot);
      allDone = false;
      delay(1500);
      continue;
    }

    // ─── Thử cả 4 touchRotation với 1 lần chạm ──────────────────
    Serial.printf("[RotDisc] sRot=%d, raw(%d,%d) -> thu 4 touchRotation:\n",
                  sRot, vrx, vry);

    float bestErr = 9999.f;
    int   bestTRot = 0;

    for (int tRot = 0; tRot < 4; tRot++) {
      int16_t cx=0, cy=0;
      applyTouchTransform((float)vrx, (float)vry, (uint8_t)tRot, touchCalib, cx, cy);
      float ex = (float)(cx - LX);
      float ey = (float)(cy - LY);
      float err = sqrt(ex*ex + ey*ey);
      Serial.printf("         tRot=%d -> pixel(%d,%d), sai_so=%.1fpx\n",
                    tRot, cx, cy, err);
      if (err < bestErr) { bestErr = err; bestTRot = tRot; }
    }

    discovered[sRot] = (uint8_t)bestTRot;
    Serial.printf("[RotDisc] sRot=%d: bestTouchRot=%d (err=%.1fpx)\n",
                  sRot, bestTRot, bestErr);

    // Cảnh báo nếu kết quả mơ hồ (best error > 20px — landmark có thể chưa đủ lệch)
    if (bestErr > 30.f) {
      Serial.printf("[WARN] RotDisc: sai so %.1f > 30px, ket qua co the khong tin cay!\n",
                    bestErr);
    }

    // Hiển thị kết quả vừa dò được
    snprintf(buf, sizeof(buf), "sRot%d -> tRot%d (sai so %.0fpx)", sRot, bestTRot, bestErr);
    tft.setTextColor(bestErr < 20 ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    tft.setCursor(W/2 - 60, H/2 + 70); tft.print(buf);
    delay(800);
  }

  // ─── Hiển thị bảng đầy đủ và hỏi xác nhận ─────────────────
  applyScreenRotation(0);  // về rotation 0 để hiển thị bảng
  int W2 = tft.width(), H2 = tft.height();
  (void)H2;  // dùng trong phần confirm bên dưới

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10); tft.print("Ket qua do touchRotation:");
  tft.setCursor(10, 22); tft.print("-------------------------------");

  for (int i = 0; i < 4; i++) {
    char buf[48];
    snprintf(buf, sizeof(buf), "screenRot %d -> touchRot %d %s",
             i, discovered[i],
             discovered[i] == 0xFF ? "(skip)" :
             (discovered[i] == recommendedTouchRot[i] ? "(khop default)" : "(KHAC default!)"));
    tft.setCursor(10, 35 + i * 14);
    tft.setTextColor(discovered[i] == 0xFF ? TFT_RED :
                     (discovered[i] == recommendedTouchRot[i] ? TFT_GREEN : TFT_YELLOW),
                     TFT_BLACK);
    tft.print(buf);
    Serial.printf("[RotDisc] Bang ket qua: sRot%d -> tRot%d\n", i, discovered[i]);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 100); tft.print("Luu lam mac dinh moi?");
  drawBtn(10,  120, 90, 30, "Luu NVS",    CALIB_BTN_COLOR, TFT_WHITE, 1);
  drawBtn(130, 120, 90, 30, "Bo qua",     BACK_BTN_COLOR, TFT_WHITE, 1);

  // Chờ xác nhận
  while (ts.touched()) delay(10);
  bool saved = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 20000) {
    TouchPoint tp = touchRead();
    if (tp.touched) {
      if (inRect(tp.x, tp.y, 10, 120, 90, 30)) {
        // Lưu NVS
        for (int i = 0; i < 4; i++) {
          if (discovered[i] != 0xFF) recommendedTouchRot[i] = discovered[i];
        }
        savePrefs();
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 165); tft.print("Da luu!");
        Serial.printf("[RotDisc] Da luu bang khuyen nghi moi vao NVS\n");
        saved = true;
        delay(1500);
        break;
      } else if (inRect(tp.x, tp.y, 130, 120, 90, 30)) {
        Serial.printf("[RotDisc] Nguoi dung bo qua luu bang moi\n");
        delay(500);
        break;
      }
    }
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='y'||c=='Y') {
        for (int i = 0; i < 4; i++) {
          if (discovered[i] != 0xFF) recommendedTouchRot[i] = discovered[i];
        }
        savePrefs();
        Serial.printf("[RotDisc] Da luu (qua Serial)\n");
        saved = true; break;
      } else if (c=='n'||c=='N') {
        Serial.printf("[RotDisc] Bo qua (qua Serial)\n"); break;
      } else {
        handleGlobalSerial(c);
      }
    }
    delay(30);
  }

  if (!saved) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(10, 165); tft.print("(Timeout - giu nguyen cu)");
    delay(1000);
  }

  Serial.printf("[RotDisc] Ket thuc\n");
}

