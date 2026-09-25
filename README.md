# Arduino Tela — player de animações + MP3

Projeto inicial para montar um pequeno player multimídia usando o hardware já identificado:

- NodeMCU ESP8266 (ESP-12E)
- Display TFT 1,54" SPI 240x240 com controlador ST7789
- DFPlayer Mini / compatível com TD5580A
- Cartão microSD no DFPlayer
- Alto-falante 4–8 Ω

## Importante sobre o cartão microSD

O cartão inserido no **DFPlayer Mini é controlado pelo próprio DFPlayer**. O ESP8266 não consegue abrir JPG/BMP diretamente desse mesmo cartão.

Nesta primeira versão:

- os **MP3 ficam no microSD do DFPlayer**;
- as **imagens/frames ficam na memória flash do ESP8266**, compiladas no arquivo `frames.h`.

Se futuramente quisermos carregar imagens diretamente de um cartão, será necessário adicionar um segundo módulo microSD SPI (ou mudar a arquitetura para ESP32 + SD compartilhado de forma controlada).

---

## Ligações

### ST7789 1.54" → NodeMCU ESP8266

| ST7789 | NodeMCU | GPIO | Observação |
|---|---|---:|---|
| VCC | 3V3 | — | Alimentação 3,3 V |
| GND | GND | — | Terra comum |
| SCL | D5 | GPIO14 | SPI CLK |
| SDA | D7 | GPIO13 | SPI MOSI |
| CS | D8 | GPIO15 | Chip Select |
| DC | D2 | GPIO4 | Data/Command |
| RST | D0 | GPIO16 | Reset do display |
| BL | 3V3 | — | Backlight sempre ligado |

> D8/GPIO15 é pino de bootstrap do ESP8266 e deve permanecer em nível baixo durante o boot. A ligação como CS do display normalmente funciona, mas não adicione pull-up nesse pino.

### DFPlayer Mini → NodeMCU ESP8266

| DFPlayer | NodeMCU | GPIO | Observação |
|---|---|---:|---|
| VCC | VIN / 5V | — | Recomendado alimentar em 5 V |
| GND | GND | — | Terra comum |
| TX | D6 | GPIO12 | RX do ESP8266 |
| RX | D1 | GPIO5 | TX do ESP8266; usar resistor de 1 kΩ em série |
| SPK1 | Alto-falante | — | Saída amplificada |
| SPK2 | Alto-falante | — | Saída amplificada |

### Diagrama simplificado

```text
                 +----------------------+
                 |   NodeMCU ESP8266    |
                 |                      |
 D5 / GPIO14 ----+---- SCL   ST7789     |
 D7 / GPIO13 ----+---- SDA              |
 D8 / GPIO15 ----+---- CS               |
 D2 / GPIO4  ----+---- DC               |
 D0 / GPIO16 ----+---- RST              |
 3V3 ----------- +---- VCC + BL          |
 GND ------------+---- GND              |
                 |                      |
 D6 / GPIO12 <---+---- TX   DFPlayer    |
 D1 / GPIO5  ----+---- RX (via 1 kΩ)    |
 VIN / 5V -------+---- VCC              |
 GND ------------+---- GND              |
                 +----------------------+

 DFPlayer SPK1 ------------------+ Alto-falante
 DFPlayer SPK2 ------------------+
```

---

## Preparação do cartão do DFPlayer

Formate o cartão em FAT32 e crie:

```text
/mp3/
  0001.mp3
  0002.mp3
  0003.mp3
  ...
```

Os nomes com quatro dígitos são a forma mais previsível de usar `playMp3Folder()`.

## Bibliotecas Arduino IDE

Instale pelo Library Manager:

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library**
- **DFRobotDFPlayerMini**

`SoftwareSerial` já faz parte do core ESP8266.

## Programa

Abra:

`media_player_esp8266/media_player_esp8266.ino`

O exemplo:

1. inicializa o display ST7789 em 240x240;
2. inicializa o DFPlayer;
3. inicia a faixa `0001.mp3`;
4. exibe uma pequena animação com frames armazenados em `frames.h`;
5. quando uma música termina, avança automaticamente para a próxima;
6. após a última faixa, volta para a primeira.

## Ajustes rápidos

No início do `.ino` você pode alterar:

```cpp
const uint8_t DFPLAYER_VOLUME = 20; // 0 a 30
const uint16_t TRACK_COUNT = 3;     // quantidade de MP3 em /mp3
const uint16_t FRAME_INTERVAL_MS = 120;
```

Ajuste `TRACK_COUNT` para a quantidade real de músicas no cartão.

## Próximo passo

Para usar suas próprias animações, substitua os bitmaps do arquivo `frames.h`. Os frames de exemplo são monocromáticos e pequenos para manter o primeiro teste simples e confiável.