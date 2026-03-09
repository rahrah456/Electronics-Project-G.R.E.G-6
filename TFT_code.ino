#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>

// =====================
// TFT (ILI9341) SPI pins
// =====================
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================
// Touch pins (MUST match your wiring)
// X+, X-, Y+, Y- on the TFT must go to these Arduino pins
// =====================
#define YP A2
#define XM A3
#define YM 5
#define XP 4

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// =====================
// Touch calibration (you will likely need to tune these)
// =====================
#define TS_MINX 150
#define TS_MAXX 920
#define TS_MINY 120
#define TS_MAXY 900

// Pressure thresholds
#define MINPRESSURE 1
#define MAXPRESSURE 1000

// =====================
// UI model
// =====================
const char* names[4] = {"Kaya", "Rahul", "Ethan", "Igor"};
int selected = -1;

struct Btn { int16_t x, y, w, h; };
Btn nameBtns[4];
Btn goBtn;

bool hit(const Btn& b, int16_t px, int16_t py) {
  return (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h);
}

void drawButton(const Btn& b, const char* label, bool isSelected) {
  uint16_t fill   = isSelected ? ILI9341_GREEN : ILI9341_DARKGREY;
  uint16_t border = ILI9341_WHITE;
  uint16_t text   = ILI9341_WHITE;

  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 10, border);

  tft.setTextSize(2);
  tft.setTextColor(text);

  // Basic centering-ish (good enough for these names)
  int16_t tx = b.x + 12;
  int16_t ty = b.y + (b.h / 2) - 8;
  tft.setCursor(tx, ty);
  tft.print(label);
}

void redrawUI() {
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 8);
  tft.print("Select User:");

  // Screen is 320x240 in rotation(1)
  // Layout: 2 columns x 2 rows for names, GO at bottom.
  int16_t margin = 10;
  int16_t top = 35;
  int16_t gap = 10;

  int16_t bw = (tft.width() - (2 * margin) - gap) / 2;  // two columns
  int16_t bh = 55;                                      // button height

  // Row 0
  nameBtns[0] = {margin,                 top, bw, bh};
  nameBtns[1] = {(int16_t)(margin+bw+gap), top, bw, bh};
  // Row 1
  nameBtns[2] = {margin,                 (int16_t)(top+bh+gap), bw, bh};
  nameBtns[3] = {(int16_t)(margin+bw+gap), (int16_t)(top+bh+gap), bw, bh};

  for (int i = 0; i < 4; i++) {
    drawButton(nameBtns[i], names[i], selected == i);
  }

  // GO button at bottom
  int16_t goY = top + (2 * bh) + (2 * gap) + 5;
  goBtn = {margin, goY, (int16_t)(tft.width() - 2 * margin), 55};
  drawButton(goBtn, "GO", false);

  // Optional: show currently selected at the bottom
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 220);
  tft.print("Chosen: ");
  if (selected >= 0) tft.print(names[selected]);
  else tft.print("-");
}

void setup() {
  Serial.begin(9600);

  tft.begin();
  tft.setRotation(1);
  redrawUI();

  Serial.println("Touch UI ready.");
}

void loop() {
  TSPoint p = ts.getPoint();

  // IMPORTANT: restore ALL touch pins after reading
  pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  pinMode(YM, OUTPUT);

  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return;

  // ---- Debug (uncomment to see raw values) ----
  // Serial.print("RAW x="); Serial.print(p.x);
  // Serial.print(" y="); Serial.print(p.y);
  // Serial.print(" z="); Serial.println(p.z);

  // Convert raw touch -> screen coords.
  // For rotation(1), these are *often* correct, but may need swapping/inverting.
  int16_t screenX = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
  int16_t screenY = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());

  // ---- Debug mapped coords ----
  // Serial.print("MAP X="); Serial.print(screenX);
  // Serial.print(" Y="); Serial.println(screenY);

  delay(180); // debounce

  for (int i = 0; i < 4; i++) {
    if (hit(nameBtns[i], screenX, screenY)) {
      selected = i;
      redrawUI();
      Serial.print("Selected: ");
      Serial.println(names[i]);
      return;
    }
  }

  if (hit(goBtn, screenX, screenY)) {
    Serial.println("GO pressed (no action yet).");
    // Later: use 'selected' to choose a node/robot behavior
    return;
  }
}