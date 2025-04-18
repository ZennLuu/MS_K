#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <string.h>

#define TFT_CS 5   // TFT Chip Select
#define TFT_DC 2   // TFT Data/Command
#define TFT_RST 4  // TFT Reset
#define TFT_BL 15  // TFT Backlight

#define SD_CS 17   // SD Card Chip Select

#define ENC_BUTTON 16
#define ENC_A 21
#define ENC_B 22

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

volatile int counter = 0;
unsigned long _lastIncReadTime = micros();
unsigned long _lastDecReadTime = micros();
int _pauseLength = 25000;
int _fastIncrement = 10;

bool buttonState = false;
bool lastButtonState = false;

std::vector<String> fileList;
std::vector<String> prevDirs;
String currentRoot = "/";
String baseRoot = "/";

std::vector<String> fileLines;
int currentPage = 0;
const int linesPerPage = 18;
String openedFileName = "";
std::vector<uint32_t> pageOffsets;

bool file_is_opened = false;

int fileCount = 0;
int dirCount = 0;
int currentPos = 0;

void setup() {
  Serial.begin(115200);

  // Initialize SPI
  SPI.begin(18, 19, 23);  // SCK, MISO, MOSI for VSPI

  pinMode(ENC_BUTTON, INPUT_PULLDOWN);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);

  // Initialize TFT Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);
  // Turn on Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  while (true) {
    if (SD.begin(SD_CS)) {
      break;
    }
    Serial.println("SD Card initialization failed!");
    delay(1000);
  }
  Serial.println("SD Card initialized.");

  File root = SD.open(baseRoot);
  listFiles(root);
  printFilesTFT();
}

void loop() {
  processButton();
  processEncoder();
}

void processButton() {
  delay(10);
  buttonState = digitalRead(ENC_BUTTON);
  if (buttonState == HIGH && lastButtonState == LOW) {
    if (!file_is_opened) {
      if (currentPos < dirCount) {
        String selected = fileList[currentPos];

        if (selected == "..") {
          // Назад
          if (!prevDirs.empty()) {
            currentRoot = prevDirs.back();
            prevDirs.pop_back();
          } else {
            currentRoot = baseRoot;
          }
          listFiles(SD.open(currentRoot));
          counter = 0;
          printFilesTFT();
        } else {
          // Вглиб
          prevDirs.push_back(currentRoot);
          if (currentRoot == baseRoot)
            currentRoot = currentRoot + selected;
          else
            currentRoot = currentRoot + "/" + selected;
          listFiles(SD.open(currentRoot));
          counter = 0;
          printFilesTFT();
        }
      } else {
        // Відкрити файл
        file_is_opened = true;
        counter = 0;
        if (currentRoot == baseRoot)
          displayFileContent(currentRoot + fileList[currentPos]);
        else
          displayFileContent(currentRoot + "/" + fileList[currentPos]);
      }
    } else if (file_is_opened) {
      file_is_opened = false;
      counter = 0;
      printFilesTFT();
    }
  }

  lastButtonState = buttonState;
}

void processEncoder() {
  if (counter != currentPos) {
    Serial.println(counter);
    currentPos = counter;
    if (!file_is_opened) {
      printFilesTFT();
    } else {
      if (0 <= currentPos < pageOffsets.size()) {
        currentPage = currentPos;
        printFilePage(openedFileName, tft);
      }
    }
  }
}

void read_encoder() {
  static uint8_t old_AB = 3;
  static int8_t encval = 0;
  static const int8_t enc_states[] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

  old_AB <<= 2;
  if (digitalRead(ENC_A)) old_AB |= 0x02;
  if (digitalRead(ENC_B)) old_AB |= 0x01;

  encval += enc_states[(old_AB & 0x0f)];

  int changevalue = 0;

  if (encval < -3) {  // Four steps forward
    changevalue = 1;
    if ((micros() - _lastIncReadTime) < _pauseLength) {
      changevalue = _fastIncrement;
    }
    _lastIncReadTime = micros();
    encval = 0;
  } else if (encval > 3) {  // Four steps backward
    changevalue = -1;
    if ((micros() - _lastDecReadTime) < _pauseLength) {
      changevalue = -_fastIncrement;
    }
    _lastDecReadTime = micros();
    encval = 0;
  }

  // Update counter based on the encoder rotation
  if (file_is_opened) {
    if (counter + changevalue < pageOffsets.size() && counter + changevalue >= 0) {
      counter += changevalue;
    }
  } else {
    if (counter + changevalue < fileCount + dirCount && counter + changevalue >= 0) {
      counter += changevalue;
    }
  }
}

void printFilesTFT() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);
  int k = 0;

  if(currentPos > 18)
    k = currentPos % 18;

  for (int i = 0 + k; i < dirCount + fileCount && k + 18; i++) {
    
    char t_name[22];                  // Буфер для скороченого імені
    if (strlen(fileList[i].c_str()) > 21) {  // 20, бо останній символ буде '\0'
      strncpy(t_name, fileList[i].c_str(), 21);
      t_name[21] = '\0';  // Гарантуємо коректне завершення рядка
    } else {
      strcpy(t_name, fileList[i].c_str());
    }

    if (i < dirCount) {
      if (i == currentPos) {
        tft.setTextColor(ST77XX_YELLOW);
      } else {
        tft.setTextColor(ST77XX_BLUE);
      }
      tft.println(t_name);
    } else {
      if (i == currentPos) {
        tft.setTextColor(ST77XX_YELLOW);
      } else {
        tft.setTextColor(ST77XX_WHITE);
      }
      tft.println(t_name);
    }
  }
}

void listFiles(File folder) {
  fileList.clear();
  fileCount = 0;
  dirCount = 0;

  if (currentRoot != baseRoot) {
    fileList.push_back("..");  // Пункт для повернення назад
    dirCount++;
  }

  while (true) {
    File entry = folder.openNextFile();
    if (!entry) break;

    String fileName = entry.name();

    if (fileName.equals("System Volume Information")) {
      entry.close();
      continue;
    }

    if (entry.isDirectory()) {
      fileList.insert(fileList.begin() + dirCount, fileName);
      dirCount++;
    } else {
      fileList.push_back(fileName);
      fileCount++;
    }

    entry.close();
  }

  folder.close();
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

void displayFileContent(const String &fileName) {
  File file = SD.open(fileName);
  if (!file) {
    Serial.println("Read Error!");
    return;
  }

  openedFileName = fileName;
  currentPage = 0;
  pageOffsets.clear();
  pageOffsets.push_back(0);  // Перша сторінка завжди з 0

  const int maxLineLength = 21;
  int lineCounter = 0;
  int totalPages = 0;
  uint32_t currentOffset = 0;  // Змінна для збереження поточної позиції

  while (file.available()) {
    uint32_t pos = file.position();
    String line = file.readStringUntil('\n');

    // Скільки частин у рядка з перенесенням
    int parts = (line.length() + maxLineLength - 1) / maxLineLength;
    lineCounter += parts;

    // Якщо набралося сторінку, рахуємо нову сторінку
    if (lineCounter >= linesPerPage) {
      totalPages++;
      pageOffsets.push_back(currentOffset);  // Зберігаємо офсет для нової сторінки
      lineCounter = 0;
    }

    // Оновлюємо поточний офсет
    currentOffset = pos;
  }

  file.close();

  Serial.print("Total pages: ");
  Serial.println(totalPages);

  // Якщо потрібно, можна обробити останню сторінку окремо
  printFilePage(openedFileName, tft);
}

void printFilePage(String filename, Adafruit_ST7735 tft) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("Can't open file");

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(15, 75);
    tft.setTextColor(ST77XX_RED);
    tft.println("Can't open file");
    delay(500);
    file_is_opened = false;
    counter = 0;
    printFilesTFT();
    return;
  }

  // Якщо offset для цієї сторінки ще не збережено — знайдемо його
  if (pageOffsets[currentPage] == 0 && currentPage != 0) {
    // Шукаємо початок цієї сторінки
    const int maxLineLength = 21;
    int lineCounter = 0;
    file.seek(pageOffsets[currentPage - 1]);  // Починаємо з попереднього відомого

    while (file.available() && lineCounter < linesPerPage) {
      uint32_t pos = file.position();
      String line = file.readStringUntil('\n');
      int parts = (line.length() + maxLineLength - 1) / maxLineLength;
      lineCounter += parts;
    }

    // Зберігаємо знайдену позицію для цієї сторінки
    pageOffsets[currentPage] = file.position();
    file.seek(pageOffsets[currentPage]);  // Перейти до неї
  } else {
    file.seek(pageOffsets[currentPage]);
  }

  // Тепер виводимо сторінку
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);
  tft.setTextColor(ST77XX_WHITE);

  int linesPrinted = 0;
  const int maxLineLength = 21;

  while (file.available() && linesPrinted < linesPerPage) {
    String line = file.readStringUntil('\n');

    for (int i = 0; i < line.length(); i += maxLineLength) {
      if (linesPrinted >= linesPerPage) break;

      String part = line.substring(i, i + maxLineLength);
      tft.println(part);
      linesPrinted++;
    }
  }

  // Показати номер сторінки
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(0, 150);
  tft.print("Page ");
  tft.print(currentPage + 1);
  tft.print("/");
  tft.print(pageOffsets.size());

  file.close();
}
