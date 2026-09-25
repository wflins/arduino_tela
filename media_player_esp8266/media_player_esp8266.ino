#include <Arduino.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <DFRobotDFPlayerMini.h>

#include "frames.h"

// ============================================================
// Hardware
// ============================================================
// ST7789 1.54" 240x240
#define TFT_CS   D8   // GPIO15
#define TFT_DC   D2   // GPIO4
#define TFT_RST  D0   // GPIO16

// SPI por hardware no ESP8266:
// SCL/SCK = D5 / GPIO14
// SDA/MOSI = D7 / GPIO13

// DFPlayer Mini
// SoftwareSerial(rxPin, txPin)
#define DFPLAYER_RX D6  // recebe do TX do DFPlayer
#define DFPLAYER_TX D1  // envia ao RX do DFPlayer (usar 1 kOhm em série)

// ============================================================
// Configurações
// ============================================================
const uint8_t DFPLAYER_VOLUME = 20;       // 0..30
const uint16_t TRACK_COUNT = 3;           // /mp3/0001.mp3 ... /mp3/0003.mp3
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
// Utilidades de tela
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
  tft.setCursor(24, 8);
  tft.print(F("Mini Media Player"));

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
  tft.setCursor(10, 60);
  tft.println(line1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 105);
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
// Áudio
// ============================================================
void playTrack(uint16_t track) {
  if (!dfPlayerReady) {
    return;
  }

  if (track < 1) {
    track = 1;
  }
  if (track > TRACK_COUNT) {
    track = 1;
  }

  currentTrack = track;
  drawTrackInfo();

  // Requer arquivos no formato /mp3/0001.mp3, /mp3/0002.mp3, ...
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
  delay(200);
  Serial.println();
  Serial.println(F("Iniciando Mini Media Player..."));

  // O construtor da Adafruit_ST7789 usa o SPI por hardware.
  // No NodeMCU ESP8266: SCK=D5 e MOSI=D7.
  tft.init(240, 240);
  tft.setRotation(0);
  drawHeader();

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(32, 100);
  tft.print(F("Iniciando audio..."));

  dfSerial.begin(9600);
  delay(1000);

  if (!dfPlayer.begin(dfSerial, true, true)) {
    Serial.println(F("Falha ao iniciar DFPlayer."));
    Serial.println(F("Verifique alimentacao, RX/TX e microSD."));

    drawError(
      F("DFPlayer nao iniciou"),
      F("Verifique SD, fios RX/TX e alimentacao 5V.")
    );

    return;
  }

  dfPlayerReady = true;
  dfPlayer.volume(DFPLAYER_VOLUME);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  delay(500);

  drawHeader();
  drawTrackInfo();
  playTrack(1);

  Serial.println(F("Player pronto."));
}

void loop() {
  updateAnimation();
  handleDfPlayerEvents();
  yield();
}
