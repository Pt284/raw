// ═══════════════════════════════════════════════════════════════
//  TFT_Test_Suite.ino — Main sketch (Phase 1+2+3 rev2)
//  Fixes: stub freeze, menu overlap, serial fallback global,
//         persistent cursor+trail, dark theme, settings page
// ═══════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include "Common.h"
#include "Touch.h"

// ─── Đối tượng chính ───────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// ─── Biến trạng thái toàn cục ──────────────────────────────────
AppState appState       = STATE_CALIBRATION;
TestID   currentTest    = TEST_NONE;
uint8_t  screenRotation = 0;
uint8_t  touchRotation  = 0;
uint8_t  ledBrightness  = LED_BRIGHTNESS_DEF;
bool     darkMode       = false;

uint8_t recommendedTouchRot[4] = { 0, 1, 2, 3 };

// ─── Trạng thái menu ───────────────────────────────────────────
static int  menuPage        = 0;
static int  menuTestPages   = 0;   // số trang test (không tính settings)
static bool menuNeedsRedraw = true;
static bool inSettings      = false; // đang xem trang Settings

// ─── Con trỏ ảo — persistent + trail ──────────────────────────
#define CURSOR_HALF  5
#define TRAIL_MAX   32
static int16_t  trailX[TRAIL_MAX], trailY[TRAIL_MAX];
static int       trailLen   = 0;
static int16_t   cursorX    = -1, cursorY = -1;
static bool      wasTouch   = false;  // trạng thái chạm kỳ trước

// ─── Màu theme ─────────────────────────────────────────────────
// Trả về màu đã đảo nếu darkMode (đảo 1 lần nữa = về gốc)
static uint16_t T(uint16_t c) {
  if (!darkMode) return c;
  // Đảo từng kênh R(5)/G(6)/B(5) của RGB565
  uint8_t r = (~(c >> 11)) & 0x1F;
  uint8_t g = (~(c >>  5)) & 0x3F;
  uint8_t b = (~c)         & 0x1F;
  return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}
static uint16_t TC_BG()     { return darkMode ? TFT_BLACK   : 0x0820; }  // nền tổng
static uint16_t TC_HEADER() { return darkMode ? 0x1084      : MENU_HEADER_COLOR; }
static uint16_t TC_BTN()    { return darkMode ? 0x2124      : MENU_BTN_COLOR; }
static uint16_t TC_NAV()    { return darkMode ? 0x2945      : NAV_BTN_COLOR; }
static uint16_t TC_ROT()    { return darkMode ? 0x4208      : ROT_BTN_COLOR; }
static uint16_t TC_CALIB()  { return darkMode ? 0x0248      : CALIB_BTN_COLOR; }
static uint16_t TC_BACK()   { return darkMode ? 0x3186      : BACK_BTN_COLOR; }

// ─── Prototype ─────────────────────────────────────────────────
void     initLED();
void     setLED(uint8_t brightness);
void     initSD();
void     loadPrefs();
void     savePrefs();
void     applyScreenRotation(uint8_t rot);
void     drawBtn(int x,int y,int w,int h,const char* lbl,
                 uint16_t bg,uint16_t fg,uint8_t sz);
bool     inRect(int tx,int ty,int rx,int ry,int rw,int rh);
void     drawBackBtn();
bool     isBackBtn(int tx,int ty);
void     printSerialHelp();
bool     handleGlobalSerial(char c);    // lệnh serial toàn cục
void     drawMenu();
void     drawSettingsPage();
void     drawTestListPage();
void     drawStatusBar();
void     handleMenuTouch(int tx,int ty);
void     handleSettingsTouch(int tx,int ty);
void     loopMenu();
void     launchTest(TestID id);
// Trail cursor
void     trailClear();
void     trailAdd(int16_t x,int16_t y);
void     trailDraw();
void     cursorDraw(int16_t x,int16_t y);
void     cursorErase();

// Prototypes phase 1+2
void runTouchCalib();
void runRotationDiscovery();
// Phase 3
void runTestPattern();
void runDefectivePixels();
void runUniformity();
void runColorDistance();
void runGradient();
void runSharpness();
void runViewingAngle();
void runGamma();
void runRGBSwatch();
void runCheckerboard();
// Phase 4+
// (sẽ thêm sau)

// ═══════════════════════════════════════════════════════════════
//  ─── Helper UI ────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void drawBtn(int x,int y,int w,int h,const char* label,
             uint16_t bgColor,uint16_t textColor,uint8_t textSize) {
  tft.fillRoundRect(x,y,w,h,4,bgColor);
  tft.drawRoundRect(x,y,w,h,4,tft.alphaBlend(128,bgColor,TFT_WHITE));
  int strLen = strlen(label)*6*textSize;
  int tx2 = x+(w-strLen)/2;
  int ty2 = y+(h-8*textSize)/2;
  tft.setTextColor(textColor,bgColor);
  tft.setTextSize(textSize);
  tft.setCursor(tx2,ty2);
  tft.print(label);
}

bool inRect(int tx,int ty,int rx,int ry,int rw,int rh){
  return tx>=rx && tx<rx+rw && ty>=ry && ty<ry+rh;
}

// Back button — góc trên-trái, kích thước đủ lớn để chạm dễ
void drawBackBtn(){
  drawBtn(2,2,BACK_BTN_W,BACK_BTN_H,"<Back",TC_BACK(),TFT_WHITE,1);
}
bool isBackBtn(int tx,int ty){
  return inRect(tx,ty,2,2,BACK_BTN_W,BACK_BTN_H);
}

// ═══════════════════════════════════════════════════════════════
//  ─── Serial toàn cục ──────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void printSerialHelp(){
  Serial.printf("\n╔══════════════════════════════════════════╗\n");
  Serial.printf("║  TFT Test Suite — Serial Commands        ║\n");
  Serial.printf("╠══════════════════════════════════════════╣\n");
  Serial.printf("║  GLOBAL (hoat dong o MOI man hinh)       ║\n");
  Serial.printf("║  r  = Xoay man hinh CW (0->1->2->3->0)  ║\n");
  Serial.printf("║  R  = Xoay man hinh CCW                  ║\n");
  Serial.printf("║  t  = Xoay cam ung CW                    ║\n");
  Serial.printf("║  T  = Xoay cam ung CCW                   ║\n");
  Serial.printf("║  w  = Xoay ca hai (man+camung) CW        ║\n");
  Serial.printf("║  W  = Xoay ca hai (man+camung) CCW       ║\n");
  Serial.printf("║  0  = Reset ca hai ve mac dinh           ║\n");
  Serial.printf("║  c  = Mo lai hieu chuan cam ung           ║\n");
  Serial.printf("║  x  = Tat calibration (dung raw map)     ║\n");
  Serial.printf("║  d  = Bat/tat Dark Mode                  ║\n");
  Serial.printf("║  +/-= Tang/giam do sang 10%%              ║\n");
  Serial.printf("╠══════════════════════════════════════════╣\n");
  Serial.printf("║  MENU                                    ║\n");
  Serial.printf("║  1-9= Chon test (trang hien tai)         ║\n");
  Serial.printf("║  n  = Trang ke     p = Trang truoc       ║\n");
  Serial.printf("║  s  = Vao trang Settings                 ║\n");
  Serial.printf("║  b/q= Quay lai menu (trong test)         ║\n");
  Serial.printf("║  ?  = Hien bang nay                      ║\n");
  Serial.printf("╚══════════════════════════════════════════╝\n");
  Serial.printf("[Status] screenRot=%d touchRot=%d bright=%d%% dark=%s\n",
    screenRotation,touchRotation,ledBrightness*100/255,darkMode?"ON":"OFF");
}

// Trả về true nếu đã xử lý lệnh (caller không cần xử lý tiếp)
bool handleGlobalSerial(char c){
  switch(c){
    case 'r': // xoay màn CW
      screenRotation=(screenRotation+1)&3;
      applyScreenRotation(screenRotation);
      savePrefs();
      Serial.printf("[Serial] Xoay man CW: screenRot=%d (touchRot=%d giu)\n",
                    screenRotation,touchRotation);
      menuNeedsRedraw=true; return true;
    case 'R': // xoay màn CCW
      screenRotation=(screenRotation+3)&3;
      applyScreenRotation(screenRotation);
      savePrefs();
      Serial.printf("[Serial] Xoay man CCW: screenRot=%d\n",screenRotation);
      menuNeedsRedraw=true; return true;
    case 't': // xoay touch CW
      touchRotation=(touchRotation+1)&3;
      savePrefs();
      Serial.printf("[Serial] Xoay touch CW: touchRot=%d (screenRot=%d giu)\n",
                    touchRotation,screenRotation);
      menuNeedsRedraw=true; return true;
    case 'T': // xoay touch CCW
      touchRotation=(touchRotation+3)&3;
      savePrefs();
      Serial.printf("[Serial] Xoay touch CCW: touchRot=%d\n",touchRotation);
      menuNeedsRedraw=true; return true;
    case 'w': // xoay cả hai CW
      screenRotation=(screenRotation+1)&3;
      touchRotation=recommendedTouchRot[screenRotation];
      applyScreenRotation(screenRotation);
      savePrefs();
      Serial.printf("[Serial] Xoay ca hai CW: screenRot=%d touchRot=%d\n",
                    screenRotation,touchRotation);
      menuNeedsRedraw=true; return true;
    case 'W': // xoay cả hai CCW
      screenRotation=(screenRotation+3)&3;
      touchRotation=recommendedTouchRot[screenRotation];
      applyScreenRotation(screenRotation);
      savePrefs();
      Serial.printf("[Serial] Xoay ca hai CCW: screenRot=%d touchRot=%d\n",
                    screenRotation,touchRotation);
      menuNeedsRedraw=true; return true;
    case '0': { // reset cả hai
      uint8_t old_sr=screenRotation,old_tr=touchRotation;
      screenRotation=0; touchRotation=recommendedTouchRot[0];
      applyScreenRotation(0); savePrefs();
      Serial.printf("[Serial] Reset: screenRot %d->0 touchRot %d->%d\n",
                    old_sr,old_tr,touchRotation);
      menuNeedsRedraw=true; return true;
    }
    case 'c': // calibration
      Serial.printf("[Serial] Goi hieu chuan cam ung...\n");
      launchTest(TEST_TOUCH_CALIB);
      menuNeedsRedraw=true; return true;
    case 'x': // dùng preset thô (tắt calibration)
      touchCalib.valid=false;
      Serial.printf("[Serial] Tat calibration - dung raw map (200-3900 -> 0-240/320)\n");
      return true;
    case 'd': // dark mode
      darkMode=!darkMode;
      prefs.putBool("dark_mode",darkMode);
      Serial.printf("[Serial] Dark mode: %s\n",darkMode?"ON":"OFF");
      menuNeedsRedraw=true; return true;
    case '+':{ // tăng sáng
      int nb=min((int)ledBrightness+26,(int)LED_BRIGHTNESS_MAX);
      setLED((uint8_t)nb); savePrefs();
      Serial.printf("[Serial] Tang sang: %d%%\n",ledBrightness*100/255);
      menuNeedsRedraw=true; return true;
    }
    case '-':{ // giảm sáng
      int nb=max((int)ledBrightness-26,(int)LED_BRIGHTNESS_MIN);
      setLED((uint8_t)nb); savePrefs();
      Serial.printf("[Serial] Giam sang: %d%%\n",ledBrightness*100/255);
      menuNeedsRedraw=true; return true;
    }
    case '?':
      printSerialHelp(); return true;
    default: return false;
  }
}

// ═══════════════════════════════════════════════════════════════
//  ─── Khởi tạo đèn nền PWM ─────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void initLED(){
  if(TFT_LED_PIN==12){
    Serial.printf("[WARN] GPIO12 la strapping pin! Khuyen nghi doi sang GPIO19.\n");
  }
  Serial.printf("[LED] Pin=GPIO%d, %dHz, 8-bit\n",TFT_LED_PIN,LED_PWM_FREQ);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcAttach(TFT_LED_PIN,LED_PWM_FREQ,LED_PWM_RES);
  ledcWrite(TFT_LED_PIN,ledBrightness);
#else
  ledcSetup(LED_PWM_CHANNEL,LED_PWM_FREQ,LED_PWM_RES);
  ledcAttachPin(TFT_LED_PIN,LED_PWM_CHANNEL);
  ledcWrite(LED_PWM_CHANNEL,ledBrightness);
#endif
}

void setLED(uint8_t brightness){
  ledBrightness=brightness;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcWrite(TFT_LED_PIN,brightness);
#else
  ledcWrite(LED_PWM_CHANNEL,brightness);
#endif
}

// ═══════════════════════════════════════════════════════════════
//  ─── SD card ──────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void initSD(){
  Serial.printf("[SD] Khoi tao SD_CS=GPIO%d...\n",SD_CS_PIN);
  if(!SD.begin(SD_CS_PIN)){
    Serial.printf("[WARN] SD: Khong mount duoc the.\n"); return;
  }
  uint8_t ct=SD.cardType();
  const char* types[]={"NONE","MMC","SD","SDHC","UNKNOWN"};
  Serial.printf("[SD] Loai: %s | %llu MB | Free: %llu MB\n",
    ct<5?types[ct]:"?",
    SD.cardSize()/(1024ULL*1024),
    (SD.totalBytes()-SD.usedBytes())/(1024ULL*1024));
  File root=SD.open("/");
  if(root){ File f=root.openNextFile();
    while(f){ Serial.printf("[SD]  %s%s\n",f.isDirectory()?"[D] ":"    ",f.name()); f=root.openNextFile(); }
    root.close();
  }
}

// ═══════════════════════════════════════════════════════════════
//  ─── Preferences (NVS) ────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void loadPrefs(){
  prefs.begin("tft_suite",false);
  screenRotation=prefs.getUChar("screen_rot",0);
  touchRotation =prefs.getUChar("touch_rot",0);
  recommendedTouchRot[0]=prefs.getUChar("rec_tr0",0);
  recommendedTouchRot[1]=prefs.getUChar("rec_tr1",1);
  recommendedTouchRot[2]=prefs.getUChar("rec_tr2",2);
  recommendedTouchRot[3]=prefs.getUChar("rec_tr3",3);
  ledBrightness=prefs.getUChar("brightness",LED_BRIGHTNESS_DEF);
  ledBrightness=constrain(ledBrightness,LED_BRIGHTNESS_MIN,LED_BRIGHTNESS_MAX);
  darkMode=prefs.getBool("dark_mode",false);
  Serial.printf("[Prefs] screenRot=%d touchRot=%d bright=%d dark=%s\n",
    screenRotation,touchRotation,ledBrightness,darkMode?"ON":"OFF");
}

void savePrefs(){
  prefs.putUChar("screen_rot",screenRotation);
  prefs.putUChar("touch_rot",touchRotation);
  prefs.putUChar("brightness",ledBrightness);
  prefs.putUChar("rec_tr0",recommendedTouchRot[0]);
  prefs.putUChar("rec_tr1",recommendedTouchRot[1]);
  prefs.putUChar("rec_tr2",recommendedTouchRot[2]);
  prefs.putUChar("rec_tr3",recommendedTouchRot[3]);
  prefs.putBool ("dark_mode",darkMode);
}

void applyScreenRotation(uint8_t rot){
  uint8_t newRot = rot & 3;
  bool changed = (newRot != screenRotation);
  screenRotation = newRot;
  tft.setRotation(screenRotation);
  if(changed)   // chỉ in khi rotation thực sự thay đổi (tránh spam)
    Serial.printf("[Rot] screenRot=%d => %dx%d\n",screenRotation,tft.width(),tft.height());
}

// ═══════════════════════════════════════════════════════════════
//  ─── Trail cursor (persistent + trail) ────────────────────────
// ═══════════════════════════════════════════════════════════════
void trailClear(){
  // Xóa tất cả điểm trail cũ bằng màu nền đen
  for(int i=0;i<trailLen;i++){
    tft.drawPixel(trailX[i],trailY[i],TFT_BLACK);
    if(i>0) tft.drawLine(trailX[i-1],trailY[i-1],trailX[i],trailY[i],TFT_BLACK);
  }
  trailLen=0;
}

void trailAdd(int16_t x,int16_t y){
  if(trailLen>0){
    // Vẽ đường nối
    tft.drawLine(trailX[trailLen-1],trailY[trailLen-1],x,y,0x4208); // xám nhạt
  }
  if(trailLen<TRAIL_MAX){
    trailX[trailLen]=x; trailY[trailLen]=y; trailLen++;
  } else {
    // Xóa điểm cũ nhất (shift)
    memmove(trailX,trailX+1,(TRAIL_MAX-1)*sizeof(int16_t));
    memmove(trailY,trailY+1,(TRAIL_MAX-1)*sizeof(int16_t));
    trailX[TRAIL_MAX-1]=x; trailY[TRAIL_MAX-1]=y;
  }
}

// Vẽ cursor + tại vị trí hiện tại (xóa cũ trước)
void cursorDraw(int16_t x,int16_t y){
  // Xóa cursor cũ
  if(cursorX>=0){
    tft.drawFastHLine(cursorX-CURSOR_HALF,cursorY,CURSOR_HALF*2+1,TFT_BLACK);
    tft.drawFastVLine(cursorX,cursorY-CURSOR_HALF,CURSOR_HALF*2+1,TFT_BLACK);
  }
  cursorX=x; cursorY=y;
  // Vẽ cursor mới
  tft.drawFastHLine(x-CURSOR_HALF,y,CURSOR_HALF*2+1,TFT_YELLOW);
  tft.drawFastVLine(x,y-CURSOR_HALF,CURSOR_HALF*2+1,TFT_YELLOW);
  tft.drawPixel(x,y,TFT_WHITE); // tâm sáng hơn
}

// ═══════════════════════════════════════════════════════════════
//  ─── Status bar (hiện ở dưới menu) ───────────────────────────
// ═══════════════════════════════════════════════════════════════
#define STATUS_H 16
void drawStatusBar(){
  int W=tft.width(),H=tft.height();
  int y=H-STATUS_H;
  bool mismatch=(touchRotation!=recommendedTouchRot[screenRotation]);
  uint16_t sbg=mismatch?TFT_RED:0x1082;
  tft.fillRect(0,y,W,STATUS_H,sbg);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE,sbg);
  char buf[64];
  snprintf(buf,sizeof(buf),"Man:R%d | Touch:R%d | %d%% | %s%s",
    screenRotation,touchRotation,ledBrightness*100/255,
    darkMode?"[DK]":"",
    mismatch?" !! LECH !!":" ");
  tft.setCursor(2,y+(STATUS_H-8)/2);
  tft.print(buf);
}

// ═══════════════════════════════════════════════════════════════
//  ─── Trang Settings ───────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void drawSettingsPage(){
  int W=tft.width(),H=tft.height();
  tft.fillScreen(TC_BG());

  // Header + Back button (ở vị trí nút Setting khi ở menu chính)
  tft.fillRect(0,0,W,24,TC_HEADER());
  tft.setTextColor(TFT_WHITE,TC_HEADER());
  tft.setTextSize(1);
  tft.setCursor(4,8); tft.print("= Cai dat & Dieu khien =");
  drawBtn(W-68,2,66,20,"< Back",TC_BACK(),TFT_WHITE,1); // thay vị trí nút Setting

  int y=28, bh=34, bw2=(W-8)/2, mg=4;

  // Độ sáng
  tft.setTextColor(TFT_CYAN,TC_BG());
  tft.setCursor(4,y); tft.print("Do sang:"); y+=12;
  char br[12]; snprintf(br,sizeof(br),"- %d%%",ledBrightness*100/255);
  char brp[8]; snprintf(brp,sizeof(brp),"%d%% +",ledBrightness*100/255);
  drawBtn(mg,  y,bw2,bh,br,  TC_NAV(),TFT_WHITE,1);
  drawBtn(W/2+mg,y,bw2,bh,brp,TC_NAV(),TFT_WHITE,1);
  y+=bh+mg;

  // Xoay màn / touch
  drawBtn(mg,     y,bw2,bh,"< Man",    TC_ROT(),TFT_WHITE,1);
  drawBtn(W/2+mg, y,bw2,bh,"Man >",    TC_ROT(),TFT_WHITE,1);
  y+=bh+mg;
  drawBtn(mg,     y,bw2,bh,"< Touch",  TC_ROT(),TFT_WHITE,1);
  drawBtn(W/2+mg, y,bw2,bh,"Touch >",  TC_ROT(),TFT_WHITE,1);
  y+=bh+mg;
  drawBtn(mg,     y,bw2,bh,"Xoay Ca Hai", TC_ROT(),TFT_WHITE,1);
  drawBtn(W/2+mg, y,bw2,bh,"Reset Xoay",  TC_BACK(),TFT_WHITE,1);
  y+=bh+mg;

  // Calibration / Dark mode
  drawBtn(mg,     y,bw2,bh,"Hieu Chuan CU",TC_CALIB(),TFT_WHITE,1);
  char dmLbl[16]; snprintf(dmLbl,sizeof(dmLbl),"Dark:%s",darkMode?"ON":"OFF");
  drawBtn(W/2+mg, y,bw2,bh,dmLbl,       darkMode?0x8410:TC_NAV(),TFT_WHITE,1);
  y+=bh+mg;

  // Thông tin hệ thống
  tft.setTextSize(1); tft.setTextColor(0xBDF7,TC_BG());
  char info[48];
  snprintf(info,sizeof(info),"Driver:%s  %dx%d  GPIO%d",
    driverName(),tft.width(),tft.height(),TFT_LED_PIN);
  tft.setCursor(4,y); tft.print(info);
  y+=12;
  snprintf(info,sizeof(info),"Calib:%s  Rec[%d%d%d%d]",
    touchCalib.valid?"OK":"CHUA",
    recommendedTouchRot[0],recommendedTouchRot[1],
    recommendedTouchRot[2],recommendedTouchRot[3]);
  tft.setCursor(4,y); tft.print(info);

  drawStatusBar();
}

void handleSettingsTouch(int tx,int ty){
  int W=tft.width();
  int bh=34,bw2=(W-8)/2,mg=4;
  int y=40; // sau header+label

  // Brightness -
  if(inRect(tx,ty,mg,y,bw2,bh)){
    int nb=max((int)ledBrightness-26,(int)LED_BRIGHTNESS_MIN);
    setLED((uint8_t)nb); savePrefs();
    Serial.printf("[Settings] Sang: %d%%\n",ledBrightness*100/255);
    menuNeedsRedraw=true; return;
  }
  // Brightness +
  if(inRect(tx,ty,W/2+mg,y,bw2,bh)){
    int nb=min((int)ledBrightness+26,(int)LED_BRIGHTNESS_MAX);
    setLED((uint8_t)nb); savePrefs();
    Serial.printf("[Settings] Sang: %d%%\n",ledBrightness*100/255);
    menuNeedsRedraw=true; return;
  }
  y+=bh+mg;
  // Man CCW
  if(inRect(tx,ty,mg,y,bw2,bh)){
    screenRotation=(screenRotation+3)&3; applyScreenRotation(screenRotation);
    savePrefs(); Serial.printf("[Settings] Man CCW: R%d\n",screenRotation);
    menuNeedsRedraw=true; return;
  }
  // Man CW
  if(inRect(tx,ty,W/2+mg,y,bw2,bh)){
    screenRotation=(screenRotation+1)&3; applyScreenRotation(screenRotation);
    savePrefs(); Serial.printf("[Settings] Man CW: R%d\n",screenRotation);
    menuNeedsRedraw=true; return;
  }
  y+=bh+mg;
  // Touch CCW
  if(inRect(tx,ty,mg,y,bw2,bh)){
    touchRotation=(touchRotation+3)&3; savePrefs();
    Serial.printf("[Settings] Touch CCW: R%d\n",touchRotation);
    menuNeedsRedraw=true; return;
  }
  // Touch CW
  if(inRect(tx,ty,W/2+mg,y,bw2,bh)){
    touchRotation=(touchRotation+1)&3; savePrefs();
    Serial.printf("[Settings] Touch CW: R%d\n",touchRotation);
    menuNeedsRedraw=true; return;
  }
  y+=bh+mg;
  // Xoay cả hai
  if(inRect(tx,ty,mg,y,bw2,bh)){
    screenRotation=(screenRotation+1)&3;
    touchRotation=recommendedTouchRot[screenRotation];
    applyScreenRotation(screenRotation); savePrefs();
    Serial.printf("[Settings] Ca hai CW: screenR%d touchR%d\n",screenRotation,touchRotation);
    menuNeedsRedraw=true; return;
  }
  // Reset xoay
  if(inRect(tx,ty,W/2+mg,y,bw2,bh)){
    screenRotation=0; touchRotation=recommendedTouchRot[0];
    applyScreenRotation(0); savePrefs();
    Serial.printf("[Settings] Reset xoay\n");
    menuNeedsRedraw=true; return;
  }
  y+=bh+mg;
  // Hiệu chuẩn
  if(inRect(tx,ty,mg,y,bw2,bh)){
    inSettings=false; launchTest(TEST_TOUCH_CALIB);
    menuNeedsRedraw=true; return;
  }
  // Dark mode
  if(inRect(tx,ty,W/2+mg,y,bw2,bh)){
    darkMode=!darkMode; savePrefs();
    Serial.printf("[Settings] Dark mode: %s\n",darkMode?"ON":"OFF");
    menuNeedsRedraw=true; return;
  }
  // Back button: ở góc phải header (W-68 đến W, y=2..22) — khớp drawSettingsPage
  if(inRect(tx,ty,tft.width()-68,2,66,20)){
    inSettings=false; menuNeedsRedraw=true;
  }
}

// ═══════════════════════════════════════════════════════════════
//  ─── Trang test list ──────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
#define BTN_H  44    // nút cao hơn để dễ chạm
#define BTN_MG  4

void drawTestListPage(){
  int W=tft.width(),H=tft.height();
  tft.fillScreen(TC_BG());

  // Header
  tft.fillRect(0,0,W,24,TC_HEADER());
  tft.setTextColor(TFT_WHITE,TC_HEADER());
  tft.setTextSize(1);
  char hdr[48];
  snprintf(hdr,sizeof(hdr),"TFT Test Suite | Trang %d/%d",menuPage+1,menuTestPages);
  tft.setCursor(4,8); tft.print(hdr);

  // Nút Settings ở header (góc phải)
  drawBtn(W-64,2,62,20,"Setting",TC_ROT(),TFT_WHITE,1);

  // Prev / Next (đặt TRƯỚC để tính giới hạn cho nút test)
  int navY=H-STATUS_H-28;
  int bnw=(W/2)-4;
  drawBtn(2,navY,bnw,24,"< Prev",menuPage>0?TC_NAV():0x1084,TFT_WHITE,1);
  drawBtn(W/2+2,navY,bnw,24,"Next >",menuPage<menuTestPages-1?TC_NAV():0x1084,TFT_WHITE,1);

  // Nút test — tính chiều cao động để không tràn vào Prev/Next
  int startIdx=menuPage*MENU_ITEMS_PER_PAGE;
  int endIdx=min(startIdx+MENU_ITEMS_PER_PAGE,TEST_COUNT_LIST);
  int itemCount=endIdx-startIdx;
  int bw=W-BTN_MG*2;
  int startY=28;
  int bh=BTN_H;
  if(itemCount>0){
    int maxH=navY-startY-2;  // chiều cao tối đa cho tất cả nút test
    bh=(maxH-(itemCount-1)*(int)BTN_MG)/itemCount;
    bh=constrain(bh,20,(int)BTN_H);
  }
  for(int i=startIdx;i<endIdx;i++){
    int row=i-startIdx;
    int by=startY+row*(bh+BTN_MG);
    char lbl[32];
    snprintf(lbl,sizeof(lbl),"%d. %s",i+1,TEST_LIST[i].label);
    drawBtn(BTN_MG,by,bw,bh,lbl,TC_BTN(),TFT_WHITE,1);
  }

  drawStatusBar();
}

void drawMenu(){
  if(inSettings) drawSettingsPage();
  else           drawTestListPage();
  menuNeedsRedraw=false;
}

// Xử lý chạm test list
void handleTestListTouch(int tx,int ty){
  int W=tft.width(),H=tft.height();
  int startIdx=menuPage*MENU_ITEMS_PER_PAGE;
  int endIdx=min(startIdx+MENU_ITEMS_PER_PAGE,TEST_COUNT_LIST);
  int itemCount=endIdx-startIdx;
  int bw=W-BTN_MG*2;
  int startY=28;
  // tính bh khớp với drawTestListPage
  int navY=H-STATUS_H-28, bnw=(W/2)-4;
  int bh=BTN_H;
  if(itemCount>0){
    int maxH=navY-startY-2;
    bh=(maxH-(itemCount-1)*(int)BTN_MG)/itemCount;
    bh=constrain(bh,20,(int)BTN_H);
  }
  for(int i=startIdx;i<endIdx;i++){
    int row=i-startIdx;
    int by=startY+row*(bh+BTN_MG);
    if(inRect(tx,ty,BTN_MG,by,bw,bh)){
      Serial.printf("[Menu] Chon: %s\n",TEST_LIST[i].serial);
      launchTest(TEST_LIST[i].id);
      return;
    }
  }
  // Settings button
  if(inRect(tx,ty,W-64,2,62,20)){
    inSettings=true; menuNeedsRedraw=true; return;
  }
  // Prev/Next
  if(inRect(tx,ty,2,navY,bnw,24)&&menuPage>0){ menuPage--; menuNeedsRedraw=true; }
  if(inRect(tx,ty,W/2+2,navY,bnw,24)&&menuPage<menuTestPages-1){ menuPage++; menuNeedsRedraw=true; }
}

// ─── Serial menu ───────────────────────────────────────────────
void handleMenuSerial(char c){
  if(c>='1'&&c<='9'){
    int idx=(c-'1')+menuPage*MENU_ITEMS_PER_PAGE;
    if(idx<TEST_COUNT_LIST){ Serial.printf("[Menu] Serial chon: %s\n",TEST_LIST[idx].serial);
      launchTest(TEST_LIST[idx].id); return; }
  }
  if(c=='n'||c=='N'){ if(menuPage<menuTestPages-1){ menuPage++; menuNeedsRedraw=true; } return; }
  if(c=='p'||c=='P'){ if(menuPage>0){ menuPage--; menuNeedsRedraw=true; } return; }
  if(c=='s'||c=='S'){ inSettings=!inSettings; menuNeedsRedraw=true; return; }
  handleGlobalSerial(c);
}

// ═══════════════════════════════════════════════════════════════
//  ─── Loop menu ────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void loopMenu(){
  static uint32_t lastTouchMs=0;
  if(menuNeedsRedraw){
    int16_t prevCX=cursorX, prevCY=cursorY;  // lưu vị trí cursor trước redraw
    drawMenu(); trailClear(); cursorX=-1;
    if(prevCX>=0) cursorDraw(prevCX,prevCY); // vẽ lại cursor sau redraw
  }

  // Poll serial
  while(Serial.available()){
    char c=(char)Serial.read();
    handleMenuSerial(c);
  }

  // Poll touch
  if(millis()-lastTouchMs>TOUCH_DEBOUNCE_MS){
    TouchPoint tp=touchRead();
    if(tp.touched){
      lastTouchMs=millis();
      if(!wasTouch){
        // Bắt đầu chạm mới → xóa trail cũ
        trailClear();
      }
      wasTouch=true;
      trailAdd(tp.x,tp.y);
      cursorDraw(tp.x,tp.y);
      // Xử lý chạm (chỉ khi nhấn, không kéo)
      if(inSettings) handleSettingsTouch(tp.x,tp.y);
      else           handleTestListTouch(tp.x,tp.y);
    } else {
      if(wasTouch){
        // Vừa nhấc tay — cursor + trail vẫn hiển thị (persistent)
        // Cursor vẫn ở cursorX,cursorY cho đến lần chạm tiếp theo
      }
      wasTouch=false;
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  ─── Launch test ──────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════
void launchTest(TestID id){
  currentTest=id;
  appState=STATE_TEST_RUNNING;
  tft.fillScreen(TFT_BLACK);
  trailClear(); cursorX=-1;

  switch(id){
    case TEST_TOUCH_CALIB:      runTouchCalib();        break;
    case TEST_ROTATION_DISC:    runRotationDiscovery(); break;
    case TEST_PATTERN:          runTestPattern();       break;
    case TEST_DEFECTIVE_PIXELS: runDefectivePixels();   break;
    case TEST_UNIFORMITY:       runUniformity();        break;
    case TEST_COLOR_DISTANCE:   runColorDistance();     break;
    case TEST_GRADIENT:         runGradient();          break;
    case TEST_SHARPNESS:        runSharpness();         break;
    case TEST_VIEWING_ANGLE:    runViewingAngle();      break;
    case TEST_GAMMA:            runGamma();             break;
    case TEST_RGB_SWATCH:       runRGBSwatch();         break;
    case TEST_CHECKERBOARD:     runCheckerboard();      break;
    default: {
      // Stub — KHÔNG freeze: có đầy đủ serial + touch fallback
      const char* name=(int)id-1<TEST_COUNT_LIST?TEST_LIST[(int)id-1].serial:"?";
      Serial.printf("[%s] Bat dau (chua implement)\n",name);
      Serial.printf("[%s] Goc lenh: b/q=quay lai, r/R/t/T/w/0=xoay, ?=help\n",name);
      tft.fillScreen(TC_BG());
      tft.setTextColor(TFT_WHITE,TC_BG());
      tft.setTextSize(1);
      tft.setCursor(10,30); tft.printf("Module %d: chua implement",(int)id);
      tft.setCursor(10,46); tft.print("Phase tiep theo se co.");
      tft.setCursor(10,62); tft.print("Cham goc tren-trai de quay lai");
      tft.setCursor(10,78); tft.print("Moi lenh serial van hoat dong.");
      drawStatusBar();
      // ── Loop chờ Back — KHÔNG freeze ──
      uint32_t lastT=0;
      while(true){
        while(Serial.available()){
          char c=(char)Serial.read();
          if(c=='b'||c=='B'||c=='q'||c=='Q') goto stubExit;
          handleGlobalSerial(c);
          // Cập nhật status bar nếu có thay đổi
          drawStatusBar();
        }
        if(millis()-lastT>TOUCH_DEBOUNCE_MS){
          lastT=millis();
          TouchPoint tp=touchRead();
          if(tp.touched&&isBackBtn(tp.x,tp.y)) goto stubExit;
        }
        delay(20);
      }
      stubExit:
      Serial.printf("[%s] Ket thuc (stub)\n",name);
      break;
    }
  }
  currentTest=TEST_NONE;
  appState=STATE_MENU;
  applyScreenRotation(screenRotation);
  menuNeedsRedraw=true;
}

// ═══════════════════════════════════════════════════════════════
//  setup() & loop()
// ═══════════════════════════════════════════════════════════════
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n╔══════════════════════════════════════╗\n");
  Serial.printf("║   TFT Test Suite                     ║\n");
  Serial.printf("║   Goc '?' de xem huong dan serial    ║\n");
  Serial.printf("╚══════════════════════════════════════╝\n");

  initLED();
  loadPrefs();
  setLED(ledBrightness);

  tft.init();
  applyScreenRotation(screenRotation);
  tft.fillScreen(TFT_BLACK);

  // Boot splash
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.setTextSize(1);
  int y=10;
  #define LINE(fmt,...) tft.setCursor(4,y); tft.printf(fmt,##__VA_ARGS__); y+=13
  LINE("TFT Test Suite");
  LINE("Driver: %s  %dx%d",driverName(),tft.width(),tft.height());
  LINE("LED: GPIO%d  Sang:%d%%",TFT_LED_PIN,ledBrightness*100/255);
  LINE("Dark: %s",darkMode?"ON":"OFF");
  #undef LINE

  initSD();
  touchInit();
  bool calibOK=touchLoadCalib(prefs,touchCalib);
  tft.setTextColor(calibOK?TFT_GREEN:TFT_YELLOW,TFT_BLACK);
  tft.setCursor(4,y); y+=13;
  tft.print(calibOK?"Cam ung: DA HIEU CHUAN":"Cam ung: CHUA HIEU CHUAN");

  menuTestPages=(TEST_COUNT_LIST+MENU_ITEMS_PER_PAGE-1)/MENU_ITEMS_PER_PAGE;
  menuPage=0;
  delay(1200);

  if(!calibOK){ launchTest(TEST_TOUCH_CALIB); }

  appState=STATE_MENU;
  menuNeedsRedraw=true;
  printSerialHelp();
}

void loop(){
  switch(appState){
    case STATE_MENU:         loopMenu(); break;
    case STATE_TEST_RUNNING: appState=STATE_MENU; menuNeedsRedraw=true; break;
    case STATE_CALIBRATION:  launchTest(TEST_TOUCH_CALIB); appState=STATE_MENU; menuNeedsRedraw=true; break;
  }
  delay(5);
}
