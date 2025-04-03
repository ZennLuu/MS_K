#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <string.h>

// ESP32 SPI Pins
#define TFT_CS 5   // TFT Chip Select
#define TFT_DC 2   // TFT Data/Command
#define TFT_RST 4  // TFT Reset
#define TFT_BL 15  // TFT Backlight (if applicable)

#define SD_CS 17  // SD Card Chip Select

#define E_BUTTON 16

#define ENC_A 22
#define ENC_B 21

volatile int counter = 0;

bool buttonState = false;
bool lastButtonState = false;

std::vector<String> fileList;
std::vector<String> prevDirs;
String currentRoot = "";
String baseRoot = "/";

int fileCount = 0;
int dirCount = 0;
int currentPos = 0;
File root;

bool file_is_opened = false;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
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

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(0, 3);
    tft.print("SD ERROR!");
    return;
  }
  Serial.println("SD Card initialized.");
  root = SD.open(baseRoot);
  listFiles(root);


  printFilesTFT();
}

void loop() {
  delay(10);
  buttonState = digitalRead(E_BUTTON);
  if (buttonState == HIGH && lastButtonState == LOW) {
    if (!file_is_opened) {
      if (currentPos < dirCount) {
        Serial.print("Trying to open folder: ");
        Serial.println(fileList[currentPos]);
        currentRoot = currentRoot + "/" + fileList[currentPos];
        listFiles(SD.open(currentRoot));
        counter = 0;
        if (currentPos == 0)
          printFilesTFT();
      } else {
        file_is_opened = true;
        Serial.print("Trying to open file: ");
        Serial.println(fileList[currentPos]);
        displayFileContent(currentRoot + "/" + fileList[currentPos]);
      }
    } else if (file_is_opened) {
      file_is_opened = false;
      printFilesTFT();
    }
  }
  lastButtonState = buttonState;

  // If count has changed print the new value to serial
  if (counter != currentPos) {
    Serial.println(counter);
    currentPos = counter;
    printFilesTFT();
  }
}

void printFilesTFT() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);

  for (int i = 0; i < dirCount + fileCount; i++) {
    if (i < dirCount) {
      if (i == currentPos) {
        tft.setTextColor(ST77XX_YELLOW);
      } else {
        tft.setTextColor(ST77XX_BLUE);
      }
      tft.println(fileList[i]);
    } else {
      if (i == currentPos) {
        tft.setTextColor(ST77XX_YELLOW);
      } else {
        tft.setTextColor(ST77XX_WHITE);
      }
      tft.println(fileList[i]);
    }
  }

  // Print valid filenames
  // int index = 0;
  // for (const auto &name : fileList) {
  //   char t_name[22];                  // Буфер для скороченого імені
  //   if (strlen(name.c_str()) > 21) {  // 20, бо останній символ буде '\0'
  //     strncpy(t_name, name.c_str(), 21);
  //     t_name[21] = '\0';  // Гарантуємо коректне завершення рядка
  //   } else {
  //     strcpy(t_name, name.c_str());
  //   }

  //   if (index == currentPos)
  //     tft.setTextColor(ST77XX_YELLOW);
  //   //Serial.println(name);
  //   tft.println(t_name);
  //   tft.setTextColor(ST77XX_WHITE);
  //   index++;
  // }
  tft.setTextColor(ST77XX_WHITE);
}

void listFiles(File folder) {
  fileList.clear();  // Clear previous entries
  fileCount = 0;
  dirCount = 0;

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

    if (entry.isDirectory()) {
      fileList.insert(fileList.begin(), fileName);
      dirCount++;
    } else {
      fileList.push_back(fileName);  // Store valid files
      fileCount++;
    }
    entry.close();
  }

  Serial.print("File count: ");
  Serial.println(fileCount);
  Serial.print("Dir count: ");
  Serial.println(dirCount);
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry = dir.openNextFile();
    if (!entry) {
      // No more files
      // Serial.println("**nomorefiles**");
      break;
    }

    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('-');
    }

    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // Files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
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
  if (!file_is_opened && !buttonState) {
    if (encval > 3) {
      if (counter + 1 < fileCount + dirCount)
        counter = counter + 1;
      encval = 0;
    } else if (encval < -3) {
      if (counter - 1 >= 0)
        counter = counter - 1;  // Update counter
      encval = 0;
    }
  }
}

void displayFileContent(const String &fileName) {
  File file = SD.open(fileName);
  if (!file) {
    Serial.println("Read Error!");
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(5, 5);
    tft.setTextColor(ST77XX_RED);
    tft.println("Read Error!");
    return;
  }

  tft.fillScreen(ST77XX_BLACK);  // Очищаємо екран перед виведенням
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 5);

  char buffer[21];  // Буфер для читання файлу частинами
  int y = 0;        // Початкова координата Y для тексту

  while (file.available()) {
    int bytesRead = file.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[bytesRead] = '\0';  // Завершуємо рядок '\0'

    tft.setCursor(0, y);
    tft.println(buffer);
    y += 10;  // Зсуваємо вниз

    // Якщо досягли нижнього краю екрану – очікуємо підтвердження
    if (y >= 160) {
      break;
    }
  }
  tft.setTextColor(ST77XX_WHITE);
  file.close();
}
