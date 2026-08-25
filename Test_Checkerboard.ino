// ═══════════════════════════════════════════════════════════════
//  Test_Checkerboard.ino — Module 4.19
//  Checkerboard đa màu, đa mật độ (1-5px), đảo pha
//  Nguyên lý: nếu đốm sáng giữ nguyên vị trí sau đảo pha
//  → lỗi phần cứng thật; nếu biến mất → chỉ là ảo giác pattern
// ═══════════════════════════════════════════════════════════════
#include "Common.h"

// 7 cặp màu: màu sáng + đen làm nền tương phản
static const uint16_t CB_COLORS_A[] = {
  TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE,
  TFT_CYAN, TFT_MAGENTA, TFT_YELLOW
};
static const char* CB_COLOR_NAMES[] = {
  "Trang/Den","Do/Den","Luc/Den","Lam/Den",
  "Cyan/Den","Magenta/Den","Vang/Den"
};
static const int CB_PAIR_COUNT = 7;
static const uint16_t CB_COLOR_B = TFT_BLACK;

// Vẽ toàn màn checkerboard với cỡ ô và 2 màu cho trước
static void drawCheckerboard(int cellSize, uint16_t colA, uint16_t colB) {
  int W = tft.width(), H = tft.height();
  for (int y = 0; y < H; y += cellSize) {
    for (int x = 0; x < W; x += cellSize) {
      int cx = x / cellSize, cy = y / cellSize;
      uint16_t c = ((cx + cy) % 2 == 0) ? colA : colB;
      int rw = min(cellSize, W - x);
      int rh = min(cellSize, H - y);
      tft.fillRect(x, y, rw, rh, c);
    }
  }
}

// ─── 4.19 Checkerboard đa màu / đa mật độ / đảo pha ───────────
void runCheckerboard() {
  Serial.printf("[Checkerboard] Bat dau\n");

  int pairIdx  = 0;  // cặp màu hiện tại (0-6)
  int cellSize = 2;  // cỡ ô (1-5px)
  bool phaseInv = false; // đảo pha
  bool needRedraw = true;

  while (true) {
    if (needRedraw) {
      uint16_t cA = phaseInv ? CB_COLOR_B          : CB_COLORS_A[pairIdx];
      uint16_t cB = phaseInv ? CB_COLORS_A[pairIdx]: CB_COLOR_B;

      // Vẽ checkerboard toàn màn
      drawCheckerboard(cellSize, cA, cB);

      // HUD góc trên trái (nền đen bán trong suốt)
      tft.fillRect(0, 0, tft.width(), 20, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      char buf[48];
      snprintf(buf, sizeof(buf), "%s  %dpx  %s",
               CB_COLOR_NAMES[pairIdx], cellSize,
               phaseInv ? "[DAO PHA]" : "");
      tft.setCursor(2, 6);
      tft.print(buf);

      // Hướng dẫn góc dưới
      int H = tft.height();
      tft.fillRect(0, H - 36, tft.width(), 36, TFT_BLACK);
      tft.setTextColor(0xBDF7, TFT_BLACK);
      tft.setCursor(2, H - 34); tft.print("< Mau  Pha  Size  Mau >");
      tft.setCursor(2, H - 22); tft.print("Dao pha: xem diem co dich chuyen?");
      tft.setCursor(2, H - 11); tft.print("Giu nguyen=loi phan cung|Bien mat=ao giac");

      Serial.printf("[Checkerboard] Cap=%s cellSize=%dpx phase=%s\n",
                    CB_COLOR_NAMES[pairIdx], cellSize,
                    phaseInv ? "DAO" : "THUONG");
      needRedraw = false;
    }

    // Xử lý Serial — n/p/+/-/i là test-specific, còn lại gọi global
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') goto cbExit;
      else if (c=='n'||c=='N') { pairIdx=(pairIdx+1)%CB_PAIR_COUNT; phaseInv=false; needRedraw=true; }
      else if (c=='p'||c=='P') { pairIdx=(pairIdx-1+CB_PAIR_COUNT)%CB_PAIR_COUNT; phaseInv=false; needRedraw=true; }
      else if (c=='+') { if(cellSize<5){cellSize++;needRedraw=true;} }
      else if (c=='-') { if(cellSize>1){cellSize--;needRedraw=true;} }
      else if (c=='i'||c=='I') { phaseInv=!phaseInv; needRedraw=true; }
      else handleGlobalSerial(c);
    }

    // Xử lý chạm — chia màn thành 5 vùng ngang
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(150); // debounce
      int W = tft.width(), H = tft.height();
      int tx = tp.x, ty = tp.y;

      // Bỏ qua vùng HUD trên/dưới khi phân tích nút
      if (ty > 20 && ty < H - 36) {
        int zone = tx * 5 / W;  // 0=Mau-, 1=Size-, 2=Pha, 3=Size+, 4=Mau+
        switch (zone) {
          case 0: pairIdx = (pairIdx - 1 + CB_PAIR_COUNT) % CB_PAIR_COUNT; phaseInv = false; needRedraw = true; break;
          case 1: if (cellSize > 1) { cellSize--; needRedraw = true; } break;
          case 2: phaseInv = !phaseInv; needRedraw = true; break;
          case 3: if (cellSize < 5) { cellSize++; needRedraw = true; } break;
          case 4: pairIdx = (pairIdx + 1) % CB_PAIR_COUNT; phaseInv = false; needRedraw = true; break;
        }
      }
      if (isBackBtn(tx, ty)) break;
    }
    delay(20);
  }
  cbExit:
  Serial.printf("[Checkerboard] Ket thuc\n");
}

