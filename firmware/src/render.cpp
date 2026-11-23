#include "render.h"
#include "font_diag.h"
#include "font_4x5.h"
#include "geometry.h"
#include "artcodec.h"

namespace {
// ---- Константы раскладки и анимации ----
constexpr float    ROW_HEIGHT         = 0.866f;   // высота ряда сот в диаметрах шарика (корень из 3, делённый на 2)
constexpr float    ANGLE_TO_RAD         = 0.0174533f;
constexpr float    NOISE_SCALE        = 384.0f;   // единиц шума на один шарик (при scale = 1.0)
constexpr uint8_t  GLYPH_ROWS         = 5;        // высота цифры
constexpr uint8_t  TEXT_TOP_ROW       = 1;        // цифры занимают ряды 1..5 из 0..6
constexpr uint8_t  DIGIT_COL[4]       = {1, 5, 11, 15};  // первый столбец каждой цифры ЧЧ:ММ
constexpr uint8_t  COLON_COL          = 9;        // столбец двоеточия
constexpr uint8_t  COLON_COL_LOW_4X5  = 10;       // у шрифта 4x5 нижняя точка сдвинута на столбец
constexpr uint8_t  DASH_ROW           = 3;        // ряд прочерков, пока нет времени
constexpr uint8_t  BLINK_FADE_MIN     = 25;       // насколько тускнеет двоеточие (из 255)
constexpr uint16_t BLINK_PERIOD_MS    = 1000;

const CRGB STOPS[4][4] = {
  {CRGB(40,0,60),  CRGB(200,30,80),  CRGB(255,140,30), CRGB(255,220,120)},
  {CRGB(0,20,60),  CRGB(0,90,160),   CRGB(0,200,200),  CRGB(180,255,240)},
  {CRGB(5,40,20),  CRGB(20,120,50),  CRGB(140,200,60), CRGB(230,240,150)},
  {CRGB(90,0,160), CRGB(255,0,140),  CRGB(0,200,255),  CRGB(255,255,255)}
};

// 0-3 свои палитры, 4-8 встроенные палитры FastLED
CRGBPalette16 makePalette(uint8_t i) {
  uint32_t colors[4];
  if(i>=9 && i<9+GRADIENT_SLOTS && gradientColors(S.gradients[i-9],colors))
    return CRGBPalette16(CRGB(colors[0]),CRGB(colors[1]),CRGB(colors[2]),CRGB(colors[3]));
  if(i>=9)i=0; // safe fallback for a deleted/unassigned preset
  switch (i) {
    case 0: case 1: case 2: case 3: return CRGBPalette16(STOPS[i][0], STOPS[i][1], STOPS[i][2], STOPS[i][3]);
    case 4: return CRGBPalette16(RainbowColors_p);
    case 5: return CRGBPalette16(PartyColors_p);
    case 6: return CRGBPalette16(LavaColors_p);
    case 7: return CRGBPalette16(HeatColors_p);
    default: return CRGBPalette16(CloudColors_p);
  }
}

// Палитра пересобирается только при смене выбора
const CRGBPalette16& currentPalette() {
  static uint8_t built = 255;
  static CRGBPalette16 pal;
  static char lastGradient[GRADIENT_BYTES]={};
  const char *custom=S.pal>=9 && S.pal<9+GRADIENT_SLOTS ? S.gradients[S.pal-9] : "";
  if (built != S.pal || strcmp(lastGradient,custom)) {
    pal = makePalette(S.pal); built = S.pal;
    memcpy(lastGradient,custom,strlen(custom)+1);
  }
  return pal;
}

// ---- Маска цифр: бит g в rowMask[ряд] = пиксель в диагональном столбце g ----
uint32_t rowMask[geo::ROWS];
uint32_t lastKey = 0xFFFFFFFF;

// Диагональная сетка со сдвигом рядов: X = 2c + 3 - r  =>  c = (X + r - 3) / 2
inline uint8_t diagCol(uint8_t X, uint8_t r) { return (X + r - 3) >> 1; }

inline uint8_t glyphWidth(uint8_t style) { return style == FS_4X5 ? 4 : 3; }

// Рисует одну цифру в маску, начиная со столбца col0
void stampGlyph(uint32_t* mask, uint8_t style, uint8_t digit, uint8_t col0) {
  for (uint8_t c = 0; c < glyphWidth(style); c++) {
    for (uint8_t y = 0; y < GLYPH_ROWS; y++) {
      const uint8_t bits = (style == FS_4X5) ? FONT_4X5[digit][c] : FONT_DIAG[digit][c];
      if ((bits >> y) & 1) mask[TEXT_TOP_ROW + y] |= 1UL << (col0 + c);
    }
  }
}

void buildMask(uint8_t h, uint8_t m, uint8_t style, bool lead, bool timeOk) {
  for (auto& r : rowMask) r = 0;
  if (style == FS_NONE) return;
  if (!timeOk) {                                        // прочерки "-- --"
    for (uint8_t i = 0; i < 4; i++)
      for (uint8_t c = 0; c < 3; c++) rowMask[DASH_ROW] |= 1UL << (DIGIT_COL[i] + c);
    return;
  }
  const uint8_t digit[4] = {uint8_t(h / 10), uint8_t(h % 10), uint8_t(m / 10), uint8_t(m % 10)};
  for (uint8_t i = (h < 10 && !lead) ? 1 : 0; i < 4; i++) stampGlyph(rowMask, style, digit[i], DIGIT_COL[i]);
}

// Всё, от чего зависит маска, в одном числе: пересобираем только когда оно изменилось
inline uint32_t maskKey(uint8_t h, uint8_t m, uint8_t style, bool lead, bool timeOk) {
  return (uint32_t(h) << 16) | (uint32_t(m) << 8) | (uint32_t(style) << 4) | (uint32_t(lead) << 1) | uint32_t(timeOk);
}

bool isColon(uint8_t style, uint8_t r, uint8_t g) {
  if (style == FS_4X5) return (r == 2 && g == COLON_COL) || (r == 4 && g == COLON_COL_LOW_4X5);
  return g == COLON_COL && (r == 2 || r == 4);
}

// Яркость двоеточия 25..255 с периодом 1 с (255, если мигание выключено)
uint8_t colonFade(uint32_t ms) {
  if (!S.blink) return 255;
  const uint8_t v = cos8(uint8_t((ms % BLINK_PERIOD_MS) * 255 / BLINK_PERIOD_MS));
  return v < BLINK_FADE_MIN ? BLINK_FADE_MIN : v;
}

// Цвет фона в точке (x, y), координаты в диаметрах шарика
CRGB background(const CRGBPalette16& pal, float ca, float sa, float x, float y,
                 float phase, uint16_t noiseZ, float projectionMin, float projectionInvSpan, float gradientSpan) {
  if (S.bmode == BG_SOLID) {
    CRGB c(S.bcolor);
    c.nscale8(S.bbright);
    return c;
  }
  const float scl = S.scale / 100.0f * 0.6f + 0.05f;
  uint8_t idx;
  if (S.bmode == BG_GRADIENT) {
    // Normalize across the real staggered physical surface, not per ball.
    // Scale 50 means one palette traversal across the complete clock.
    float position = ((x * ca + y * sa) - projectionMin) * projectionInvSpan;
    position = fminf(1.0f, fmaxf(0.0f, position));
    float u = fmodf((position - 0.5f) * gradientSpan + 0.5f + phase, 2.0f);
    if (u < 0) u += 2.0f;
    if (u > 1.0f) u = 2.0f - u; // seamless forward/back movement
    // FastLED's indices 241..255 blend the last palette colour back to the
    // first. A broad gradient must end at the LAST colour, not wrap to dark.
    idx = uint8_t(u * 240.0f);
  } else {
    uint8_t n = inoise8(uint16_t(x * scl * NOISE_SCALE), uint16_t(y * scl * NOISE_SCALE), noiseZ);
    n = qsub8(n, 16); n = qadd8(n, scale8(n, 39));
    idx = n;
  }
  CRGB c = ColorFromPalette(pal, idx, 255, LINEARBLEND);
  c.nscale8(S.bbright);
  return c;
}
}

void drawFrame(CRGB* leds, uint8_t h, uint8_t m, uint32_t ms, bool timeOk, bool nightActive) {
  const uint8_t style = S.bmode==BG_DRAW && !S.drawDigits ? FS_NONE : S.font;
  const uint32_t key = maskKey(h, m, style, S.lead, timeOk);
  if (key != lastKey) { buildMask(h, m, style, S.lead, timeOk); lastKey = key; }

  const bool useBg = !nightActive && S.bmode != BG_NONE;
  const CRGBPalette16& pal = currentPalette();
  const float rad = S.angle * ANGLE_TO_RAD, ca = cosf(rad), sa = sinf(rad);   // один раз за кадр, не на каждый светодиод
  // Reduce time once per frame, BEFORE adding small honeycomb coordinates.
  // Large float seconds lose their fractional bits and cause freezes/flashes.
  const float phase = S.bmode == BG_GRADIENT ? float(fmod(double(ms) * S.speed / 500000.0, 2.0)) : 0;
  float projectionMin = 0, projectionInvSpan = 1;
  const float gradientSpan = S.scale <= 50 ? 0.25f + S.scale * 0.015f : 1.0f + (S.scale - 50) * 0.02f;
  if (S.bmode == BG_GRADIENT) {
    float low = 1e9f, high = -1e9f;
    for (uint8_t i = 0; i < geo::LED_COUNT; ++i) {
      const float projected = geo::ledX[i] * 0.5f * ca + geo::ledRow[i] * ROW_HEIGHT * sa;
      low = fminf(low, projected); high = fmaxf(high, projected);
    }
    projectionMin = low;
    projectionInvSpan = high > low ? 1.0f / (high - low) : 1.0f;
  }
  // Defined integer wrap; unlike converting a huge float directly to uint16.
  const uint16_t noiseZ = uint16_t((uint64_t(ms) * S.speed) / 1000);
  const uint8_t fade = colonFade(ms);
  const CRGB fg = nightActive ? CRGB(S.ncolor) : CRGB(S.color);

  for (uint8_t i = 0; i < geo::LED_COUNT; i++) {
    const uint8_t r = geo::ledRow[i], X = geo::ledX[i], g = diagCol(X, r);
    const bool txt   = (style != FS_NONE) && (rowMask[r] & (1UL << g));
    const bool colon = (style != FS_NONE) && timeOk && isColon(style, r, g);
    CRGB c = CRGB::Black;
    if(useBg && S.bmode==BG_DRAW){uint8_t rgb[3];artColor(Drawing[i],ms,rgb);c=CRGB(rgb[0],rgb[1],rgb[2]);c.nscale8(S.bbright);}
    else if (useBg) c = background(pal, ca, sa, X * 0.5f, r * ROW_HEIGHT, phase, noiseZ,
                              projectionMin, projectionInvSpan, gradientSpan);
    if (txt) c = fg;
    else if (colon) c = blend(c, fg, fade);          // двоеточие плавно проявляется из фона
    leds[i] = c;
  }
}

void drawNumber(CRGB* leds, uint16_t n) {
  char buf[6]; snprintf(buf, sizeof(buf), "%u", n);
  const uint8_t len = strlen(buf), w = len * 4 - 1, start = (geo::W - w) / 2;
  uint32_t mask[geo::ROWS] = {0};
  for (uint8_t i = 0; i < len; i++) stampGlyph(mask, FS_DIAG, buf[i] - '0', start + i * 4);
  for (uint8_t i = 0; i < geo::LED_COUNT; i++) {
    const uint8_t g = diagCol(geo::ledX[i], geo::ledRow[i]);
    leds[i] = (mask[geo::ledRow[i]] & (1UL << g)) ? CRGB::White : CRGB::Black;
  }
}
