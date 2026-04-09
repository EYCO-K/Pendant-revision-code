#include <Arduino.h>
#include <algorithm>
#include <vector>

#define MJPEG_BUFFER_SIZE (240 * 240 * 2 / 4)

#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <Arduino_GFX_Library.h>

#define TFT_BRIGHTNESS 200
#define NEXT_FILE_PIN 23
#define BUTTON_DEBOUNCE_MS 40
#define MISO 2
#define SCK 14  // SCL
#define MOSI 15 // SDA
#define SD_CS 13
#define TFT_CS 5
#define TFT_BL 22
#define TFT_DC 27
#define TFT_RST 33
#define TFT_WIDTH 240
#define TFT_HEIGHT 240
#define TFT_COL_OFFSET 0
#define TFT_ROW_OFFSET 0
#define BUTTON_STARTUP_IGNORE_MS 1500

void setup();
void loop();

int8_t dc = TFT_DC;
int8_t cs = TFT_CS;
int8_t sck = SCK;
int8_t mosi = MOSI;
int8_t miso = MISO;
SPIClass *spi = &SPI;
bool isShared = true;

Arduino_DataBus *bus = new Arduino_HWSPI(dc, cs, sck, mosi, miso, spi, isShared);
// Arduino_GC9A01  *gfx = new Arduino_GC9A01(bus, TFT_RST, 1, true);
Arduino_ST7789 *gfx = new Arduino_ST7789(
    bus, TFT_RST, 0, true,
    TFT_WIDTH, TFT_HEIGHT,
    TFT_COL_OFFSET, TFT_ROW_OFFSET);

#include "MjpegClass.h"
static MjpegClass mjpeg;

static std::vector<String> playlistFiles;
static uint8_t *mjpeg_buf = nullptr;
static size_t currentFileIndex = 0;
static bool sdReady = false;
static bool nextFileRequested = false;
static int32_t lastDrawnFileIndex = -1;
static uint32_t bootMs = 0;

static bool hasPlayableExtension(const String &path)
{
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mjpeg");
}

static String normalizePath(const String &path)
{
  String normalized = path;
  normalized.replace("\\", "/");
  if (!normalized.startsWith("/"))
  {
    normalized = "/" + normalized;
  }
  return normalized;
}

static void showStatus(const String &message)
{
  Serial.println(message);
  gfx->fillScreen(BLACK);
  gfx->setCursor(0, 0);
  gfx->println(message);
}

static void collectPlayableFiles(File dir)
{
  if (!dir || !dir.isDirectory())
  {
    return;
  }

  File entry = dir.openNextFile();
  while (entry)
  {
    if (entry.isDirectory())
    {
      collectPlayableFiles(entry);
    }
    else
    {
      String path = normalizePath(String(entry.name()));
      if (!path.endsWith("/") && hasPlayableExtension(path))
      {
        playlistFiles.push_back(path);
      }
    }

    entry.close();
    entry = dir.openNextFile();
  }
}

static bool refreshPlaylist()
{
  playlistFiles.clear();

  File root = SD.open("/");
  if (!root || !root.isDirectory())
  {
    return false;
  }

  collectPlayableFiles(root);
  root.close();

  std::sort(playlistFiles.begin(), playlistFiles.end(), [](const String &a, const String &b) {
    return strcmp(a.c_str(), b.c_str()) < 0;
  });

  return !playlistFiles.empty();
}

static void requestNextFile()
{
  if (!playlistFiles.empty())
  {
    currentFileIndex = (currentFileIndex + 1) % playlistFiles.size();
    nextFileRequested = false;
  }
}

static bool consumeNextFileRequest()
{
  bool requested = nextFileRequested;
  nextFileRequested = false;
  return requested;
}

static void pollNextFileButton()
{
  static uint8_t lastReading = HIGH;
  static uint8_t stableState = HIGH;
  static uint32_t lastChangeMs = 0;

  if (millis() - bootMs < BUTTON_STARTUP_IGNORE_MS)
  {
    lastReading = digitalRead(NEXT_FILE_PIN);
    stableState = lastReading;
    lastChangeMs = millis();
    nextFileRequested = false;
    return;
  }

  uint8_t reading = digitalRead(NEXT_FILE_PIN);
  if (reading != lastReading)
  {
    lastChangeMs = millis();
    lastReading = reading;
  }

  if ((millis() - lastChangeMs) >= BUTTON_DEBOUNCE_MS && reading != stableState)
  {
    uint8_t previousStableState = stableState;
    stableState = reading;
    if (previousStableState == LOW && stableState == HIGH)
    {
      nextFileRequested = true;
    }
  }
}

static void playCurrentFile()
{
  const String path = playlistFiles[currentFileIndex];
  File vFile = SD.open(path, FILE_READ);
  if (!vFile || vFile.isDirectory())
  {
    showStatus(String("Open failed: ") + path);
    requestNextFile();
    delay(150);
    return;
  }

  if (lastDrawnFileIndex != static_cast<int32_t>(currentFileIndex))
  {
    gfx->fillScreen(BLACK);
    lastDrawnFileIndex = static_cast<int32_t>(currentFileIndex);
  }

  if (!mjpeg.setup(vFile, mjpeg_buf, gfx, true))
  {
    vFile.close();
    showStatus(String("Player setup failed: ") + path);
    requestNextFile();
    delay(150);
    return;
  }

  Serial.print(F("Playing: "));
  Serial.println(path);

  bool decodeFailed = false;
  while (true)
  {
    pollNextFileButton();
    if (consumeNextFileRequest())
    {
      vFile.close();
      requestNextFile();
      return;
    }

    if (!mjpeg.readMjpegBuf())
    {
      break;
    }

    if (!mjpeg.drawJpg())
    {
      decodeFailed = true;
      break;
    }
  }

  vFile.close();

  if (decodeFailed)
  {
    showStatus(String("Decode failed: ") + path);
    requestNextFile();
    delay(150);
    return;
  }

  if (consumeNextFileRequest())
  {
    requestNextFile();
  }
}

void setup()
{
  bootMs = millis();
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  pinMode(NEXT_FILE_PIN, INPUT_PULLUP);

  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  ledcAttachPin(TFT_BL, 1);     // assign TFT_BL pin to channel 1
  ledcSetup(1, 12000, 8);       // 12 kHz PWM, 8-bit resolution
  ledcWrite(1, TFT_BRIGHTNESS); // brightness 0 - 255
#endif

  SPI.begin(SCK, MISO, MOSI, SD_CS);
  if (!SD.begin(SD_CS))
  {
    showStatus(F("ERROR: SD card mount failed!"));
    return;
  }

  mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
  if (!mjpeg_buf)
  {
    showStatus(F("ERROR: mjpeg_buf malloc failed!"));
    return;
  }

  if (!refreshPlaylist())
  {
    showStatus(F("ERROR: No MJPEG files found!"));
    return;
  }

  sdReady = true;
  Serial.printf("Found %u files\r\n", static_cast<unsigned>(playlistFiles.size()));
  Serial.print(F("First file: "));
  Serial.println(playlistFiles[currentFileIndex]);
}

void loop()
{
  pollNextFileButton();

  if (!sdReady || playlistFiles.empty() || !mjpeg_buf)
  {
    delay(20);
    return;
  }

  playCurrentFile();
}
