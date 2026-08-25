// ═══════════════════════════════════════════════════════════════
//  Test_Visual.ino — Modules hiển thị (Phase 3)
//  4.1 Test Pattern  4.2 Defective Pixels  4.3 Uniformity
//  4.4 Color Distance  4.5 Gradient  4.6 Sharpness
//  4.7 Viewing Angle  4.8 Gamma
//  NOTE: handleGlobalSerial() định nghĩa trong TFT_Test_Suite.ino
//  (Arduino IDE gộp tất cả .ino vào 1 unit, không cần extern)
// ═══════════════════════════════════════════════════════════════
#include "Common.h"


// ─── Helper: chờ Back + poll input ─────────────────────────────
// Trả về true = Back, false = Next
// handleGlobalSerial() luôn được gọi cho mọi ký tự không phải b/q/n
static bool waitBackOrNext(bool hasNext = false) {
  while (true) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') return true;
      if (hasNext && (c=='n'||c=='N'||c==' ')) return false;
      // Lệnh toàn cục: r/R/t/T/w/0/c/x/d/+/-/?
      handleGlobalSerial(c);
    }
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(120);
      if (isBackBtn(tp.x, tp.y)) return true;
      if (hasNext) return false;
    }
    delay(20);
  }
}


// ─── Helper: vẽ nút nhỏ hàng dưới màn ─────────────────────────
static void drawBottomBar(const char* left, const char* mid, const char* right) {
  int W = tft.width(), H = tft.height();
  int y = H - 22, bh = 20, bw = W / 3 - 3;
  tft.fillRect(0, y - 2, W, 24, TFT_BLACK);
  if (left)  drawBtn(1,        y, bw, bh, left,  BACK_BTN_COLOR, TFT_WHITE, 1);
  if (mid)   drawBtn(W/2-bw/2, y, bw, bh, mid,   NAV_BTN_COLOR,  TFT_WHITE, 1);
  if (right) drawBtn(W-bw-1,   y, bw, bh, right, NAV_BTN_COLOR,  TFT_WHITE, 1);
}

// ═══════════════════════════════════════════════════════════════
//  4.1 — Test Pattern (EIZO-style, 4-screen cycle)
//
//  Screen 0: OVERVIEW  — tất cả phần tử thu nhỏ
//  Screen 1: BORDER    — grid + checkerboard border + corner circles
//  Screen 2: LINES     — 4 quadrant frequency patterns
//  Screen 3: CIRCLE    — color circle full screen
//
//  Điều hướng: tap bất kỳ (trừ góc Back) = next screen
//              chạm góc trên-trái (isBackBtn) = thoát về menu
//              Serial: n/N/space = next  b/B/q/Q = back
// ═══════════════════════════════════════════════════════════════

// ── Màu chuẩn từ SVG/PNG gốc ──────────────────────────────────
#define TP_GRID_FILL  tft.color565(226, 226, 226)  // #E2E2E2
#define TP_GRID_LINE  tft.color565(68,  68,  68)   // #444444
#define TP_CIRCLE_BG  tft.color565(68,  68,  68)   // #444444

// ── Primitive: Background grid ─────────────────────────────────
// Đổ nền xám nhạt #E2E2E2, vẽ lưới ô tileSize px màu #444444
static void tp_drawBackgroundGrid(int tileSize) {
  int W = tft.width(), H = tft.height();
  tft.fillScreen(TP_GRID_FILL);
  for (int y = 0; y <= H; y += tileSize)
    tft.drawFastHLine(0, y, W, TP_GRID_LINE);
  for (int x = 0; x <= W; x += tileSize)
    tft.drawFastVLine(x, 0, H, TP_GRID_LINE);
}

// ── Primitive: Border checkerboard ─────────────────────────────
// Viền checkerboard đen/trắng, barW px dày, ô tileSize px
// Vẽ trên nền đã có (gọi sau drawBackgroundGrid)
static void tp_drawBorderCheckerboard(int barW, int tileSize) {
  int W = tft.width(), H = tft.height();

  // Lambda: vẽ một dải ô, dx/dy là bước tiến của mỗi ô
  auto drawStrip = [&](int x0, int y0, int dx, int dy, int count, int parity0) {
    for (int i = 0; i < count; i++) {
      uint16_t col = (((i + parity0) % 2) == 0) ? TFT_BLACK : TFT_WHITE;
      int x = x0 + i * dx;
      int y = y0 + i * dy;
      int bw = (dx > 0) ? min(tileSize, W - x) : barW;
      int bh = (dy > 0) ? min(tileSize, H - y) : barW;
      if (bw > 0 && bh > 0) tft.fillRect(x, y, bw, bh, col);
    }
  };

  int nH = (W + tileSize - 1) / tileSize + 1;
  int nV = (H + tileSize - 1) / tileSize - 1;

  // Top bar: y=0, ô chẵn=đen
  drawStrip(0, 0, tileSize, 0, nH, 0);
  // Bottom bar: y=H-barW, ô chẵn=đen
  // Căn ô dưới sao cho corner khớp màu (parity theo cột)
  int botParity = ((H / tileSize) % 2 == 0) ? 0 : 1;
  drawStrip(0, H - barW, tileSize, 0, nH, botParity);
  // Left bar: bắt đầu từ y=barW (sau top bar), ô theo hàng
  for (int j = 0; j < nV; j++) {
    int y = barW + j * tileSize;
    if (y + tileSize > H - barW) break;
    uint16_t col = ((j % 2) == 0) ? TFT_WHITE : TFT_BLACK;
    tft.fillRect(0, y, barW, min(tileSize, H - barW - y), col);
  }
  // Right bar
  for (int j = 0; j < nV; j++) {
    int y = barW + j * tileSize;
    if (y + tileSize > H - barW) break;
    uint16_t col = ((j % 2) == 0) ? TFT_BLACK : TFT_WHITE;
    tft.fillRect(W - barW, y, barW, min(tileSize, H - barW - y), col);
  }
}

// ── Primitive: Corner circle với crosshair ─────────────────────
// Khớp testscreen_edge.svg: fillCircle trắng, stroke đen 1px,
// crosshair 2 đường: arm = 40% bán kính mỗi phía
static void tp_drawCornerCircle(int cx, int cy, int r) {
  tft.fillCircle(cx, cy, r, TFT_WHITE);
  tft.drawCircle(cx, cy, r, TFT_BLACK);
  int arm = max(3, r * 2 / 5);  // 40% bán kính (tỉ lệ SVG: 40px/r=49.5px)
  tft.drawFastHLine(cx - arm, cy, 2 * arm + 1, TFT_BLACK);
  tft.drawFastVLine(cx, cy - arm, 2 * arm + 1, TFT_BLACK);
}

// ── Primitive: Linetest box (4 quadrants) ──────────────────────
// Layout theo HTML float order (spec v2 ĐÚNG):
//   TL = line_v1 : sọc DỌC  1px đen/trắng
//   TR = line_v2 : sọc DỌC  2px đen/trắng
//   BL = line_h1 : sọc NGANG 1px đen/trắng
//   BR = line_h2 : sọc NGANG 2px đen/trắng
static void tp_drawLinetestBox(int x, int y, int sz) {
  int half = sz / 2;
  tft.fillRect(x, y, sz, sz, TFT_WHITE);

  // TL: sọc dọc 1px đen / 1px trắng
  for (int px = x; px < x + half; px += 2)
    tft.drawFastVLine(px, y, half, TFT_BLACK);

  // TR: sọc dọc 2px đen / 2px trắng
  for (int px = x + half; px < x + sz; px += 4) {
    tft.drawFastVLine(px,     y, half, TFT_BLACK);
    if (px + 1 < x + sz)
      tft.drawFastVLine(px + 1, y, half, TFT_BLACK);
  }

  // BL: sọc ngang 1px đen / 1px trắng
  for (int py = y + half; py < y + sz; py += 2)
    tft.drawFastHLine(x, py, half, TFT_BLACK);

  // BR: sọc ngang 2px đen / 2px trắng
  for (int py = y + half; py < y + sz; py += 4) {
    tft.drawFastHLine(x + half, py,     half, TFT_BLACK);
    if (py + 1 < y + sz)
      tft.drawFastHLine(x + half, py + 1, half, TFT_BLACK);
  }

  // Viền + đường phân cách quadrant (màu xám trung tính)
  uint16_t sep = tft.color565(160, 160, 160);
  tft.drawRect(x, y, sz, sz, sep);
  tft.drawFastHLine(x, y + half, sz, sep);
  tft.drawFastVLine(x + half, y, sz, sep);
}

// ── Primitive: Gradient bar (trái→phải) ────────────────────────
static void tp_drawGradientBar(int x, int y, int w, int h,
                               uint32_t from, uint32_t to) {
  if (w <= 0) return;
  for (int i = 0; i < w; i++) {
    float t = (w > 1) ? (float)i / (w - 1) : 0.0f;
    uint8_t r = (uint8_t)(((from >> 16) & 0xFF) * (1.0f - t) + ((to >> 16) & 0xFF) * t);
    uint8_t g = (uint8_t)(((from >>  8) & 0xFF) * (1.0f - t) + ((to >>  8) & 0xFF) * t);
    uint8_t b = (uint8_t)(( from        & 0xFF) * (1.0f - t) + ( to        & 0xFF) * t);
    tft.drawFastVLine(x + i, y, h, tft.color565(r, g, b));
  }
}

// ── Primitive: Color circle (đầy đủ bands + gradients) ─────────
// Khớp spec v2 Section 3 / 5.2:
//   Nền: #444444
//   Band %: R(3%–11%) G(11%–19%) B(19%–27%) | White(30%–60%) | gap(60%–64%)
//           C(64%–72%) M(72%–80%) Y(80%–88%) | K(còn lại)
//   White area chứa 4 gradient bars (Blue→Blk, Green→Blk, Red→Blk, White→Blk)
static void tp_drawColorCircle(int cx, int cy, int r) {
  if (r < 4) return;

  // Helper: clip horizontal line vào đường tròn
  auto hlineClip = [&](int iy, int x0, int x1, uint16_t col) {
    float dy = (float)(iy - cy);
    if (fabsf(dy) > r) return;
    int dx = (int)sqrtf((float)(r * r) - dy * dy);
    x0 = max(x0, cx - dx);
    x1 = min(x1, cx + dx);
    if (x1 > x0) tft.drawFastHLine(x0, iy, x1 - x0, col);
  };

  int D  = 2 * r;
  int top = cy - r;
  int W  = tft.width();

  // 1. Nền circle: #444444
  tft.fillCircle(cx, cy, r, TP_CIRCLE_BG);

  // 2. Color bands (theo % đường kính từ trên xuống)
  // Yellow kéo dài đến 100% để tránh padding xám ở đáy
  struct Band { float yPct; float hPct; uint16_t col; };
  Band bands[] = {
    { 0.03f, 0.08f, tft.color565(255,   0,   0) },  // Red
    { 0.11f, 0.08f, tft.color565(  0, 255,   0) },  // Green
    { 0.19f, 0.08f, tft.color565(  0,   0, 255) },  // Blue
    // White area: 0.30–0.60 (xử lý riêng bên dưới)
    // gap 0.60–0.64: giữ màu nền (không vẽ)
    { 0.64f, 0.08f, tft.color565(  0, 255, 255) },  // Cyan
    { 0.72f, 0.08f, tft.color565(255,   0, 255) },  // Magenta
    { 0.80f, 0.20f, tft.color565(255, 255,   0) },  // Yellow — kéo đến 100% (0.80+0.20)
  };
  for (auto& b : bands) {
    int yS = top + (int)(b.yPct * D);
    int yE = top + (int)((b.yPct + b.hPct) * D);
    for (int iy = yS; iy < yE; iy++)
      hlineClip(iy, 0, W, b.col);
  }

  // 3. White area (30%–60%)
  {
    int yWS = top + (int)(0.30f * D);
    int yWE = top + (int)(0.60f * D);
    for (int iy = yWS; iy < yWE; iy++)
      hlineClip(iy, 0, W, TFT_WHITE);

    // 4. Gradient bars trong white area
    // Container: width = r (50% đường kính), căn giữa ngang
    // Chiều cao: 80% white area, padding top 10%
    int gcW = r;
    int gcH = (int)((yWE - yWS) * 0.80f);
    int gcX = cx - gcW / 2;
    int gcY = yWS + (int)((yWE - yWS) * 0.10f);
    int barH = max(2, gcH / 4);
    // Blue → Black
    tp_drawGradientBar(gcX, gcY,          gcW, barH, 0x0000FF, 0x000000);
    // Green → Black
    tp_drawGradientBar(gcX, gcY + barH,   gcW, barH, 0x00FF00, 0x000000);
    // Red → Black
    tp_drawGradientBar(gcX, gcY + barH*2, gcW, barH, 0xFF0000, 0x000000);
    // White → Black
    tp_drawGradientBar(gcX, gcY + barH*3, gcW, barH, 0xFFFFFF, 0x000000);
  }

  // 5. Labels R G B (trắng trên nền tối) + C M Y (đen trên nền sáng)
  tft.setTextSize(1);
  struct Label { const char* t; float yPct; uint16_t bg; bool darkText; };
  Label labels[] = {
    { "R", 0.03f + 0.04f, tft.color565(255,   0,   0), false },  // trắng trên đỏ
    { "G", 0.11f + 0.04f, tft.color565(  0, 255,   0), false },  // trắng trên lục
    { "B", 0.19f + 0.04f, tft.color565(  0,   0, 255), false },  // trắng trên lam
    { "C", 0.64f + 0.04f, tft.color565(  0, 255, 255), true  },  // đen trên cyan sáng
    { "M", 0.72f + 0.04f, tft.color565(255,   0, 255), true  },  // đen trên magenta sáng
    { "Y", 0.80f + 0.10f, tft.color565(255, 255,   0), true  },  // đen trên yellow sáng
  };
  for (auto& lb : labels) {
    int ly = top + (int)(lb.yPct * D);
    tft.setTextColor(lb.darkText ? TFT_BLACK : TFT_WHITE, lb.bg);
    tft.setCursor(cx - 2, ly - 4);
    tft.print(lb.t);
  }
}

// ── Screen 0: OVERVIEW ─────────────────────────────────────────
static void tp_drawOverview() {
  int W = tft.width(), H = tft.height();
  const int TILE = 8;

  tp_drawBackgroundGrid(TILE);
  tp_drawBorderCheckerboard(TILE, TILE);

  // 4 corner circles (r = TILE)
  int cr = TILE;
  tp_drawCornerCircle(cr,     cr,     cr);
  tp_drawCornerCircle(W-cr-1, cr,     cr);
  tp_drawCornerCircle(cr,     H-cr-1, cr);
  tp_drawCornerCircle(W-cr-1, H-cr-1, cr);

  // 2 linetest boxes (trái + phải)
  int lsz = max(20, min(W, H) / 5);
  int lx_l = TILE * 3;
  int lx_r = W - TILE * 3 - lsz;
  int ly   = (H - lsz) / 2;
  tp_drawLinetestBox(lx_l, ly, lsz);
  tp_drawLinetestBox(lx_r, ly, lsz);

  // Color circle ở giữa
  int availW = lx_r - (lx_l + lsz) - TILE * 2;
  int availH = H - TILE * 4;
  int r = min(availW, availH) / 2;
  r = max(r, 16);
  tp_drawColorCircle(W / 2, H / 2, r);

  // Label nhỏ (góc trên-phải để không che checkerboard)
  tft.setTextColor(TFT_BLACK, TP_GRID_FILL);
  tft.setTextSize(1);
  char buf[32];
  snprintf(buf, sizeof(buf), "0/3 OV %dx%d", W, H);
  tft.setCursor(W - strlen(buf) * 6 - 2, 2);
  tft.print(buf);
}

// ── Screen 1: BORDER + GRID + CORNERS ──────────────────────────
static void tp_drawBorderScreen() {
  int W = tft.width(), H = tft.height();
  const int TILE = 10;
  const int BAR  = TILE;

  tp_drawBackgroundGrid(TILE);
  tp_drawBorderCheckerboard(BAR, TILE);

  // 4 corner circles — r = BAR + 4 để thấy rõ
  int r = BAR + 4;
  tp_drawCornerCircle(r,     r,     r);
  tp_drawCornerCircle(W-r-1, r,     r);
  tp_drawCornerCircle(r,     H-r-1, r);
  tp_drawCornerCircle(W-r-1, H-r-1, r);

  // Label
  tft.setTextColor(TFT_BLACK, TP_GRID_FILL);
  tft.setTextSize(1);
  tft.setCursor(W/2 - 30, H/2 - 4);
  tft.print("1/3 BORDER");
}

// ── Screen 2: FREQUENCY LINES (4 quadrants full screen) ────────
//   TL: sọc dọc  1px   TR: sọc dọc  2px
//   BL: sọc ngang 1px  BR: sọc ngang 2px
static void tp_drawLinesScreen() {
  int W = tft.width(), H = tft.height();
  int hw = W / 2, hh = H / 2;

  // TL: sọc dọc 1px đen / 1px trắng
  tft.fillRect(0, 0, hw, hh, TFT_WHITE);
  for (int x = 0; x < hw; x += 2)
    tft.drawFastVLine(x, 0, hh, TFT_BLACK);

  // TR: sọc dọc 2px đen / 2px trắng
  tft.fillRect(hw, 0, W - hw, hh, TFT_WHITE);
  for (int x = hw; x < W; x += 4) {
    tft.drawFastVLine(x,     0, hh, TFT_BLACK);
    if (x + 1 < W) tft.drawFastVLine(x + 1, 0, hh, TFT_BLACK);
  }

  // BL: sọc ngang 1px đen / 1px trắng
  tft.fillRect(0, hh, hw, H - hh, TFT_WHITE);
  for (int y = hh; y < H; y += 2)
    tft.drawFastHLine(0, y, hw, TFT_BLACK);

  // BR: sọc ngang 2px đen / 2px trắng
  tft.fillRect(hw, hh, W - hw, H - hh, TFT_WHITE);
  for (int y = hh; y < H; y += 4) {
    tft.drawFastHLine(hw, y,     W - hw, TFT_BLACK);
    if (y + 1 < H) tft.drawFastHLine(hw, y + 1, W - hw, TFT_BLACK);
  }

  // Đường chia giữa màn + labels
  uint16_t divCol = tft.color565(128, 128, 128);
  tft.drawFastHLine(0,  hh, W, divCol);
  tft.drawFastVLine(hw, 0,  H, divCol);

  // Labels trên nền trắng bán trong suốt (dùng màu đỏ để tương phản)
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4,    2);    tft.print("V 1px");
  tft.setCursor(hw+4, 2);    tft.print("V 2px");
  tft.setCursor(4,    hh+2); tft.print("H 1px");
  tft.setCursor(hw+4, hh+2); tft.print("H 2px");

  // Screen number
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.setCursor(W - 36, 2); tft.print("2/3");
}

// ── Screen 3: COLOR CIRCLE (full screen) ───────────────────────
static void tp_drawCircleScreen() {
  int W = tft.width(), H = tft.height();
  tft.fillScreen(TP_GRID_FILL);  // nền #E2E2E2
  int r = min(W, H) / 2 - 4;
  tp_drawColorCircle(W / 2, H / 2, r);

  // Screen number (trên nền xám)
  tft.setTextColor(TFT_BLACK, TP_GRID_FILL);
  tft.setTextSize(1);
  tft.setCursor(2, 2); tft.print("3/3 COLOR");
}

// ── Main entry point ────────────────────────────────────────────
void runTestPattern() {
  Serial.printf("[TestPattern] Bat dau — 4-screen cycle (tap=next, back=exit)\n");
  Serial.printf("[TestPattern] Serial: n/space=next  b/q=thoat\n");

  int screen = 0;  // 0=Overview, 1=Border, 2=Lines, 3=Circle
  bool needRedraw = true;

  while (true) {
    if (needRedraw) {
      switch (screen) {
        case 0: tp_drawOverview();     break;
        case 1: tp_drawBorderScreen(); break;
        case 2: tp_drawLinesScreen();  break;
        case 3: tp_drawCircleScreen(); break;
      }
      Serial.printf("[TestPattern] Screen %d/3\n", screen);
      needRedraw = false;
    }

    // Poll serial
    bool quit = false;
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == 'b' || c == 'B' || c == 'q' || c == 'Q') { quit = true; break; }
      if (c == 'n' || c == 'N' || c == ' ') {
        screen = (screen + 1) % 4;
        needRedraw = true;
        break;
      }
      handleGlobalSerial(c);
    }
    if (quit) break;
    if (needRedraw) continue;

    // Poll touch
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(120);
      if (isBackBtn(tp.x, tp.y)) break;
      // Chạm bất kỳ vị trí khác = sang màn tiếp theo
      screen = (screen + 1) % 4;
      needRedraw = true;
    }
    delay(20);
  }

  Serial.printf("[TestPattern] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.2 — Defective Pixels (5 field màu)
// ═══════════════════════════════════════════════════════════════
void runDefectivePixels() {
  Serial.printf("[DefectPixels] Bat dau\n");

  struct Field { const char* name; uint16_t color; const char* hint; };
  const Field fields[] = {
    { "DEN",   TFT_BLACK,   "Diem sang len = sub-pixel ket sang (stuck-on)" },
    { "TRANG", TFT_WHITE,   "Diem toi = pixel chet hoan toan"               },
    { "DO",    TFT_RED,     "Diem toi = sub-pixel DO bi chet rieng le"      },
    { "LUC",   TFT_GREEN,   "Diem toi = sub-pixel LUC bi chet rieng le"     },
    { "LAM",   TFT_BLUE,    "Diem toi = sub-pixel LAM bi chet rieng le"     },
  };
  const int NF = 5;
  int fi = 0;

  while (true) {
    uint16_t bg = fields[fi].color;
    tft.fillScreen(bg);

    // Thông tin góc — dùng fg() để màu chữ tương phản
    tft.setTextColor(fg(bg), bg);
    tft.setTextSize(1);
    int W = tft.width(), H = tft.height();

    // Tên field + số thứ tự (góc trên)
    char buf[32];
    snprintf(buf, sizeof(buf), "4.2 %s  %d/%d", fields[fi].name, fi+1, NF);
    tft.setCursor(2, 2); tft.print(buf);

    // Hướng dẫn pass/fail (góc dưới)
    tft.setCursor(2, H - 30); tft.print(fields[fi].hint);
    tft.setCursor(2, H - 18); tft.print("Soi ky - mat thuong o 20-30cm");
    tft.setCursor(2, H -  7); tft.print("Cham/N=tiep | B=quay lai");

    Serial.printf("[DefectPixels] Field %d/%d: %s (0x%04X)\n",
                  fi+1, NF, fields[fi].name, bg);

    // Chờ input
    bool quit = false;
    while (true) {
      bool changed = false;
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c=='b'||c=='B'||c=='q'||c=='Q') { quit=true; break; }
        else if (c=='n'||c=='N'||c==' ') { fi=(fi+1)%NF; changed=true; break; }
        else if (c=='p'||c=='P') { fi=(fi-1+NF)%NF; changed=true; break; }
        else handleGlobalSerial(c);
      }
      if(quit||changed) break;  // thoát inner để redraw hoặc quit
      TouchPoint tp = touchRead();
      if (tp.touched) {
        delay(120);
        if (isBackBtn(tp.x, tp.y)) { quit=true; break; }
        // Chạm nửa trái = Prev, nửa phải = Next
        if (tp.x < tft.width()/2) fi = (fi-1+NF)%NF;
        else                       fi = (fi+1)%NF;
        break;
      }
      delay(25);
    }
    if (quit) break;
  }
  Serial.printf("[DefectPixels] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.3 — Uniformity (đồng đều sáng)
// ═══════════════════════════════════════════════════════════════
void runUniformity() {
  Serial.printf("[Uniformity] Bat dau\n");

  // 6 mức xám: 0(đen),10,25,50,75,90%
  const uint8_t levels[] = { 0, 26, 64, 128, 191, 230 };
  const char*   labels[] = { "0%", "10%", "25%", "50%", "75%", "90%" };
  const int NL = 6;
  int li = 0;

  while (true) {
    uint8_t v = levels[li];
    uint16_t bg = tft.color565(v, v, v);
    tft.fillScreen(bg);

    tft.setTextColor(fg(bg), bg);
    tft.setTextSize(1);
    int W = tft.width(), H = tft.height();

    char buf[32];
    snprintf(buf, sizeof(buf), "4.3 Dong Deu  %s  %d/%d", labels[li], li+1, NL);
    tft.setCursor(2, 2); tft.print(buf);

    if (li == 0) {
      // Field đen: nhắc tắt đèn phòng
      tft.setCursor(2, H - 40);
      tft.print("** Tat het den phong **");
      tft.setCursor(2, H - 28);
      tft.print("Soi hor sang (backlight bleed)");
      tft.setCursor(2, H - 16);
      tft.print("Ro nhat o phong toi hoan toan");
    } else {
      tft.setCursor(2, H - 16);
      tft.print("Tim vung am mau/chenh sang bau");
    }
    tft.setCursor(2, H - 5);
    tft.print("N=tiep | P=lui | B=quay lai");

    Serial.printf("[Uniformity] Muc %d/%d: %s (v=%d, 0x%04X)\n",
                  li+1, NL, labels[li], v, bg);

    bool quit = false;
    while (true) {
      bool changed = false;
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c=='b'||c=='B'||c=='q'||c=='Q') { quit=true; break; }
        else if (c=='n'||c=='N'||c==' ') { li=(li+1)%NL; changed=true; break; }
        else if (c=='p'||c=='P') { li=(li-1+NL+NL)%NL; changed=true; break; }
        else handleGlobalSerial(c);
      }
      if(quit||changed) break;  // thoát inner để redraw hoặc quit
      TouchPoint tp = touchRead();
      if (tp.touched) {
        delay(120);
        if (isBackBtn(tp.x, tp.y)) { quit=true; break; }
        if (tp.x < tft.width()/2) li=(li-1+NL)%NL;
        else                       li=(li+1)%NL;
        break;
      }
      delay(25);
    }
    if (quit) break;
  }
  Serial.printf("[Uniformity] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.4 — Color Distance (độ phân giải màu)
//  Portrait : 2 hàng nút dưới, nút cao hơn
//  Landscape: panel nút 1/3 phải màn hình
// ═══════════════════════════════════════════════════════════════
void runColorDistance() {
  Serial.printf("[ColorDist] Bat dau\n");
  int bg_r=15, bg_g=31, bg_b=15;
  int rc_r=1,  rc_g=1,  rc_b=1;

  auto make565 = [](int r, int g, int b) -> uint16_t {
    r=constrain(r,0,31); g=constrain(g,0,63); b=constrain(b,0,31);
    return ((uint16_t)r<<11)|((uint16_t)g<<5)|(uint16_t)b;
  };

  bool needRedraw = true;

  // Lưu layout để touch handler dùng đúng tọa độ
  // Portrait: panH=70, 2 hàng nút, bh=30
  // Landscape: panX=W*2/3, 6 hàng × 2 cột
  struct Layout {
    bool landscape;
    int panX, panH;    // landscape: panX; portrait: panH
    int bh;            // chiều cao mỗi nút
    int bw;            // portrait: W/6; landscape: panW/2-2
    int row1y, row2y;  // portrait only
  } lay = {};

  while (true) {
    if (needRedraw) {
      int W=tft.width(), H=tft.height();
      lay.landscape=(W>H);
      uint16_t bgColor  =make565(bg_r,bg_g,bg_b);
      uint16_t rectColor=make565(bg_r+rc_r,bg_g+rc_g,bg_b+rc_b);

      tft.fillScreen(bgColor);

      if (lay.landscape) {
        // ─── Landscape: nút panel chiếm 1/3 phải ─────────────
        lay.panX = W*2/3;
        int panW = W - lay.panX;
        tft.fillRect(lay.panX, 0, panW, H, TFT_BLACK);

        // Hình chữ nhật nhỏ hơn (phần còn lại bên trái)
        int mainW = lay.panX - 4;
        int rw = mainW*3/5, rh = (H-32)*3/5;
        int rx2 = (mainW-rw)/2, ry2 = 32+(H-32-rh)/2;
        tft.fillRect(rx2, ry2, rw, rh, rectColor);

        // HUD top
        tft.fillRect(0, 0, mainW, 28, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(2, 2);  tft.print("4.4 Phan Giai Mau");
        char buf[48];
        snprintf(buf, sizeof(buf), "Nen R%d G%d B%d | +R%d +G%d +B%d",
                 bg_r,bg_g,bg_b,rc_r,rc_g,rc_b);
        tft.setCursor(2, 16); tft.print(buf);

        // Panel nút (1/3 phải): 6 hàng × 2 cột (-/+)
        // Hàng 0-2 = nền R/G/B ; hàng 3-5 = rect R/G/B
        const char* lblA[] = {"-Rb","-Gb","-Bb","-Rr","-Gr","-Br"};
        const char* lblB[] = {"Rb+","Gb+","Bb+","Rr+","Gr+","Br+"};
        lay.bw = panW/2 - 2;
        lay.bh = (H - 6) / 6;
        for (int row = 0; row < 6; row++) {
          uint16_t bc = (row < 3) ? MENU_BTN_COLOR : NAV_BTN_COLOR;
          int ry = 2 + row * (lay.bh + 1);
          drawBtn(lay.panX + 1,          ry, lay.bw, lay.bh, lblA[row], bc, TFT_WHITE, 1);
          drawBtn(lay.panX + lay.bw + 3, ry, lay.bw, lay.bh, lblB[row], bc, TFT_WHITE, 1);
        }
      } else {
        // ─── Portrait: 2 hàng × 6 nút dưới, nút cao hơn ───────
        lay.panH = 70;  // panel cao hơn để nút dễ bấm
        int rw = W*3/5, rh = (H-32-lay.panH)*3/5;
        int rx2 = (W-rw)/2, ry2 = 32 + (H-32-lay.panH-rh)/2;
        tft.fillRect(rx2, ry2, rw, rh, rectColor);

        // HUD top
        tft.fillRect(0, 0, W, 28, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(2, 2);  tft.print("4.4 Phan Giai Mau");
        char buf[48];
        snprintf(buf, sizeof(buf), "Nen R%d G%d B%d | +R%d +G%d +B%d",
                 bg_r,bg_g,bg_b,rc_r,rc_g,rc_b);
        tft.setCursor(2, 16); tft.print(buf);

        // 2 hàng × 6 nút — nút cao 30px, khoảng cách 2px
        tft.fillRect(0, H-lay.panH, W, lay.panH, TFT_BLACK);
        lay.bw = W/6 - 2;
        lay.bh = 30;
        lay.row1y = H - lay.panH + 4;
        lay.row2y = H - lay.panH + 36;
        drawBtn(0,            lay.row1y, lay.bw, lay.bh, "-Rb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(lay.bw+2,     lay.row1y, lay.bw, lay.bh, "-Gb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(lay.bw*2+4,   lay.row1y, lay.bw, lay.bh, "-Bb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(lay.bw*3+6,   lay.row1y, lay.bw, lay.bh, "+Rb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(lay.bw*4+8,   lay.row1y, lay.bw, lay.bh, "+Gb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(lay.bw*5+10,  lay.row1y, lay.bw, lay.bh, "+Bb", MENU_BTN_COLOR, TFT_WHITE, 1);
        drawBtn(0,            lay.row2y, lay.bw, lay.bh, "-Rr", NAV_BTN_COLOR,  TFT_WHITE, 1);
        drawBtn(lay.bw+2,     lay.row2y, lay.bw, lay.bh, "-Gr", NAV_BTN_COLOR,  TFT_WHITE, 1);
        drawBtn(lay.bw*2+4,   lay.row2y, lay.bw, lay.bh, "-Br", NAV_BTN_COLOR,  TFT_WHITE, 1);
        drawBtn(lay.bw*3+6,   lay.row2y, lay.bw, lay.bh, "+Rr", NAV_BTN_COLOR,  TFT_WHITE, 1);
        drawBtn(lay.bw*4+8,   lay.row2y, lay.bw, lay.bh, "+Gr", NAV_BTN_COLOR,  TFT_WHITE, 1);
        drawBtn(lay.bw*5+10,  lay.row2y, lay.bw, lay.bh, "+Br", NAV_BTN_COLOR,  TFT_WHITE, 1);
      }

      Serial.printf("[ColorDist] Nen=0x%04X(R%d G%d B%d) Rect=0x%04X(+R%d +G%d +B%d)\n",
                    bgColor,bg_r,bg_g,bg_b,rectColor,rc_r,rc_g,rc_b);
      needRedraw = false;
    }

    // Serial
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') goto cdExit;
      handleGlobalSerial(c);
    }

    // Touch — dùng đúng tọa độ từ lay
    {
      TouchPoint tp = touchRead();
      if (tp.touched) {
        delay(100);
        int tx = tp.x, ty = tp.y;
        if (isBackBtn(tx, ty)) goto cdExit;

        if (lay.landscape) {
          // Landscape: sidebar bên phải
          if (tx >= lay.panX) {
            int relX = tx - lay.panX;
            int row  = ty / (lay.bh + 1);  // hàng 0-5
            if (row > 5) row = 5;
            bool isPlus = (relX >= lay.bw + 3);  // cột phải = +
            // row 0-2: Rb/Gb/Bb; row 3-5: Rr/Gr/Br
            if (row < 3) {
              if (!isPlus) {
                if (row==0) bg_r=constrain(bg_r-1,0,31);
                else if (row==1) bg_g=constrain(bg_g-1,0,63);
                else             bg_b=constrain(bg_b-1,0,31);
              } else {
                if (row==0) bg_r=constrain(bg_r+1,0,31);
                else if (row==1) bg_g=constrain(bg_g+1,0,63);
                else             bg_b=constrain(bg_b+1,0,31);
              }
            } else {
              int rr = row - 3;
              if (!isPlus) {
                if (rr==0) rc_r--; else if (rr==1) rc_g--; else rc_b--;
              } else {
                if (rr==0) rc_r++; else if (rr==1) rc_g++; else rc_b++;
              }
            }
            needRedraw = true;
          }
        } else {
          // Portrait: 2 hàng nút dưới
          // Dùng lay.bw, lay.bh, lay.row1y, lay.row2y
          int bw = lay.bw, bh = lay.bh;
          int r1y = lay.row1y, r2y = lay.row2y;
          if (ty >= r1y && ty < r1y + bh) {
            // Hàng 1: nền Rb Gb Bb (cột -) và +Rb +Gb +Bb (cột +)
            int col = tx / (bw + 2);  // 0..5
            if (col==0) { bg_r=constrain(bg_r-1,0,31); needRedraw=true; }
            else if (col==1) { bg_g=constrain(bg_g-1,0,63); needRedraw=true; }
            else if (col==2) { bg_b=constrain(bg_b-1,0,31); needRedraw=true; }
            else if (col==3) { bg_r=constrain(bg_r+1,0,31); needRedraw=true; }
            else if (col==4) { bg_g=constrain(bg_g+1,0,63); needRedraw=true; }
            else             { bg_b=constrain(bg_b+1,0,31); needRedraw=true; }
          }
          if (ty >= r2y && ty < r2y + bh) {
            int col = tx / (bw + 2);
            if (col==0) { rc_r--; needRedraw=true; }
            else if (col==1) { rc_g--; needRedraw=true; }
            else if (col==2) { rc_b--; needRedraw=true; }
            else if (col==3) { rc_r++; needRedraw=true; }
            else if (col==4) { rc_g++; needRedraw=true; }
            else             { rc_b++; needRedraw=true; }
          }
        }
      }
    }
    delay(20);
  }
  cdExit:
  Serial.printf("[ColorDist] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.5 — Gradient
// ═══════════════════════════════════════════════════════════════
// Dải chuyển màu để phát hiện banding (màu rời rạc thay vì liên tục)
void runGradient() {
  Serial.printf("[Gradient] Bat dau\n");

  // 7 màu đích từ đen đến màu đích
  struct GradTarget { const char* name; uint8_t r, g, b; };
  const GradTarget targets[] = {
    {"Trang",  31, 63, 31},
    {"Cyan",    0, 63, 31},
    {"Magenta",31,  0, 31},
    {"Vang",   31, 63,  0},
    {"Do",     31,  0,  0},
    {"Luc",     0, 63,  0},
    {"Lam",     0,  0, 31},
  };
  const int NT = 7;

  // Chế độ: 0=8 bước, 1=liên tục RGB565 thật, 2=liên tục+dithering
  const char* modeNames[] = {"8 Buoc", "Lien Tuc", "Dither"};
  int ti = 0, dir = 0, mode = 1; // dir: 0=ngang 1=dọc
  bool needRedraw = true;

  while (true) {
    if (needRedraw) {
      int W = tft.width(), H = tft.height();
      tft.fillScreen(TFT_BLACK);

      // Vẽ gradient
      int len = (dir == 0) ? W : H;
      const GradTarget& t = targets[ti];

      for (int i = 0; i < len; i++) {
        int r5 = 0, g6 = 0, b5 = 0;

        if (mode == 0) {
          // 8 bước rời rạc
          int step = i * 8 / len;
          r5 = t.r * step / 7;
          g6 = t.g * step / 7;
          b5 = t.b * step / 7;
        } else if (mode == 1) {
          // Liên tục — mỗi giá trị pixel là 1 mức RGB565 thực tế
          // Không nội suy giả 256 mức: tăng đúng 1 LSB từng kênh
          r5 = (t.r > 0) ? (int)((long)i * t.r / (len - 1)) : 0;
          g6 = (t.g > 0) ? (int)((long)i * t.g / (len - 1)) : 0;
          b5 = (t.b > 0) ? (int)((long)i * t.b / (len - 1)) : 0;
        } else {
          // Dithering: nội suy liên tục + nhiễu ±1 LSB xen kẽ
          float fr = (t.r > 0) ? (float)i * t.r / (len - 1) : 0;
          float fg2= (t.g > 0) ? (float)i * t.g / (len - 1) : 0;
          float fb = (t.b > 0) ? (float)i * t.b / (len - 1) : 0;
          // Ordered dithering đơn giản: cột chẵn làm tròn xuống, lẻ làm tròn lên
          int dith = (i % 2 == 0) ? 0 : 1;
          r5 = constrain((int)fr + (fr - (int)fr > 0.5f ? dith : 0), 0, t.r);
          g6 = constrain((int)fg2 + (fg2 - (int)fg2 > 0.5f ? dith : 0), 0, t.g);
          b5 = constrain((int)fb + (fb - (int)fb > 0.5f ? dith : 0), 0, t.b);
        }

        uint16_t c = ((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | (uint16_t)b5;
        if (dir == 0) tft.drawFastVLine(i, 0, H, c);
        else          tft.drawFastHLine(0, i, W, c);
      }

      // HUD trên
      tft.fillRect(0, 0, W, 18, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      char buf[64];
      snprintf(buf, sizeof(buf), "4.5 Gradient | %s | %s | %s",
               targets[ti].name, (dir==0)?"Ngang":"Doc", modeNames[mode]);
      tft.setCursor(2, 5); tft.print(buf);

      // 1 nút đơn dưới cùng — cao 28px, dễ bấm
      // Cycle qua: mode 0→1→2 (khi mode wrap về 0 thì đổi dir)
      // Nút trái = màu trước, nút phải = màu sau
      // Giữa = Pha/Dir/Mode
      int btnH = 28, btnY = H - btnH;
      int btnW3 = W / 3 - 1;
      tft.fillRect(0, btnY - 2, W, btnH + 2, TFT_BLACK);
      drawBtn(0,         btnY, btnW3,   btnH, "< Mau",         NAV_BTN_COLOR, TFT_WHITE, 1);
      drawBtn(btnW3+1,   btnY, btnW3,   btnH, "Pha/Dir/Mode",  ROT_BTN_COLOR, TFT_WHITE, 1);
      drawBtn(btnW3*2+2, btnY, W-btnW3*2-2, btnH, "Mau >",     NAV_BTN_COLOR, TFT_WHITE, 1);

      Serial.printf("[Gradient] Mau=%s Dir=%s Mode=%s\n",
                    targets[ti].name, dir?"Doc":"Ngang", modeNames[mode]);
      needRedraw = false;
    }

    { bool _q=false;
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c=='b'||c=='B'||c=='q'||c=='Q') { _q=true; break; }
        else if (c=='n'||c=='N') { ti=(ti+1)%NT; needRedraw=true; }
        else if (c=='p'||c=='P') { ti=(ti-1+NT)%NT; needRedraw=true; }
        else if (c=='d'||c=='D') { dir=1-dir; needRedraw=true; }
        else if (c=='m'||c=='M') { mode=(mode+1)%3; needRedraw=true; }
        else handleGlobalSerial(c);
      }
      if(_q) break;
    }
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(100);
      if (isBackBtn(tp.x, tp.y)) break;
      int W = tft.width(), H = tft.height();
      int tx = tp.x, ty = tp.y;
      int btnW3 = W / 3 - 1;
      int btnY = H - 28;
      if (ty >= btnY) {
        if      (tx < btnW3)       { ti=(ti-1+NT)%NT; needRedraw=true; }
        else if (tx < btnW3*2+1)   {
          // Pha/Dir/Mode: cycle mode, khi mode=0 (wrap) thì đổi dir
          mode = (mode+1) % 3;
          if (mode == 0) dir = 1-dir;
          needRedraw=true;
        }
        else                        { ti=(ti+1)%NT; needRedraw=true; }
      }
    }
    delay(20);
  }
  Serial.printf("[Gradient] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.6 — Sharpness (EIZO Monitor Test — Test 10)
//
//  Nguồn: monitor-test.html Test 10 — Lorem ipsum, Verdana 12px, no AA
//  Cơ chế: chữ phủ toàn màn hình, word-wrap, lặp lại khi hết text
//  Không có scrolling, không có interaction ngoài toggle màu + font
//
//  Toggle chế độ: chạm màn / serial 'i' / space = đảo màu
//  Font size:  serial 'f' = cycle font 1→2→4 / chạm góc phải-dưới
//  Back:       chạm góc trên-trái / serial b/q
// ═══════════════════════════════════════════════════════════════
void runSharpness() {
  Serial.printf("[Sharpness] Bat dau\n");
  Serial.printf("[Sharpness] EIZO Monitor Test 10 — Lorem ipsum, Verdana 12px, no AA\n");
  Serial.printf("[Sharpness] Pass: chu sac net, khong bong mo, khong nhoe\n");
  Serial.printf("[Sharpness] Fail: pixel xam o canh, ghosting, stroke nhoe\n");
  Serial.printf("[Sharpness] i/space/cham = dao mau | f/cham goc phai-duoi = doi font\n");
  Serial.printf("[Sharpness] b/q/cham goc tren-trai = thoat\n");

  // ─ Lorem ipsum — đúng nội dung EIZO gốc ──────────────────────────
  static const char LOREM[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum. ";
  const int LOREM_LEN = (int)sizeof(LOREM) - 1;

  // Font sets: {fontNum, lineStep_px}
  // Font 1 = 8px, Font 2 = 16px (gần nhất Verdana 12px), Font 4 = 26px
  // line-height 1.5x theo spec
  struct FontSet { uint8_t num; int lineStep; const char* name; };
  const FontSet FONTS[] = {
    { 2, 24, "Font2 (16px)" },   // default — gần Verdana 12px nhất
    { 1, 12, "Font1 (8px)"  },
    { 4, 39, "Font4 (26px)" },
  };
  const int NF = 3;

  int  schemeIdx = 0;  // 0 = Black-on-White (EIZO default), 1 = White-on-Black
  int  fontIdx   = 0;
  bool needRedraw = true;

  // ─ Lambda: vẽ toàn bộ màn hình bằng Lorem ipsum word-wrap ──────────
  auto drawSharpness = [&]() {
    int W = tft.width(), H = tft.height();
    uint16_t bg = (schemeIdx == 0) ? TFT_WHITE : TFT_BLACK;
    uint16_t fg = (schemeIdx == 0) ? TFT_BLACK : TFT_WHITE;

    tft.fillScreen(bg);
    tft.setTextFont(FONTS[fontIdx].num);
    tft.setTextColor(fg, bg);  // bg = overwrite, không có transparent ghost
    tft.setTextWrap(false);    // tự xử lý word-wrap — không để TFT_eSPI cắt giữa chữ

    int lineStep = FONTS[fontIdx].lineStep;
    int fontH    = tft.fontHeight(FONTS[fontIdx].num);  // pixel height thực tế
    int curY = fontH;    // baseline dòng 1 (y = baseline có thả kiểu)
    int curX = 0;
    int loremIdx = 0;

    while (curY < H + lineStep) {
      // ─ Đọc từ tiếp theo từ LOREM ───────────────────────────────────
      // Bỏ space đầu (chỉ khi không phải đầu dòng)
      if (curX > 0) {
        while (loremIdx < LOREM_LEN && LOREM[loremIdx] == ' ') loremIdx++;
        if (loremIdx >= LOREM_LEN) { loremIdx = 0; continue; }
      }

      // Tìm cuối từ
      int tokStart = loremIdx;
      while (loremIdx < LOREM_LEN && LOREM[loremIdx] != ' ') loremIdx++;
      int tokLen = loremIdx - tokStart;

      if (tokLen == 0) { loremIdx = 0; continue; }

      // Buffer từ
      char word[64];
      int cp = min(tokLen, 63);
      memcpy(word, LOREM + tokStart, cp);
      word[cp] = '\0';

      // Đo độ rộng từ + space trước
      int wordW  = tft.textWidth(word, FONTS[fontIdx].num);
      int spaceW = (curX > 0) ? tft.textWidth(" ", FONTS[fontIdx].num) : 0;

      // Word-wrap: xuống dòng nếu không vừa
      if (curX + spaceW + wordW > W) {
        curX = 0;
        curY += lineStep;
        if (curY >= H) break;  // màn hình đã đầy
        spaceW = 0;
      }

      // In space (nếu cần)
      if (spaceW > 0) {
        tft.setCursor(curX, curY);
        tft.setTextFont(FONTS[fontIdx].num);
        tft.print(" ");
        curX += spaceW;
      }

      // In từ
      tft.setCursor(curX, curY);
      tft.print(word);
      curX += wordW;

      // Wrap Lorem ipsum khi hết
      if (loremIdx >= LOREM_LEN) loremIdx = 0;
    }

    // Nhãn nhỏ góc dưới-phải (HUD tối thiểu — không che text chính)
    // Vẽ một hình chữ nhật nhỏ màu đảo, hiển thị mode và font
    tft.setTextFont(1);  // font nhỏ nhất cho nhãn
    char lbl[32];
    snprintf(lbl, sizeof(lbl), "%s %s",
             (schemeIdx==0)?"B/W":"W/B",
             FONTS[fontIdx].name);
    int lw = tft.textWidth(lbl, 1);
    int lh = tft.fontHeight(1);
    int lx = W - lw - 2, ly = H - lh - 1;
    tft.fillRect(lx-1, ly-1, lw+3, lh+2, fg);  // nền ngược
    tft.setTextColor(bg, fg);
    tft.setCursor(lx, ly);
    tft.print(lbl);

    Serial.printf("[Sharpness] Scheme=%s Font=%s W=%d H=%d lineStep=%d\n",
                  (schemeIdx==0)?"B/W":"W/B", FONTS[fontIdx].name, W, H, lineStep);
  };

  while (true) {
    if (needRedraw) {
      drawSharpness();
      needRedraw = false;
    }

    // Poll serial
    bool quit = false;
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') { quit=true; break; }
      else if (c=='i'||c=='I'||c==' ') { schemeIdx=1-schemeIdx; needRedraw=true; }
      else if (c=='f'||c=='F')          { fontIdx=(fontIdx+1)%NF; needRedraw=true; }
      else handleGlobalSerial(c);
    }
    if (quit) break;
    if (needRedraw) continue;

    // Poll touch
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(100);
      if (isBackBtn(tp.x, tp.y)) break;
      int W = tft.width(), H = tft.height();
      // Góc phải-dưới (1/6 W x 1/6 H) = đổi font
      if (tp.x > W*5/6 && tp.y > H*5/6) {
        fontIdx = (fontIdx + 1) % NF;
        needRedraw = true;
      } else {
        // Bất kỳ vị trí khác = đảo màu
        schemeIdx = 1 - schemeIdx;
        needRedraw = true;
      }
    }
    delay(20);
  }

  // Khôi phục font mặc định
  tft.setTextFont(1);
  Serial.printf("[Sharpness] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.7 — Viewing Angle (EIZO Monitor Test — Test 11)
//
//  Nguồn: monitor-test.html Test 11 — 5 white radial-glow circles
//  Nền: TFT_BLACK — body gradient gốc (#160052) chỉ sáng 9% = đen
//  Mỗi vòng: radial-gradient trắng, fade bắt đầu tại 66% bán kính
//             diameter = 16vw, radius = 8vw
//  Vị trí: 4 góc (tâm cách mép = 8vw) + 1 tâm màn hình
//  Màn hình tĩnh hoàn toàn (không text overlay, không interaction)
// ═══════════════════════════════════════════════════════════════
void runViewingAngle() {
  Serial.printf("[ViewAngle] Bat dau\n");
  Serial.printf("[ViewAngle] EIZO Monitor Test 11 — 5 glow circles tren nen den\n");
  Serial.printf("[ViewAngle] Quy trinh:\n");
  Serial.printf("[ViewAngle]   1. Nhin thang vao man hinh — ghi nho 5 vong tron\n");
  Serial.printf("[ViewAngle]   2. Nghieng dan: Trai/Phai/Len/Xuong den 45-60 do\n");
  Serial.printf("[ViewAngle]   3. Quan sat: vong tron doi mau/meo hinh/mat gradient?\n");
  Serial.printf("[ViewAngle]   IPS: giu nguyen >= 45 do. TN: doi ro < 30 do.\n");
  Serial.printf("[ViewAngle] Cham/b/q = thoat\n");

  int W = tft.width(), H = tft.height();

  // ── Tính kích thước theo spec ──────────────────────────────
  // circle_diameter = 16vw, circle_radius = 8vw
  // fade_start = radius * 0.66 (gradient fade bắt đầu tại 66%)
  int full_r = (int)(W * 0.08f);
  // Safety: nếu màn nhỏ, giảm để vòng góc không đè vòng giữa
  {
    int margin = min(W / 2, H / 2) - full_r;
    if (margin < full_r) full_r = min(W, H) / 4;
  }
  full_r = max(full_r, 8);
  int fade_r = max((int)(full_r * 0.66f), 4);

  // ── Pre-compute glow LUT: màu blend trắng→đen theo bán kính ─
  // r=0 (tâm): trắng (255,255,255) — t=1.0
  // r=fade_r : đen  (0,0,0)        — t=0.0
  // r>fade_r : trong suốt          — không vẽ
  uint16_t glow_lut[128];
  int lut_max = min(fade_r, 127);
  for (int r = 0; r <= lut_max; r++) {
    float t = 1.0f - (float)r / (float)fade_r;  // 1.0→0.0
    uint8_t v = (uint8_t)(t * 255.0f);
    glow_lut[r] = tft.color565(v, v, v);
  }

  // ── Tọa độ tâm 5 vòng tròn (theo spec Section 7.4) ──────────
  // va1: (R,   R)    va2: (W-R, R)
  // va3: (R, H-R)    va4: (W-R, H-R)
  // va5: (W/2, H/2)
  int R = full_r;
  struct VaCircle { int x, y; };
  const VaCircle centers[5] = {
    { R,     R     },   // va1: top-left
    { W-R,   R     },   // va2: top-right
    { R,     H-R   },   // va3: bot-left
    { W-R,   H-R   },   // va4: bot-right
    { W/2,   H/2   },   // va5: center
  };

  Serial.printf("[ViewAngle] W=%d H=%d full_r=%d fade_r=%d\n", W, H, full_r, fade_r);

  // ── Render ────────────────────────────────────────────────────
  // 1. Nền đen
  tft.fillScreen(TFT_BLACK);

  // 2. Vẽ 5 vòng glow — concentric fillCircle từ fade_r về 0
  //    Mỗi vòng ~fade_r lần fillCircle (≈17 lần cho 320px màn)
  //    r > fade_r là trong suốt → không vẽ (nền giữ nguyên)
  for (int i = 0; i < 5; i++) {
    int cx = centers[i].x, cy = centers[i].y;
    for (int r = fade_r; r >= 0; r--) {
      tft.fillCircle(cx, cy, r, glow_lut[min(r, lut_max)]);
    }
  }

  // Màn hình tĩnh — không text overlay (giống HTML gốc)
  // Serial đã có hướng dẫn đầy đủ ở trên
  // Chờ Back: chạm góc trên-trái hoặc serial b/q
  while (true) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c=='b'||c=='B'||c=='q'||c=='Q') goto vaExit;
      handleGlobalSerial(c);
    }
    TouchPoint tp = touchRead();
    if (tp.touched) {
      delay(120);
      if (isBackBtn(tp.x, tp.y)) goto vaExit;
    }
    delay(20);
  }
  vaExit:
  Serial.printf("[ViewAngle] Ket thuc\n");
}

// ═══════════════════════════════════════════════════════════════
//  4.8 — Gamma (EIZO Monitor Test faithful port)
//
//  Nguyên lý (từ EIZO monitor-test.html + gamma_spec_v2.md):
//  • Background: sọc dọc đen/trắng 1px/cột (x%2==0→đen, x%2==1→trắng)
//    → mắt tích hợp luminance, nhìn thấy xám đồng nhất 50% ở xa
//  • Logo EIZO 1-bit bitmap đặt chính giữa màn hình
//    fill = rgb(V,V,V) từ GAMMA_LUT[idx]
//  • Người dùng bấm + / − để thay đổi gamma. Khi logo "biến mất"
//    vào nền sọc → giá trị đó là gamma thật của màn hình.
//
//  Điều khiển: nút cảm ứng + / − ở bottom bar (fallback: serial +/-)
// ═══════════════════════════════════════════════════════════════

// ── GAMMA LUT (23 entries) – từ JS gốc EIZO ────────────────────
// V[i] = round(255 × 0.5^(1/γ[i]))
// sRGB[13]=188, L*[17]=194
static const uint8_t GAMMA_LUT[23] PROGMEM = {
    128, 136, 143, 150, 155, 161, 165, 170, 174, 177,
    180, 183, 186, 188, 189, 191, 193, 194, 195, 197,
    199, 201, 202
};
static const char* const GAMMA_LABELS[23] PROGMEM = {
    "1.0","1.1","1.2","1.3","1.4","1.5","1.6","1.7",
    "1.8","1.9","2.0","2.1","2.2","sRGB","2.3","2.4",
    "2.5","L*","2.6","2.7","2.8","2.9","3.0"
};
#define GAMMA_DEFAULT_IDX  5   // γ=1.5, V=161 (mặc định EIZO)
#define GAMMA_MAX_IDX     22

// ── Shape-7 Logo bitmap 64×65px (cho màn 320×240) ────────────
// Source: <path id="Shape-7"> viewBox="20 -8 232 234"
// CSS: width=20vw × height=20.2vw, left=40vw, top=50vh-10.1vw
// Bit=1 → pixel màu V, Bit=0 → giữ nền stripe. MSB first.
#define EIZO_W    64
#define EIZO_H    65
#define EIZO_BPR   8   // bytes per row = ceil(64/8)

static const uint8_t EIZO_BITMAP[520] PROGMEM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row  0
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row  1
    0x00,0x00,0x00,0x01,0x80,0x00,0x00,0x00,  // row  2
    0x00,0x00,0x00,0x07,0xC0,0x00,0x00,0x00,  // row  3
    0x00,0x00,0x00,0x07,0xE0,0x00,0x00,0x00,  // row  4
    0x00,0x00,0x00,0x0F,0xF0,0x00,0x00,0x00,  // row  5
    0x00,0x00,0x00,0x0F,0xF8,0x00,0x00,0x00,  // row  6
    0x00,0x00,0x00,0x0F,0xFC,0x00,0x00,0x00,  // row  7
    0x00,0x00,0x00,0x1F,0xFE,0x00,0x00,0x00,  // row  8
    0x00,0x00,0x00,0x1F,0xFE,0x00,0x00,0x00,  // row  9
    0x00,0x00,0x00,0x3F,0xFC,0x00,0x00,0x00,  // row 10
    0x00,0x00,0x03,0xFF,0xF8,0x00,0x00,0x00,  // row 11
    0x00,0x00,0x0F,0xFF,0xF0,0x00,0x00,0x00,  // row 12
    0x00,0x00,0x0F,0xFF,0xF0,0x30,0x00,0x00,  // row 13
    0x00,0x00,0x1F,0xFF,0xF8,0x78,0x00,0x00,  // row 14
    0x00,0x00,0x1F,0xFF,0xFC,0xFC,0x00,0x00,  // row 15
    0x00,0x00,0x1F,0xFF,0xFF,0xFE,0x00,0x00,  // row 16
    0x00,0x00,0x1F,0xFE,0x7C,0xFC,0x00,0x00,  // row 17
    0x00,0x00,0x3F,0xFC,0x38,0x78,0x00,0x00,  // row 18
    0x00,0x00,0x7F,0xF8,0x18,0x30,0x00,0x00,  // row 19
    0x00,0x07,0xFF,0xF0,0x10,0x10,0x00,0x00,  // row 20
    0x00,0x1F,0xFF,0xF8,0x38,0x30,0x30,0x00,  // row 21
    0x00,0x1F,0xFF,0xFC,0x7C,0x78,0x78,0x00,  // row 22
    0x00,0x3F,0xFF,0xFE,0xFE,0xFC,0xFC,0x00,  // row 23
    0x00,0x3F,0xFF,0xFF,0xFF,0xFF,0xFC,0x00,  // row 24
    0x00,0x3F,0xFE,0x7E,0x7C,0xFC,0xFC,0x00,  // row 25
    0x00,0x3F,0xFC,0x3C,0x38,0x78,0x78,0x00,  // row 26
    0x00,0x7F,0xF8,0x18,0x10,0x30,0x30,0x00,  // row 27
    0x00,0xFF,0xF8,0x00,0x10,0x10,0x20,0x20,  // row 28
    0x0F,0xFF,0xFC,0x18,0x38,0x38,0x70,0x70,  // row 29
    0x1F,0xFF,0xFE,0x3C,0x7C,0x7C,0x78,0xF8,  // row 30
    0x3F,0xFF,0xFF,0x7E,0xFE,0xFE,0xFD,0xFC,  // row 31
    0x7F,0xFF,0xFF,0xFE,0xFE,0xFF,0xFF,0xFC,  // row 32
    0x7F,0xFF,0xFF,0xFC,0x7C,0x7C,0xFF,0xF8,  // row 33
    0x3F,0xFF,0xFF,0xF8,0x38,0x38,0x7F,0xF0,  // row 34
    0x1F,0xFF,0xFF,0xF0,0x10,0x10,0x3F,0xE0,  // row 35
    0x0F,0xFF,0xFF,0xF8,0x10,0x30,0x3F,0xC0,  // row 36
    0x07,0xFF,0xFF,0xFC,0x38,0x78,0x7F,0x80,  // row 37
    0x03,0xFF,0xFF,0xFE,0x7C,0xFC,0xFF,0x00,  // row 38
    0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0x00,  // row 39
    0x00,0xFF,0xFF,0xFF,0xFE,0xFF,0xFC,0x00,  // row 40
    0x00,0x7F,0xFF,0xFF,0xFC,0x7F,0xF8,0x00,  // row 41
    0x00,0x3F,0xFF,0xFF,0xF8,0x3F,0xF0,0x00,  // row 42
    0x00,0x1F,0xFF,0xFF,0xF0,0x1F,0xE0,0x00,  // row 43
    0x00,0x0F,0xFF,0xFF,0xF8,0x3F,0xC0,0x00,  // row 44
    0x00,0x07,0xFF,0xFF,0xF8,0x7F,0x80,0x00,  // row 45
    0x00,0x03,0xFF,0xFF,0xFC,0xFF,0x00,0x00,  // row 46
    0x00,0x01,0xFF,0xFF,0xFF,0xFE,0x00,0x00,  // row 47
    0x00,0x00,0xFF,0xFF,0xFF,0xFC,0x00,0x00,  // row 48
    0x00,0x00,0x7F,0xFF,0xFF,0xF8,0x00,0x00,  // row 49
    0x00,0x00,0x3F,0xFF,0xFF,0xF0,0x00,0x00,  // row 50
    0x00,0x00,0x1F,0xFF,0xFF,0xE0,0x00,0x00,  // row 51
    0x00,0x00,0x0F,0xFF,0xFF,0xC0,0x00,0x00,  // row 52
    0x00,0x00,0x07,0xFF,0xFF,0x80,0x00,0x00,  // row 53
    0x00,0x00,0x03,0xFF,0xFF,0x00,0x00,0x00,  // row 54
    0x00,0x00,0x01,0xFF,0xFE,0x00,0x00,0x00,  // row 55
    0x00,0x00,0x00,0xFF,0xFC,0x00,0x00,0x00,  // row 56
    0x00,0x00,0x00,0x7F,0xF8,0x00,0x00,0x00,  // row 57
    0x00,0x00,0x00,0x3F,0xF0,0x00,0x00,0x00,  // row 58
    0x00,0x00,0x00,0x1F,0xE0,0x00,0x00,0x00,  // row 59
    0x00,0x00,0x00,0x0F,0xC0,0x00,0x00,0x00,  // row 60
    0x00,0x00,0x00,0x07,0x80,0x00,0x00,0x00,  // row 61
    0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,  // row 62
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row 63
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00   // row 64
};

// ── Shape-7 Logo bitmap 96×97px (cho màn 480×320) ─────────────
#define EIZO_W_480   96
#define EIZO_H_480   97
#define EIZO_BPR_480  12   // bytes per row = ceil(96/8)

static const uint8_t EIZO_BITMAP_480[1164] PROGMEM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row  0
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row  1
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row  2
    0x00,0x00,0x00,0x00,0x00,0x01,0xC0,0x00,0x00,0x00,0x00,0x00,  // row  3
    0x00,0x00,0x00,0x00,0x00,0x07,0xE0,0x00,0x00,0x00,0x00,0x00,  // row  4
    0x00,0x00,0x00,0x00,0x00,0x0F,0xF0,0x00,0x00,0x00,0x00,0x00,  // row  5
    0x00,0x00,0x00,0x00,0x00,0x1F,0xF8,0x00,0x00,0x00,0x00,0x00,  // row  6
    0x00,0x00,0x00,0x00,0x00,0x3F,0xFC,0x00,0x00,0x00,0x00,0x00,  // row  7
    0x00,0x00,0x00,0x00,0x00,0x3F,0xFC,0x00,0x00,0x00,0x00,0x00,  // row  8
    0x00,0x00,0x00,0x00,0x00,0x3F,0xFE,0x00,0x00,0x00,0x00,0x00,  // row  9
    0x00,0x00,0x00,0x00,0x00,0x3F,0xFF,0x00,0x00,0x00,0x00,0x00,  // row 10
    0x00,0x00,0x00,0x00,0x00,0x3F,0xFF,0x80,0x00,0x00,0x00,0x00,  // row 11
    0x00,0x00,0x00,0x00,0x00,0x7F,0xFF,0xC0,0x00,0x00,0x00,0x00,  // row 12
    0x00,0x00,0x00,0x00,0x00,0x7F,0xFF,0xE0,0x00,0x00,0x00,0x00,  // row 13
    0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,  // row 14
    0x00,0x00,0x00,0x00,0x01,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,  // row 15
    0x00,0x00,0x00,0x00,0x0F,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,  // row 16
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,  // row 17
    0x00,0x00,0x00,0x01,0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,  // row 18
    0x00,0x00,0x00,0x03,0xFF,0xFF,0xFC,0x00,0x80,0x00,0x00,0x00,  // row 19
    0x00,0x00,0x00,0x07,0xFF,0xFF,0xFE,0x01,0xC0,0x00,0x00,0x00,  // row 20
    0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0x03,0xE0,0x00,0x00,0x00,  // row 21
    0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0x87,0xF0,0x00,0x00,0x00,  // row 22
    0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0xCF,0xF8,0x00,0x00,0x00,  // row 23
    0x00,0x00,0x00,0x0F,0xFF,0xFF,0xFF,0xFF,0xFC,0x00,0x00,0x00,  // row 24
    0x00,0x00,0x00,0x0F,0xFF,0xFE,0xFF,0xCF,0xFC,0x00,0x00,0x00,  // row 25
    0x00,0x00,0x00,0x0F,0xFF,0xFC,0x7F,0x87,0xF8,0x00,0x00,0x00,  // row 26
    0x00,0x00,0x00,0x1F,0xFF,0xF8,0x3F,0x03,0xF0,0x00,0x00,0x00,  // row 27
    0x00,0x00,0x00,0x3F,0xFF,0xF0,0x1E,0x01,0xE0,0x00,0x00,0x00,  // row 28
    0x00,0x00,0x00,0xFF,0xFF,0xE0,0x0C,0x00,0xC0,0x00,0x00,0x00,  // row 29
    0x00,0x00,0x1F,0xFF,0xFF,0xC0,0x04,0x00,0x80,0x00,0x00,0x00,  // row 30
    0x00,0x00,0x3F,0xFF,0xFF,0xE0,0x0C,0x00,0xC0,0x18,0x00,0x00,  // row 31
    0x00,0x00,0x7F,0xFF,0xFF,0xF0,0x1E,0x01,0xE0,0x3C,0x00,0x00,  // row 32
    0x00,0x00,0xFF,0xFF,0xFF,0xF8,0x3F,0x03,0xF0,0x7E,0x00,0x00,  // row 33
    0x00,0x00,0xFF,0xFF,0xFF,0xFC,0x7F,0x87,0xF8,0xFF,0x00,0x00,  // row 34
    0x00,0x01,0xFF,0xFF,0xFF,0xFE,0xFF,0xCF,0xFD,0xFF,0x80,0x00,  // row 35
    0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC0,0x00,  // row 36
    0x00,0x01,0xFF,0xFF,0xE7,0xFC,0x7F,0xCF,0xF8,0xFF,0x80,0x00,  // row 37
    0x00,0x01,0xFF,0xFF,0xC3,0xF8,0x3F,0x87,0xF0,0x7F,0x00,0x00,  // row 38
    0x00,0x01,0xFF,0xFF,0x81,0xF0,0x1F,0x03,0xE0,0x3E,0x00,0x00,  // row 39
    0x00,0x03,0xFF,0xFF,0x00,0xE0,0x0E,0x01,0xC0,0x1C,0x00,0x00,  // row 40
    0x00,0x07,0xFF,0xFE,0x00,0x40,0x04,0x00,0x80,0x08,0x00,0x00,  // row 41
    0x00,0x1F,0xFF,0xFE,0x00,0x40,0x04,0x00,0x80,0x08,0x01,0x00,  // row 42
    0x01,0xFF,0xFF,0xFF,0x00,0xE0,0x0E,0x01,0xC0,0x1C,0x03,0x80,  // row 43
    0x07,0xFF,0xFF,0xFF,0x81,0xF0,0x1F,0x03,0xE0,0x3E,0x07,0xC0,  // row 44
    0x0F,0xFF,0xFF,0xFF,0xC3,0xF8,0x3F,0x87,0xF0,0x7F,0x0F,0xE0,  // row 45
    0x1F,0xFF,0xFF,0xFF,0xE7,0xFC,0x7F,0xCF,0xF8,0xFF,0x9F,0xF0,  // row 46
    0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xF8,  // row 47
    0x3F,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xCF,0xFD,0xFF,0xFF,0xF8,  // row 48
    0x3F,0xFF,0xFF,0xFF,0xFF,0xFC,0x7F,0x87,0xF8,0xFF,0xFF,0xF0,  // row 49
    0x3F,0xFF,0xFF,0xFF,0xFF,0xF8,0x3F,0x03,0xF0,0x7F,0xFF,0xE0,  // row 50
    0x1F,0xFF,0xFF,0xFF,0xFF,0xF0,0x1E,0x01,0xE0,0x3F,0xFF,0xC0,  // row 51
    0x0F,0xFF,0xFF,0xFF,0xFF,0xE0,0x0C,0x00,0xC0,0x1F,0xFF,0x80,  // row 52
    0x07,0xFF,0xFF,0xFF,0xFF,0xC0,0x04,0x00,0x80,0x0F,0xFF,0x00,  // row 53
    0x03,0xFF,0xFF,0xFF,0xFF,0xE0,0x0C,0x00,0xC0,0x1F,0xFE,0x00,  // row 54
    0x01,0xFF,0xFF,0xFF,0xFF,0xF0,0x1E,0x01,0xE0,0x3F,0xFC,0x00,  // row 55
    0x00,0xFF,0xFF,0xFF,0xFF,0xF8,0x3F,0x03,0xF0,0x7F,0xF8,0x00,  // row 56
    0x00,0x7F,0xFF,0xFF,0xFF,0xFC,0x7F,0x87,0xF8,0xFF,0xF0,0x00,  // row 57
    0x00,0x3F,0xFF,0xFF,0xFF,0xFE,0xFF,0xCF,0xFD,0xFF,0xE0,0x00,  // row 58
    0x00,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC0,0x00,  // row 59
    0x00,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xCF,0xFF,0xFF,0x80,0x00,  // row 60
    0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,0x87,0xFF,0xFF,0x00,0x00,  // row 61
    0x00,0x03,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0xFF,0xFE,0x00,0x00,  // row 62
    0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFE,0x01,0xFF,0xFC,0x00,0x00,  // row 63
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFC,0x00,0xFF,0xF8,0x00,0x00,  // row 64
    0x00,0x00,0x7F,0xFF,0xFF,0xFF,0xFC,0x00,0xFF,0xF0,0x00,0x00,  // row 65
    0x00,0x00,0x3F,0xFF,0xFF,0xFF,0xFE,0x01,0xFF,0xE0,0x00,0x00,  // row 66
    0x00,0x00,0x1F,0xFF,0xFF,0xFF,0xFF,0x03,0xFF,0xC0,0x00,0x00,  // row 67
    0x00,0x00,0x0F,0xFF,0xFF,0xFF,0xFF,0x87,0xFF,0x80,0x00,0x00,  // row 68
    0x00,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xCF,0xFF,0x00,0x00,0x00,  // row 69
    0x00,0x00,0x03,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0x00,0x00,0x00,  // row 70
    0x00,0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFC,0x00,0x00,0x00,  // row 71
    0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xF8,0x00,0x00,0x00,  // row 72
    0x00,0x00,0x00,0x7F,0xFF,0xFF,0xFF,0xFF,0xF0,0x00,0x00,0x00,  // row 73
    0x00,0x00,0x00,0x3F,0xFF,0xFF,0xFF,0xFF,0xE0,0x00,0x00,0x00,  // row 74
    0x00,0x00,0x00,0x1F,0xFF,0xFF,0xFF,0xFF,0xC0,0x00,0x00,0x00,  // row 75
    0x00,0x00,0x00,0x0F,0xFF,0xFF,0xFF,0xFF,0x80,0x00,0x00,0x00,  // row 76
    0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,  // row 77
    0x00,0x00,0x00,0x03,0xFF,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,  // row 78
    0x00,0x00,0x00,0x01,0xFF,0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,  // row 79
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xF8,0x00,0x00,0x00,0x00,  // row 80
    0x00,0x00,0x00,0x00,0x7F,0xFF,0xFF,0xF0,0x00,0x00,0x00,0x00,  // row 81
    0x00,0x00,0x00,0x00,0x3F,0xFF,0xFF,0xE0,0x00,0x00,0x00,0x00,  // row 82
    0x00,0x00,0x00,0x00,0x1F,0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,  // row 83
    0x00,0x00,0x00,0x00,0x0F,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,  // row 84
    0x00,0x00,0x00,0x00,0x07,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,  // row 85
    0x00,0x00,0x00,0x00,0x03,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,  // row 86
    0x00,0x00,0x00,0x00,0x01,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,  // row 87
    0x00,0x00,0x00,0x00,0x00,0xFF,0xF8,0x00,0x00,0x00,0x00,0x00,  // row 88
    0x00,0x00,0x00,0x00,0x00,0x7F,0xF0,0x00,0x00,0x00,0x00,0x00,  // row 89
    0x00,0x00,0x00,0x00,0x00,0x3F,0xE0,0x00,0x00,0x00,0x00,0x00,  // row 90
    0x00,0x00,0x00,0x00,0x00,0x1F,0xC0,0x00,0x00,0x00,0x00,0x00,  // row 91
    0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x00,0x00,0x00,0x00,0x00,  // row 92
    0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,  // row 93
    0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,  // row 94
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // row 95
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00   // row 96
};

// ── Helper: sọc dọc đen-trắng toàn màn hình ──────────────────
// x chẵn = đen, x lẻ = trắng (giống tile gamma_bg_2019.png 2×1px)
static void gamma_drawStripeBg(int w, int h) {
    for (int x = 0; x < w; x++)
        tft.drawFastVLine(x, 0, h, (x & 1) ? TFT_WHITE : TFT_BLACK);
}

// ── Helper: vẽ logo EIZO bitmap ───────────────────────────────
// Bit=1 → pixel màu grayV (rgb solid), Bit=0 → tái tạo stripe bg
// Dùng pushImage 1 row tại một thời điểm để tránh allocate lớn
static void gamma_drawEizoLogo(const uint8_t* bmp, int bmp_w, int bmp_h,
                                int bpr, int logo_ox, int logo_oy,
                                uint8_t grayV)
{
    uint16_t fg = tft.color565(grayV, grayV, grayV);
    // Buffer 1 hàng — bmp_w tối đa 96px
    static uint16_t row_buf[96];

    tft.setSwapBytes(true);

    for (int dy = 0; dy < bmp_h; dy++) {
        // Đọc 1 row bitmap từ PROGMEM
        uint8_t bmp_row[12];  // max BPR=12 cho 96px
        memcpy_P(bmp_row, bmp + (uint32_t)dy * bpr, bpr);
        int screen_y = logo_oy + dy;

        for (int dx = 0; dx < bmp_w; dx++) {
            bool logo_px = (bmp_row[dx >> 3] >> (7 - (dx & 7))) & 1;
            int screen_x = logo_ox + dx;
            row_buf[dx] = logo_px
                ? fg
                : ((screen_x & 1) ? TFT_WHITE : TFT_BLACK);
        }
        tft.pushImage(logo_ox, screen_y, bmp_w, 1, row_buf);
    }

    tft.setSwapBytes(false);
}

// ── Helper: bottom UI bar với nút − và + ─────────────────────
static void gamma_drawUI(int screenW, int screenH, int idx) {
    int ui_h = 36;
    int ui_y = screenH - ui_h;

    // Nền thanh UI — đen đủ tương phản với stripe
    tft.fillRect(0, ui_y, screenW, ui_h, TFT_BLACK);

    // Nút −
    int btn_w = 36, btn_h = 26, btn_y = ui_y + 5;
    tft.fillRoundRect(4, btn_y, btn_w, btn_h, 4, tft.color565(50,50,80));
    tft.drawRoundRect(4, btn_y, btn_w, btn_h, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, tft.color565(50,50,80));
    tft.setTextSize(2);
    tft.setCursor(14, btn_y + 5);
    tft.print("-");

    // Nút +
    tft.fillRoundRect(screenW - btn_w - 4, btn_y, btn_w, btn_h, 4, tft.color565(50,50,80));
    tft.drawRoundRect(screenW - btn_w - 4, btn_y, btn_w, btn_h, 4, TFT_WHITE);
    tft.setCursor(screenW - btn_w + 4, btn_y + 5);
    tft.print("+");

    // Label: "G: 2.2" ở giữa — sRGB/L* màu vàng
    bool is_special = (idx == 13 || idx == 17);  // sRGB=13, L*=17
    uint16_t lc = is_special ? TFT_YELLOW : TFT_WHITE;
    char buf[16];
    const char* lbl = (const char*)pgm_read_ptr(&GAMMA_LABELS[idx]);
    snprintf(buf, sizeof(buf), "G: %s", lbl);
    tft.setTextColor(lc, TFT_BLACK);
    tft.setTextSize(2);
    int tw = strlen(buf) * 12;
    tft.setCursor((screenW - tw) / 2, btn_y + 5);
    tft.print(buf);

    // Ghi Serial để debug
    uint8_t v = pgm_read_byte(&GAMMA_LUT[idx]);
    Serial.printf("[Gamma] idx=%d label=%s V=%d\n", idx, lbl, v);
}

// ── Helper: vị trí logo theo kích thước màn ──────────────────
// CSS: width=20vw, height=20.2vw, left=40vw, top=50vh-10.1vw
static void gamma_logoPos(int sw, int sh,
                          int &ox, int &oy, int &lw, int &lh) {
    lw = sw * 20  / 100;
    lh = sw * 202 / 1000;
    ox = sw * 40  / 100;
    oy = sh / 2   - sw * 101 / 1000;
}

// ── Main ─────────────────────────────────────────────────────
void runGamma() {
    Serial.printf("[Gamma] Bat dau — EIZO Monitor Test faithful port\n");

    int sw = tft.width(), sh = tft.height();
    int gammaIdx = GAMMA_DEFAULT_IDX;  // γ=1.5, V=161

    // 1. Chọn bitmap phù hợp với màn hình
    const uint8_t* bmp;
    int bmp_w, bmp_h, bpr;
    if (sw >= 480) {
        bmp   = EIZO_BITMAP_480;
        bmp_w = EIZO_W_480;
        bmp_h = EIZO_H_480;
        bpr   = EIZO_BPR_480;
    } else {
        bmp   = EIZO_BITMAP;
        bmp_w = EIZO_W;
        bmp_h = EIZO_H;
        bpr   = EIZO_BPR;
    }

    // 2. Vẽ lần đầu: stripe bg + logo + UI
    int logo_ox, logo_oy, logo_lw, logo_lh;
    gamma_logoPos(sw, sh, logo_ox, logo_oy, logo_lw, logo_lh);
    gamma_drawStripeBg(sw, sh);
    uint8_t v0 = pgm_read_byte(&GAMMA_LUT[gammaIdx]);
    gamma_drawEizoLogo(bmp, bmp_w, bmp_h, bpr, logo_ox, logo_oy, v0);
    gamma_drawUI(sw, sh, gammaIdx);

    Serial.printf("[Gamma] Screen=%dx%d Logo=%dx%d at (%d,%d)\n",
                  sw, sh, bmp_w, bmp_h, logo_ox, logo_oy);

    // 3. Event loop — chỉ redraw logo + UI bar khi idx thay đổi
    while (true) {
        // ── Serial input ──────────────────────────────────────
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c=='b'||c=='B'||c=='q'||c=='Q') goto gammaExit;
            else if (c=='+' || c=='=') {
                if (gammaIdx < GAMMA_MAX_IDX) {
                    gammaIdx++;
                    uint8_t v = pgm_read_byte(&GAMMA_LUT[gammaIdx]);
                    gamma_drawEizoLogo(bmp, bmp_w, bmp_h, bpr, logo_ox, logo_oy, v);
                    gamma_drawUI(sw, sh, gammaIdx);
                }
            } else if (c=='-') {
                if (gammaIdx > 0) {
                    gammaIdx--;
                    uint8_t v = pgm_read_byte(&GAMMA_LUT[gammaIdx]);
                    gamma_drawEizoLogo(bmp, bmp_w, bmp_h, bpr, logo_ox, logo_oy, v);
                    gamma_drawUI(sw, sh, gammaIdx);
                }
            } else {
                handleGlobalSerial(c);
            }
        }

        // ── Touch input ───────────────────────────────────────
        TouchPoint tp = touchRead();
        if (tp.touched) {
            delay(120);  // debounce
            if (isBackBtn(tp.x, tp.y)) goto gammaExit;

            int ui_y = sh - 36;
            if (tp.y >= ui_y) {
                // Vùng nút: − ở trái (x < sw/2), + ở phải (x >= sw/2)
                bool changed = false;
                if (tp.x < sw / 2 && gammaIdx > 0) {
                    gammaIdx--;
                    changed = true;
                } else if (tp.x >= sw / 2 && gammaIdx < GAMMA_MAX_IDX) {
                    gammaIdx++;
                    changed = true;
                }
                if (changed) {
                    uint8_t v = pgm_read_byte(&GAMMA_LUT[gammaIdx]);
                    gamma_drawEizoLogo(bmp, bmp_w, bmp_h, bpr, logo_ox, logo_oy, v);
                    gamma_drawUI(sw, sh, gammaIdx);
                }
            }
        }

        delay(20);
    }

gammaExit:
    Serial.printf("[Gamma] Ket thuc\n");
}



