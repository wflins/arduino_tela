#include <Arduino.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <DFRobotDFPlayerMini.h>

// ============================================================
// V1 - teste de hardware
// NodeMCU ESP8266 + ST7789 1.54" 240x240 + DFPlayer Mini
// Tudo em um unico arquivo .ino
// ============================================================

// ============================================================
// Frames da animacao de teste
// ============================================================
static const uint8_t FRAME_WIDTH = 16;
static const uint8_t FRAME_HEIGHT = 16;
static const uint8_t FRAME_COUNT = 4;

const uint8_t frame0[] PROGMEM = {
  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x01,0x80,
  0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x03,0xC0,
  0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x1F,0xC0,
  0x3F,0x80, 0x3F,0x00, 0x1E,0x00, 0x00,0x00
};

const uint8_t frame1[] PROGMEM = {
  0x00,0x00, 0x00,0x00, 0x00,0xC0, 0x01,0xE0,
  0x03,0xE0, 0x03,0xC0, 0x03,0xC0, 0x03,0xC0,
  0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x1F,0xC0,
  0x3F,0x80, 0x3F,0x00, 0x1E,0x00, 0x00,0x00
};

const uint8_t frame2[] PROGMEM = {
  0x00,0x00, 0x00,0x00, 0x00,0x60, 0x00,0xF0,
  0x01,0xF0, 0x03,0xE0, 0x03,0xC0, 0x03,0xC0,
  0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x1F,0xC0,
  0x3F,0x80, 0x3F,0x00, 0x1E,0x00, 0x00,0x00
};

const uint8_t frame3[] PROGMEM = {
  0x00,0x00, 0x00,0x00, 0x00,0x30, 0x00,0x78,
  0x00,0xF8, 0x01,0xF0, 0x03,0xE0, 0x03,0xC0,
  0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x1F,0xC0,
  0x3F,0x80, 0x3F,0x00, 0x1E,0x00, 0x00,0x00
};

const uint8_t* const FRAMES[FRAME_COUNT] = {
  frame0, frame1, frame2, frame3
};

// ============================================================
// Hardware
// ============================================================

// ST7789
#define TFT_CS   D8   // GPIO15
#define TFT_DC   D2   // GPIO4
#define TFT_RST  D0   // GPIO16
// SPI por hardware:
// SCL/SCK = D5 / GPIO14
// SDA/MOSI = D7 / GPIO13

// DFPlayer Mini
// SoftwareSerial(rxPin, txPin)
#define DFPLAYER_RX D6  // recebe do TX do DFPlayer
#define DFPLAYER_TX D1  // envia ao RX do DFPlayer; resistor de 1 kOhm recomendado

// ============================================================
// Configuracoes
// ============================================================
const uint8_t DFPLAYER_VOLUME = 20;       // 0..30
const uint16_t TRACK_COUNT = 2;           // /mp3/0001.mp3 e /mp3/0002.mp3
const uint16_t FRAME_INTERVAL_MS = 120;   // ~8 FPS

const uint8_t FRAME_SCALE = 8;            // 16x16 -> 128x128 pixels
const int16_t FRAME_X = 56;
const int16_t FRAME_Y = 35;

// ============================================================
// Objetos
// ============================================================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
SoftwareSerial dfSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;

uint16_t currentTrack = 1;
uint8_t currentFrame = 0;
unsigned long lastFrameAt = 0;
bool dfPlayerReady = false;

// ============================================================
// Tela
// ============================================================
void drawBitmapScaled(
  int16_t x,
  int16_t y,
  const uint8_t *bitmap,
  uint8_t width,
  uint8_t height,
  uint8_t scale,
  uint16_t color,
  uint16_t background
) {
  tft.fillRect(x, y, width * scale, height * scale, background);

  for (uint8_t py = 0; py < height; py++) {
    for (uint8_t px = 0; px < width; px++) {
      const uint16_t bitIndex = py * width + px;
      const uint16_t byteIndex = bitIndex / 8;
      const uint8_t bitMask = 0x80 >> (bitIndex % 8);
      const uint8_t value = pgm_read_byte(bitmap + byteIndex);

      if (value & bitMask) {
        tft.fillRect(
          x + (px * scale),
          y + (py * scale),
          scale,
          scale,
          color
        );
      }
    }
  }
}

void drawHeader() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(30, 8);
  tft.print(F("Media Player V1"));

  tft.drawFastHLine(10, 28, 220, ST77XX_BLUE);
}

void drawTrackInfo() {
  tft.fillRect(0, 175, 240, 65, ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(42, 180);
  tft.print(F("Tocando MP3"));

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.setCursor(70, 207);
  tft.printf("%04u", currentTrack);
}

void drawError(const __FlashStringHelper *line1, const __FlashStringHelper *line2) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.println(line1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 100);
  tft.println(line2);
}

void updateAnimation() {
  const unsigned long now = millis();
  if (now - lastFrameAt < FRAME_INTERVAL_MS) {
    return;
  }

  lastFrameAt = now;

  drawBitmapScaled(
    FRAME_X,
    FRAME_Y,
    FRAMES[currentFrame],
    FRAME_WIDTH,
    FRAME_HEIGHT,
    FRAME_SCALE,
    ST77XX_YELLOW,
    ST77XX_BLACK
  );

  currentFrame++;
  if (currentFrame >= FRAME_COUNT) {
    currentFrame = 0;
  }
}

// ============================================================
// Audio
// ============================================================
void playTrack(uint16_t track) {
  if (!dfPlayerReady) {
    return;
  }

  if (track < 1 || track > TRACK_COUNT) {
    track = 1;
  }

  currentTrack = track;
  drawTrackInfo();

  Serial.print(F("Tocando faixa: "));
  Serial.println(currentTrack);

  // Cartao: /mp3/0001.mp3 e /mp3/0002.mp3
  dfPlayer.playMp3Folder(currentTrack);
}

void playNextTrack() {
  uint16_t nextTrack = currentTrack + 1;
  if (nextTrack > TRACK_COUNT) {
    nextTrack = 1;
  }
  playTrack(nextTrack);
}

void handleDfPlayerEvents() {
  if (!dfPlayerReady || !dfPlayer.available()) {
    return;
  }

  const uint8_t type = dfPlayer.readType();
  const int value = dfPlayer.read();

  Serial.print(F("DFPlayer evento: "));
  Serial.print(type);
  Serial.print(F(" valor: "));
  Serial.println(value);

  if (type == DFPlayerPlayFinished) {
    delay(200);
    playNextTrack();
  }
}

// ============================================================
// Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("=== Media Player V1 ==="));
  Serial.println(F("Inicializando ST7789..."));

  tft.init(240, 240);
  tft.setRotation(0);
  drawHeader();

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(25, 100);
  tft.print(F("Iniciando audio..."));

  Serial.println(F("Inicializando DFPlayer..."));
  dfSerial.begin(9600);
  delay(1500);

  // Alguns DFPlayer/clones respondem aos comandos normalmente, mas falham
  // quando a biblioteca exige ACK e envia RESET durante o begin().
  // Neste hardware, a inicializacao confiavel e sem ACK e sem RESET.
  if (!dfPlayer.begin(dfSerial, false, false)) {
    Serial.println(F("ERRO: DFPlayer nao iniciou."));
    Serial.println(F("Confira 5V, GND, RX/TX, resistor e microSD."));

    drawError(
      F("DFPlayer nao iniciou"),
      F("Confira SD, RX/TX e alimentacao 5V.")
    );
    return;
  }

  dfPlayerReady = true;
  Serial.println(F("DFPlayer iniciado!"));

  dfPlayer.volume(DFPLAYER_VOLUME);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(500);

  drawHeader();
  playTrack(1);

  Serial.println(F("Player pronto. Deve alternar 0001.mp3 e 0002.mp3."));
}

void loop() {
  updateAnimation();
  handleDfPlayerEvents();
  yield();
}
