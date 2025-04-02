#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <vector>

// ESP32 SPI Pins
#define TFT_CS 5   // TFT Chip Select
#define TFT_DC 2   // TFT Data/Command
#define TFT_RST 4  // TFT Reset
#define TFT_BL 15  // TFT Backlight (if applicable)

#define SD_CS 17  // SD Card Chip Select

#define E_BUTTON 16

bool e_b_state = false;

std::vector<String> fileList;
int fileCount = 0;
int currentPos = 0;
File root;

// Initialize ST7735 Display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define ENC_A 22
#define ENC_B 21

unsigned long _lastIncReadTime = micros();
unsigned long _lastDecReadTime = micros();
int _pauseLength = 25000;
int _fastIncrement = 10;

volatile int counter = 0;

void setup() {

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);

  Serial.begin(115200);

  // Initialize SPI
  SPI.begin(18, 19, 23);  // SCK, MISO, MOSI for VSPI

  // Initialize TFT Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);

  // Turn on Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(E_BUTTON, INPUT_PULLDOWN);

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(0, 3);
    tft.print("SD ERROR!");
    return;
  }
  tft.println("SD Card initialized.");
  root = SD.open("/");
  listFiles(root);
}

void loop() {
  int v = digitalRead(E_BUTTON);
  if (v && !e_b_state) {
    Serial.println("Button Pressed");
    root = SD.open("/");
    listFilesTFT(root);
    e_b_state = true;
  } else if (!v && e_b_state) {
    Serial.println("Button Released");
    e_b_state = false;
  }

  static int lastCounter = 0;

  // If count has changed print the new value to serial
  if (counter != lastCounter) {
    Serial.println(counter);
    lastCounter = counter;
  }
}

void listFilesTFT(File folder) {
  fileList.clear();  // Clear previous entries

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);
  while (true) {
    File entry = folder.openNextFile();
    if (!entry) break;

    String fileName = entry.name();

    // Skip "System Volume Information"
    if (fileName.equals("System Volume Information")) {
      entry.close();
      continue;
    }

    fileList.push_back(fileName);  // Store valid files
    entry.close();
  }

  // Print valid filenames
  int index = 0;
  for (const auto &name : fileList) {
    if (index == currentPos)
      tft.setTextColor(ST77XX_YELLOW);
    Serial.println(name);
    tft.println(name);
    tft.setTextColor(ST77XX_WHITE);
    index++;
  }
}

void listFiles(File folder) {
  fileList.clear();  // Clear previous entries

  while (true) {
    File entry = folder.openNextFile();
    if (!entry) break;

    String fileName = entry.name();

    // Skip "System Volume Information"
    if (fileName.equals("System Volume Information")) {
      entry.close();
      continue;
    }

    fileList.push_back(fileName);  // Store valid files
    entry.close();
  }

  fileCount = fileList.size();

  // Print valid filenames
  for (const auto &name : fileList) {
    Serial.println(name);
  }
}

void read_encoder() {
  // Encoder interrupt routine for both pins. Updates counter
  // if they are valid and have rotated a full indent

  static uint8_t old_AB = 3;                                                                  // Lookup table index
  static int8_t encval = 0;                                                                   // Encoder value
  static const int8_t enc_states[] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };  // Lookup table

  old_AB <<= 2;  // Remember previous state

  if (digitalRead(ENC_A)) old_AB |= 0x02;  // Add current state of pin A
  if (digitalRead(ENC_B)) old_AB |= 0x01;  // Add current state of pin B

  encval += enc_states[(old_AB & 0x0f)];

  // Update counter if encoder has rotated a full indent, that is at least 4 steps
  if (encval > 3) {  // Four steps forward
    int changevalue = 1;
    if ((micros() - _lastIncReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue;
    }
    _lastIncReadTime = micros();
    counter = counter + changevalue;  // Update counter
    encval = 0;
  } else if (encval < -3) {  // Four steps backward
    int changevalue = -1;
    if ((micros() - _lastDecReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue;
    }
    _lastDecReadTime = micros();
    counter = counter + changevalue;  // Update counter
    encval = 0;
  }
}