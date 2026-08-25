// ═══════════════════════════════════════════════════════════════
//  Test_Extras.ino — Module 4.14: RGB/BGR Swatch
//  Xác nhận TFT_RGB_ORDER đúng hay sai bằng cách nhìn màu ô
// ═══════════════════════════════════════════════════════════════
#include "Common.h"

// ─── 4.14 RGB/BGR Swatch ───────────────────────────────────────
void runRGBSwatch() {
  Serial.printf("[RGBSwatch] Bat dau\n");
  int W = tft.width(), H = tft.height();

  tft.fillScreen(TFT_BLACK);
  drawBackBtn();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(60, 4);
  tft.print("4.14 RGB/BGR Swatch");

  // 8 mau: R,G,B,C,M,Y,W,K — moi o 50x50, 4 tren 4 duoi
  struct Swatch { const char* label; uint16_t color; };
  const Swatch sw[] = {
    { "R",  TFT_RED     }, { "G",  TFT_GREEN   },
    { "B",  TFT_BLUE    }, { "C",  TFT_CYAN    },
    { "M",  TFT_MAGENTA }, { "Y",  TFT_YELLOW  },
    { "W",  TFT_WHITE   }, { "K",  TFT_BLACK   },
  };

  int cellW = W / 4, cellH = (H - 50) / 2;
  int startY = 28;

  for (int i = 0; i < 8; i++) {
    int col = i % 4, row = i / 4;
    int x = col * cellW, y = startY + row * cellH;
    tft.fillRect(x, y, cellW - 2, cellH - 2, sw[i].color);
    tft.drawRect(x, y, cellW - 2, cellH - 2, TFT_DARKGREY);
    tft.setTextColor(fg(sw[i].color), sw[i].color);
    tft.setTextSize(2);
    tft.setCursor(x + cellW/2 - 6, y + cellH/2 - 8);
    tft.print(sw[i].label);
  }

  // Huong dan
  int instrY = startY + cellH * 2 + 4;  // gap nho hon de khong bi cat
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4, instrY);
  tft.print("O 'R' phai hien DO thuc su.");
  tft.setCursor(4, instrY + 12);
  tft.print("O 'B' phai hien XANH LAM.");
  tft.setCursor(4, instrY + 24);
  tft.print("Neu R=xanh/B=do: doi RGB_ORDER");
  tft.setCursor(4, instrY + 36);
  tft.print("trong User_Setup.h roi nan lai.");

  // Kiem tra chi tiet (in ra Serial de khong bi cat man)
  Serial.printf("[RGBSwatch] Kiem tra: o 'R' phai hien DO (0xF800), o 'B' phai hien XANH (0x001F)\n");

  Serial.printf("[RGBSwatch] Hien thi 8 o mau: R/G/B/C/M/Y/W/K\n");
  Serial.printf("[RGBSwatch] Kiem tra: o 'R' phai hien DO (0xF800), o 'B' phai hien XANH (0x001F)\n");

  // Chờ Back — có đầy đủ serial fallback
  while (true) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') goto swatchExit;
      handleGlobalSerial(c);
    }
    TouchPoint tp = touchRead();
    if (tp.touched && isBackBtn(tp.x, tp.y)) break;
    delay(20);
  }
  swatchExit:
  Serial.printf("[RGBSwatch] Ket thuc\n");
}
