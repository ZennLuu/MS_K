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
bool blockEncoder = false;

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

  File root = SD.open(baseRoot);
  listFiles(root);
  printFilesTFT();
}

void loop() {
  delay(10);
  buttonState = digitalRead(E_BUTTON);
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
        blockEncoder = true;
        if (currentRoot == baseRoot)
          displayFileContent(currentRoot + fileList[currentPos]);
        else
          displayFileContent(currentRoot + "/" + fileList[currentPos]);
      }
    } else if (file_is_opened) {
      file_is_opened = false;
      blockEncoder = false;
      printFilesTFT();
    }
  }

  lastButtonState = buttonState;

  // If count has changed print the new value to serial
  if (counter != currentPos) {
    Serial.println(counter);
    currentPos = counter;
    if (!file_is_opened)
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

void read_encoder() {
  static uint8_t old_AB = 3;
  static int8_t encval = 0;
  static const int8_t enc_states[] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

  old_AB <<= 2;

  if (digitalRead(ENC_A)) old_AB |= 0x02;
  if (digitalRead(ENC_B)) old_AB |= 0x01;

  encval += enc_states[(old_AB & 0x0f)];

  if (file_is_opened) {
    if (encval > 3) {
      if (currentPage + 1 < pageOffsets.size()) {
        currentPage++;
        printFilePage(); 
      }
      encval = 0;
    } else if (encval < -3) {
      if (currentPage > 0) {
        currentPage--;
        printFilePage();
      }
      encval = 0;
    }
  } else if (!buttonState && !blockEncoder) {
    if (encval > 3) {
      if (counter + 1 < fileCount + dirCount)
        counter = counter + 1;
      encval = 0;
    } else if (encval < -3) {
      if (counter - 1 >= 0)
        counter = counter - 1;
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

  openedFileName = fileName;
  currentPage = 0;
  pageOffsets.clear();
  pageOffsets.push_back(0);  // Перша сторінка починається з 0

  const int maxLineLength = 21;
  int lineCounter = 0;

  while (file.available()) {
    uint32_t pos = file.position();

    String line = file.readStringUntil('\n');

    // Скільки рядків буде після переносу?
    int parts = (line.length() + maxLineLength - 1) / maxLineLength;

    lineCounter += parts;

    // Якщо заповнено сторінку — запам’ятай позицію початку наступної
    if (lineCounter >= linesPerPage) {
      pageOffsets.push_back(file.position());
      lineCounter = 0;
    }
  }

  file.close();

  printFilePage();
}

void printFilePage() {
  File file = SD.open(openedFileName);
  if (!file) {
    Serial.println("Can't reopen file");
    return;
  }

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 3);
  tft.setTextColor(ST77XX_WHITE);

  const int maxLineLength = 21;
  int linesPrinted = 0;

  // Перейти до початку потрібної сторінки
  if (currentPage < pageOffsets.size()) {
    file.seek(pageOffsets[currentPage]);
  } else {
    file.close();
    return;
  }

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
  tft.setCursor(0, 160);
  tft.print("Page ");
  tft.print(currentPage + 1);
  tft.print("/");
  tft.print(pageOffsets.size());

  file.close();
}