/*
  LD2450 "Heartbeat Sensor" Radar for CYD2USB (ESP32-2432S028 2.8" - two USB version)
  Library: LovyanGFX v1.x
  Board:   "ESP32 Dev Module"

  Core 0 -> display task (rendering)
  Core 1 -> radar task   (LD2450 UART / simulator)

  Wiring (LD2450 -> CYD):
    LD2450 5V  -> CYD 5V   (P1 connector "VIN")
    LD2450 GND -> CYD GND
    LD2450 TX  -> GPIO27   (CN1 connector)
    LD2450 RX  -> GPIO22   (CN1 connector)
*/

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ======================= USER SETTINGS =======================
#define SIMULATE           false    // true = fake targets, false = real LD2450

#define PANEL_INVERT       false   // flip if colors look inverted
#define SCREEN_ROTATION    1       // 1 or 3 for landscape
#define MIRROR_X           false   // flip left/right if targets are on the wrong side

#define RADAR_RX_PIN       27      // ESP32 RX  <- LD2450 TX
#define RADAR_TX_PIN       22      // ESP32 TX  -> LD2450 RX
#define RADAR_BAUD         256000
#define FORCE_MULTI_TARGET true    // put LD2450 in multi-target mode at boot

#define MAX_RANGE_MM       6000    // outer ring distance
#define RING_COUNT         6       // rings (6 = one per metre at 6 m)
#define FOV_DEG            90      // half-angle of the drawn rings (90 = flat bottom)

#define PULSE_TRAVEL_MS    1400    // time for pulse to reach the outer ring
#define PULSE_PERIOD_MS    2000    // time between pulses
#define PULSE_WAKE_PX      28      // length of the fading tail behind the pulse
#define BLIP_FADE_MS       1800    // how long a target stays bright after a hit
#define BLIP_MIN           0.30f   // minimum target brightness between pulses (0 = vanish)

#define DISPLAY_CORE       0
#define RADAR_CORE         1
// =============================================================

// ---------------- LovyanGFX config for CYD2USB ----------------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;   // use Panel_ILI9341 if your board is the 1-USB CYD
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 80000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc   = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable   = true;
      cfg.invert     = PANEL_INVERT;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = 21;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

static LGFX tft;
static LGFX_Sprite stripA(&tft);
static LGFX_Sprite stripB(&tft);
LGFX_Sprite *strips[2] = { &stripA, &stripB };
LGFX_Sprite *canvas = &stripA;     // strip currently being drawn
HardwareSerial RadarSerial(2);

// ---------------- Layout ----------------
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
constexpr int STRIP_H  = 40;       // screen is rendered in 6 strips (double-buffered)
constexpr int CX = 160;            // radar origin
constexpr int CY = 226;
constexpr int RADAR_R  = 150;      // outer ring radius (px)
constexpr int ORIGIN_R = 12;       // origin circle radius
const float TAN_FOV = tanf(min(FOV_DEG, 89) * DEG_TO_RAD);

int oy = 0;                        // y offset of the strip being drawn

// ---------------- Colors ----------------
struct RGB { uint8_t r, g, b; };

const RGB BG_RGB      = { 24, 104, 116};   // single background color
const RGB OUTLINE_RGB = {  0,  18,  26};   // dark outline around top text
const RGB LINE_RGB    = {110, 200, 200};
const RGB LABEL_RGB   = {170, 235, 232};
const RGB OUTER_RGB   = {180, 248, 242};
const RGB PULSE_RGB   = {200, 255, 248};
const RGB DOT_RGB     = {255, 215, 190};

// colors of the blips on the radar
const RGB TARGET_RGB[3] = { {255, 0, 0}, {255, 255, 0}, {0, 255, 0} };
// colors used for the readouts at the top
const RGB HUD_RGB[3]    = { {255, 0, 0}, {255, 255, 0}, {0, 255, 0} };

uint16_t bgSwap;                   // byte-swapped background for direct buffer writes

static inline uint16_t c565(RGB c) {
  return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
}

static inline uint16_t swap16(uint16_t c) { return (c >> 8) | (c << 8); }

static inline RGB lerpRGB(RGB a, RGB b, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return { (uint8_t)(a.r + (b.r - a.r) * t),
           (uint8_t)(a.g + (b.g - a.g) * t),
           (uint8_t)(a.b + (b.b - a.b) * t) };
}

static inline uint16_t mix565(RGB a, RGB b, float t) { return c565(lerpRGB(a, b, t)); }

// =============================================================
//                 SHARED DATA (core 1 -> core 0)
// =============================================================
struct TargetData {
  bool    valid = false;
  int16_t x = 0, y = 0;      // mm
  int16_t speed = 0;         // cm/s
  float   dist = 0;          // mm
  float   angle = 0;         // deg, + = right
};

TargetData   sharedTargets[3];
uint32_t     sharedFrameMs = 0;
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

// =============================================================
//                 RADAR SIDE (runs on core 1)
// =============================================================
static inline int16_t decodeSigned(uint16_t raw) {
  return (raw & 0x8000) ? (int16_t)(raw & 0x7FFF) : -(int16_t)(raw & 0x7FFF);
}

void sendRadarCmd(const uint8_t *data, uint16_t len) {
  const uint8_t head[4] = {0xFD, 0xFC, 0xFB, 0xFA};
  const uint8_t tail[4] = {0x04, 0x03, 0x02, 0x01};
  RadarSerial.write(head, 4);
  RadarSerial.write(len & 0xFF);
  RadarSerial.write(len >> 8);
  RadarSerial.write(data, len);
  RadarSerial.write(tail, 4);
  RadarSerial.flush();
  vTaskDelay(pdMS_TO_TICKS(100));
}

void setMultiTargetMode() {
  const uint8_t enableCfg[] = {0xFF, 0x00, 0x01, 0x00};
  const uint8_t multi[]     = {0x90, 0x00};
  const uint8_t endCfg[]    = {0xFE, 0x00};
  sendRadarCmd(enableCfg, sizeof(enableCfg));
  sendRadarCmd(multi, sizeof(multi));
  sendRadarCmd(endCfg, sizeof(endCfg));
  while (RadarSerial.available()) RadarSerial.read();
}

// Frame: AA FF 03 00 | 3 x 8 bytes | 55 CC  (30 bytes)
uint8_t frameBuf[30];
uint8_t frameIdx = 0;

void parseFrame() {
  TargetData tmp[3];
  for (int i = 0; i < 3; i++) {
    const uint8_t *p = frameBuf + 4 + i * 8;
    uint16_t rx = p[0] | (p[1] << 8);
    uint16_t ry = p[2] | (p[3] << 8);
    uint16_t rs = p[4] | (p[5] << 8);
    uint16_t rr = p[6] | (p[7] << 8);

    if (rx == 0 && ry == 0 && rs == 0 && rr == 0) continue;   // empty slot

    TargetData &t = tmp[i];
    t.valid = true;
    t.x = decodeSigned(rx);
    t.y = decodeSigned(ry);
    t.speed = decodeSigned(rs);
    t.dist = sqrtf((float)t.x * t.x + (float)t.y * t.y);
    t.angle = degrees(atan2f(MIRROR_X ? -t.x : t.x, t.y));
  }

  uint32_t now = millis();
  taskENTER_CRITICAL(&dataMux);
  memcpy(sharedTargets, tmp, sizeof(tmp));
  sharedFrameMs = now;
  taskEXIT_CRITICAL(&dataMux);
}

void readRadar() {
  static const uint8_t hdr[4] = {0xAA, 0xFF, 0x03, 0x00};
  while (RadarSerial.available()) {
    uint8_t b = RadarSerial.read();
    if (frameIdx < 4) {
      if (b == hdr[frameIdx]) {
        frameBuf[frameIdx++] = b;
      } else {
        frameIdx = (b == hdr[0]) ? 1 : 0;
        if (frameIdx) frameBuf[0] = b;
      }
      continue;
    }
    frameBuf[frameIdx++] = b;
    if (frameIdx == 30) {
      if (frameBuf[28] == 0x55 && frameBuf[29] == 0xCC) parseFrame();
      frameIdx = 0;
    }
  }
}

// ---------------- Simulator (no LD2450 needed) ----------------
static inline uint16_t encodeSigned(int v) {
  return v >= 0 ? (uint16_t)(0x8000 | v) : (uint16_t)(-v);
}

static inline void put16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

void simulateRadar(uint32_t now) {
  static uint32_t lastSim = 0;
  static float prevDist[3] = {0, 0, 0};
  if (now - lastSim < 100) return;          // ~10 frames per second like the real sensor
  lastSim = now;

  float t = now / 1000.0f;

  // fake target paths in mm (x = left/right, y = forward)
  float pos[3][2] = {
    { 1500 * sinf(t * 0.5f),  2500 },                          // walks left-right at 2.5 m
    { -800,                   3500 + 2000 * sinf(t * 0.3f) },  // walks toward/away
    { 1200 * cosf(t * 0.4f),  4000 + 1200 * sinf(t * 0.4f) },  // walks in a circle
  };
  bool present[3] = { true, true, fmodf(t, 10.0f) < 7.0f };    // T3 vanishes 3 s out of 10

  frameBuf[0] = 0xAA; frameBuf[1] = 0xFF; frameBuf[2] = 0x03; frameBuf[3] = 0x00;
  for (int i = 0; i < 3; i++) {
    uint8_t *p = frameBuf + 4 + i * 8;
    if (!present[i]) {
      memset(p, 0, 8);
      continue;
    }
    int x = (int)pos[i][0];
    int y = (int)pos[i][1];
    float d = sqrtf((float)x * x + (float)y * y);
    int speed = (int)(d - prevDist[i]);     // mm per 100 ms = cm/s
    prevDist[i] = d;

    put16(p + 0, encodeSigned(x));
    put16(p + 2, encodeSigned(y));
    put16(p + 4, encodeSigned(speed));
    put16(p + 6, 360);
  }
  frameBuf[28] = 0x55; frameBuf[29] = 0xCC;

  parseFrame();
}

void radarTask(void *) {
#if SIMULATE
  for (;;) {
    simulateRadar(millis());
    vTaskDelay(pdMS_TO_TICKS(5));
  }
#else
  RadarSerial.setRxBufferSize(2048);
  RadarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  vTaskDelay(pdMS_TO_TICKS(200));
  if (FORCE_MULTI_TARGET) setMultiTargetMode();

  for (;;) {
    readRadar();
    vTaskDelay(pdMS_TO_TICKS(2));           // ~50 bytes arrive per 2 ms, buffer holds 2048
  }
#endif
}

// =============================================================
//                 DISPLAY SIDE (runs on core 0)
// =============================================================
struct Blip {                // per-frame drawing data
  bool     show = false;
  bool     onScreen = false;
  int      sx = 0, sy = 0;
  float    glow = 0;
  uint32_t age = 0;
  float    dist = 0;
  int      angle = 0;
  int      speed = 0;
};

TargetData view[3];          // snapshot of sharedTargets for this frame
uint32_t   viewFrameMs = 0;
uint32_t   lastHit[3] = {0, 0, 0};
Blip       blips[3];

uint32_t pulseStart = 0;
float pulseR = -1, prevPulseR = -1;

void takeSnapshot() {
  taskENTER_CRITICAL(&dataMux);
  memcpy(view, sharedTargets, sizeof(view));
  viewFrameMs = sharedFrameMs;
  taskEXIT_CRITICAL(&dataMux);
}

void updatePulse(uint32_t now) {
  uint32_t t = (now - pulseStart) % PULSE_PERIOD_MS;
  prevPulseR = pulseR;
  if (t < PULSE_TRAVEL_MS) {
    pulseR = (RADAR_R + PULSE_WAKE_PX) * (float)t / PULSE_TRAVEL_MS;
  } else {
    pulseR = -1;
  }
}

void updateTargets(uint32_t now) {
  bool stale = (now - viewFrameMs) > 1000;
  float from = (prevPulseR < 0 || prevPulseR > pulseR) ? 0 : prevPulseR;

  for (int i = 0; i < 3; i++) {
    const TargetData &t = view[i];
    Blip &b = blips[i];
    b.show = t.valid && !stale;
    if (!b.show) continue;

    float s = (float)RADAR_R / MAX_RANGE_MM;
    float xmm = MIRROR_X ? -t.x : t.x;
    b.sx = CX + (int)(xmm * s);
    b.sy = CY - (int)(t.y * s);
    float tr = t.dist * s;
    b.onScreen = tr <= RADAR_R + 4;

    // pulse just swept past this target's distance -> flash
    if (pulseR >= 0 && tr > from && tr <= pulseR) lastHit[i] = now;

    b.age = now - lastHit[i];
    float fade = 1.0f - (float)b.age / BLIP_FADE_MS;
    if (fade < 0) fade = 0;
    b.glow  = BLIP_MIN + (1.0f - BLIP_MIN) * fade;
    b.dist  = t.dist;
    b.angle = (int)t.angle;
    b.speed = t.speed;
  }
}

// ---------------- Drawing (all coordinates shifted by oy) ----------------
void drawBackground() {
  uint16_t *buf = (uint16_t *)canvas->getBuffer();
  for (int i = 0; i < SCREEN_W * STRIP_H; i++) buf[i] = bgSwap;
}

void drawPulse() {
  if (pulseR < 0) return;
  float rOut = min(pulseR + 3.0f, (float)RADAR_R - 2);
  float rIn  = max(pulseR - (float)PULSE_WAKE_PX, (float)ORIGIN_R);
  if (rIn >= rOut) return;

  float intensity = 1.0f - 0.45f * min(pulseR / RADAR_R, 1.0f);
  uint16_t *buf = (uint16_t *)canvas->getBuffer();

  for (int row = 0; row < STRIP_H; row++) {
    int sy = oy + row;
    int dy = CY - sy;
    if (dy < 0 || dy > rOut) continue;

    int xOut   = (int)sqrtf(rOut * rOut - (float)dy * dy);
    int xWedge = (FOV_DEG >= 90) ? xOut : (int)(dy * TAN_FOV);
    int xMax   = min(xOut, xWedge);
    int xMin   = (dy < rIn) ? (int)sqrtf(rIn * rIn - (float)dy * dy) : 0;
    uint16_t *line = buf + row * SCREEN_W;

    for (int dx = xMin; dx <= xMax; dx++) {
      float r = sqrtf((float)(dx * dx + dy * dy));
      float d = r - pulseR;
      float a;
      if (d >= 0) {
        a = 1.0f - d / 3.0f;                      // sharp leading edge
      } else {
        float t = 1.0f + d / PULSE_WAKE_PX;       // soft fading wake
        a = t * t * 0.5f;
      }
      if (a <= 0.02f) continue;
      uint16_t col = swap16(mix565(BG_RGB, PULSE_RGB, a * intensity));
      line[CX + dx] = col;
      if (dx) line[CX - dx] = col;
    }
  }
}

void drawGrid() {
  int cy = CY - oy;
  uint16_t lineCol = c565(LINE_RGB);

  // inner rings
  for (int k = 1; k < RING_COUNT; k++) {
    int rr = RADAR_R * k / RING_COUNT;
    canvas->fillArc(CX, cy, rr, rr - 1, 270 - FOV_DEG, 270 + FOV_DEG, lineCol);
  }

  // spokes every 30 degrees (the +/-90 ones form the flat baseline)
  for (int a = -FOV_DEG; a <= FOV_DEG; a += 30) {
    float r = radians(a);
    canvas->drawLine(CX + ORIGIN_R * sinf(r), cy - ORIGIN_R * cosf(r),
                     CX + RADAR_R * sinf(r),  cy - RADAR_R * cosf(r), lineCol);
  }

  // thick outer ring
  canvas->fillArc(CX, cy, RADAR_R + 2, RADAR_R - 2,
                  270 - FOV_DEG, 270 + FOV_DEG, c565(OUTER_RGB));

  // distance labels under the baseline, left side
  canvas->setFont(&fonts::Font0);
  canvas->setTextSize(1);
  canvas->setTextDatum(middle_center);
  canvas->setTextColor(c565(LABEL_RGB));
  char buf[8];
  for (int k = 1; k <= RING_COUNT; k++) {
    int rr = RADAR_R * k / RING_COUNT;
    snprintf(buf, sizeof(buf), "%gm", MAX_RANGE_MM * k / (float)RING_COUNT / 1000.0f);
    canvas->drawString(buf, CX - rr, cy + 8);
  }
}

void drawOrigin() {
  int cy = CY - oy;
  if (cy + ORIGIN_R < 0 || cy - ORIGIN_R > STRIP_H) return;
  canvas->fillArc(CX, cy, ORIGIN_R, 0, 0, 180, mix565(BG_RGB, LINE_RGB, 0.45f));  // lower half glow
  canvas->drawCircle(CX, cy, ORIGIN_R, c565(LINE_RGB));
  canvas->fillCircle(CX, cy, 4, c565(DOT_RGB));
}

void drawTargets() {
  canvas->setFont(&fonts::Font0);
  canvas->setTextSize(1);
  canvas->setTextDatum(middle_left);
  char buf[12];

  for (int i = 0; i < 3; i++) {
    const Blip &b = blips[i];
    if (!b.show || !b.onScreen) continue;
    int y = b.sy - oy;
    if (y < -24 || y > STRIP_H + 24) continue;

    const RGB &c = TARGET_RGB[i];

    // expanding ring right after the pulse hits
    if (b.age < 600) {
      float f = 1.0f - b.age / 600.0f;
      canvas->drawCircle(b.sx, y, 7 + b.age / 50, mix565(BG_RGB, c, f));
    }
    canvas->fillCircle(b.sx, y, 7, mix565(BG_RGB, c, b.glow * 0.35f));   // halo
    canvas->fillCircle(b.sx, y, 4, mix565(BG_RGB, c, b.glow));            // dot
    canvas->fillCircle(b.sx, y, 1, mix565(BG_RGB, {255, 255, 255}, b.glow * 0.9f));

    snprintf(buf, sizeof(buf), "%.2fm", b.dist / 1000.0f);
    canvas->setTextColor(mix565(BG_RGB, c, max(b.glow, 0.65f)));
    canvas->drawString(buf, b.sx + 10, y);
  }
}

// text with a 1px dark outline on all sides so it stands out from the background
void drawOutlinedText(const char *s, int x, int y, uint16_t col) {
  canvas->setTextColor(c565(OUTLINE_RGB));
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      if (dx || dy) canvas->drawString(s, x + dx, y + dy);
    }
  }
  canvas->setTextColor(col);
  canvas->drawString(s, x, y);
}

void drawHud(uint32_t now) {
  char buf[24];

  // top readouts (fit entirely in the first strip)
  if (oy == 0) {
    canvas->setTextDatum(top_left);

    for (int i = 0; i < 3; i++) {
      const Blip &b = blips[i];
      int x = 6 + i * 106;
      const RGB &c = HUD_RGB[i];
      uint16_t col = b.show ? c565(c) : mix565(BG_RGB, c, 0.55f);

      // color key dot matching the target's blip
      canvas->fillCircle(x + 4, 9, 5, c565(OUTLINE_RGB));
      canvas->fillCircle(x + 4, 9, 4, col);

      canvas->setFont(&fonts::Font2);
      if (b.show) snprintf(buf, sizeof(buf), "T%d %.2fm", i + 1, b.dist / 1000.0f);
      else        snprintf(buf, sizeof(buf), "T%d --", i + 1);
      drawOutlinedText(buf, x + 12, 2, col);

      if (b.show) {
        canvas->setFont(&fonts::Font0);
        snprintf(buf, sizeof(buf), "ang %+d  %+dcm/s", b.angle, b.speed);
        drawOutlinedText(buf, x, 21, col);
      }
    }
  }

  // status, bottom-right
  if (oy + STRIP_H > SCREEN_H - 14) {
    bool alive = (now - viewFrameMs) < 1000;
    canvas->setFont(&fonts::Font0);
    canvas->setTextDatum(bottom_right);
    canvas->setTextColor(alive ? c565(LABEL_RGB) : c565({255, 80, 70}));
    canvas->drawString(alive ? (SIMULATE ? "SIMULATED" : "LD2450") : "NO SENSOR",
                       SCREEN_W - 4, SCREEN_H - 2 - oy);
  }
}

void renderFrame(uint32_t now) {
  tft.startWrite();
  int s = 0;
  for (oy = 0; oy < SCREEN_H; oy += STRIP_H) {
    canvas = strips[s];          // draw into the buffer that is NOT being sent

    drawBackground();
    drawPulse();
    drawGrid();
    drawOrigin();
    drawTargets();
    drawHud(now);

    tft.waitDMA();               // wait for the previous strip to finish sending
    canvas->pushSprite(0, oy);   // starts a DMA transfer and returns immediately
    s ^= 1;
  }
  tft.waitDMA();
  tft.endWrite();
}

void displayTask(void *) {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);

  for (int i = 0; i < 2; i++) {
    strips[i]->setColorDepth(16);
    if (!strips[i]->createSprite(SCREEN_W, STRIP_H)) {
      tft.setTextColor(TFT_RED);
      tft.drawString("Sprite alloc failed", 10, 10);
      for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  bgSwap = swap16(c565(BG_RGB));
  pulseStart = millis();

  for (;;) {
    takeSnapshot();              // grab latest radar data from core 1
    uint32_t now = millis();     // read after snapshot so frame age never underflows

    updatePulse(now);
    updateTargets(now);
    renderFrame(now);

    vTaskDelay(1);               // let core 0's idle task run (keeps the watchdog happy)
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(displayTask, "display", 8192, nullptr, 1, nullptr, DISPLAY_CORE);
  xTaskCreatePinnedToCore(radarTask,   "radar",   4096, nullptr, 2, nullptr, RADAR_CORE);
}

void loop() {
  vTaskDelete(nullptr);          // Arduino loop task isn't needed
}