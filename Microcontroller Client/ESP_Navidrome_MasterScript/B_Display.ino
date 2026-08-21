#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library for ST7735
#include <SPI.h>
#include <TJpg_Decoder.h>


// Match your working physical pin layout
// #define TFT_SCLK 39
// #define TFT_MOSI 38
#define TFT_DC 8 //A0
#define TFT_RST 9
#define TFT_CS 10

// Initialize hardware SPI using designated pins on ESP32-S3
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Configuration variables
String scrollText = "ESP32-S3 N15R8";
int scrollPos;
int textWidth;
int textHeight = 16;
int screenWidth = 160;

// const int TFT_BackLightPin = 21;
const int freq = 200;
const int ledChannel = 0;
const int resolution = 4;

// Create an off-screen drawing window (Canvas) only as wide as the screen
// Width: 160 pixels, Height: 16 pixels. (Uses only ~5.1 KB of RAM)
GFXcanvas16 *canvas;

// Layout & Image Configurations
const int tftWidth = 128;
const int tftHeight = 160;
const int imgDim = 64;
const int radius = 32;
const int centerX = 32;
const int centerY = 32;
const int imgPosX = (tftWidth - imgDim) / 2;  // X: 32
const int imgPosY = 64;                       // Below title text

// Memory Buffers (Allocated in PSRAM)
uint16_t* decodedRawImage = NULL;
uint16_t* maskedImage = NULL;
float rotationAngle = 0.0;

// Callback function used by TJpg_Decoder to output 16-bit pixels to our array
bool tjpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  int idx = 0;
  for (int j = y; j < y + h; j++) {
    for (int i = x; i < x + w; i++) {
      if (i < imgDim && j < imgDim) {
        decodedRawImage[j * imgDim + i] = bitmap[idx++];
      }
    }
  }
  return true;
}


void init_display() {

  Serial.println("Setting up display");
  // set as params later
  bool portrait = false, reversed = false;


  // pinMode(TFT_BackLightPin, OUTPUT);
  // digitalWrite(TFT_BackLightPin, HIGH);

  tft.initR(INITR_BLACKTAB);
  int rot = 1;
  if (portrait) {
    rot = 0;
  }
  if (reversed) {
    tft.setRotation(rot + 2);
  } else {
    tft.setRotation(rot);
  }
  delay(1000);
  tft.fillScreen(ST7735_BLACK);

  decodedRawImage = (uint16_t*)ps_malloc(imgDim * imgDim * sizeof(uint16_t));
  maskedImage = (uint16_t*)ps_malloc(imgDim * imgDim * sizeof(uint16_t));
}

inline void display_text(char *text) {
  tft.setTextSize(2);                            // 1 = small, 2 = medium, etc.
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);  // Text color, Background color

  // Set the cursor position (X, Y)
  tft.setCursor(10, 50);

  // Print text to display
  tft.println(text);
}



///////// Handling Album Art

// Downloads raw JPEG bytes from HTTP server straight to PSRAM and calls the Decoder
bool downloadAndDecodeJPEG() {
  WiFiClient client;
  HTTPClient http;
  bool success = false;

  Serial.println("Fetching image via HTTP GET...");
  snprintf(url, sizeof(url), "%s/%s", base, imageUrl);
  if (http.begin(client, url)) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      int imageSize = http.getSize();
      if (imageSize <= 0) {
        Serial.println("Error: Unknown image stream length");
        http.end();
        return false;
      }

      // Temporary PSRAM buffer to hold incoming raw JPEG data stream
      uint8_t* rawJpegBuffer = (uint8_t*)ps_malloc(imageSize);
      if (rawJpegBuffer == NULL) {
        Serial.println("Failed to allocate raw JPEG buffer in PSRAM");
        http.end();
        return false;
      }

      WiFiClient* stream = http.getStreamPtr();
      int bytesRead = 0;

      while (http.connected() && (bytesRead < imageSize)) {
        size_t availableBytes = stream->available();
        if (availableBytes) {
          int readNow = stream->readBytes(&rawJpegBuffer[bytesRead], availableBytes);
          bytesRead += readNow;
        }
        delay(1);
      }

      Serial.printf("Downloaded %d bytes. Parsing JPEG formatting...\n", bytesRead);

      // Setup the JPEG decoder tracking configurations
      TJpgDec.setJpgScale(1);
      TJpgDec.setCallback(tjpgCallback);

      // Execute conversion processing out of raw array
      uint8_t dresult = TJpgDec.drawJpg(0, 0, rawJpegBuffer, bytesRead);
      if (dresult == 0) {  // 0 matches JDR_OK
        Serial.println("JPEG successfully parsed into RGB565 structure.");
        success = true;
      } else {
        Serial.printf("JPEG decoding error code: %d\n", dresult);
        if (dresult == 1) Serial.println("-> Error: Input stream error (bad data or short file)");
        if (dresult == 2) Serial.println("-> Error: Identifier error (Not a valid JPEG file!)");
        if (dresult == 3) Serial.println("-> Error: Unsupported JPEG format (Is it progressive?)");
      }
      if (dresult == JDR_OK) {
        Serial.println("JPEG successfully parsed into RGB565 structure.");
        success = true;
      } else {
        Serial.printf("JPEG decoding error: %d\n", dresult);
      }

      free(rawJpegBuffer);  // Clear intermediate download space instantly
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }


  return success;
}

void applyCircularMask() {
  for (int y = 0; y < imgDim; y++) {
    for (int x = 0; x < imgDim; x++) {
      int idx = y * imgDim + x;
      int dx = x - centerX;
      int dy = y - centerY;

      if ((dx * dx) + (dy * dy) <= (radius * radius)) {
        maskedImage[idx] = decodedRawImage[idx];
      } else {
        maskedImage[idx] = ST7735_BLACK;  // Match background
      }
    }
  }
}

// Renders the rotated frame directly onto the GFXCanvas structure
void renderRotatedImageToCanvas(float angle) {
  canvas->fillScreen(ST7735_BLACK);

  // Pre-calculate floats ONLY once per frame
  float cA = cos(angle);
  float sA = sin(angle);

  // Convert to 16-bit fixed-point integers (scaled by 256)
  int32_t cosA_fixed = (int32_t)(cA * 256.0f);
  int32_t sinA_fixed = (int32_t)(sA * 256.0f);

  for (int y = 0; y < imgDim; y++) {
    int yt = y - centerY;

    // Pre-calculate the Y-axis component before entering the inner X loop
    int32_t yt_sinA = yt * sinA_fixed;
    int32_t yt_cosA = yt * cosA_fixed;

    for (int x = 0; x < imgDim; x++) {
      int xt = x - centerX;

      // Pure integer math with bit-shifting (>> 8 is equivalent to dividing by 256)
      int srcX = ((xt * cosA_fixed + yt_sinA) >> 8) + centerX;
      int srcY = ((-xt * sinA_fixed + yt_cosA) >> 8) + centerY;

      if (srcX >= 0 && srcX < imgDim && srcY >= 0 && srcY < imgDim) {
        canvas->drawPixel(x, y, maskedImage[srcY * imgDim + srcX]);
      }
    }
  }
}


void refreshVisuals(){

  // Fetch, Decode, and Apply Circle Crop
  if (downloadAndDecodeJPEG()) {
    applyCircularMask();

    // Allocate the Adafruit GFX Canvas context inside memory
    canvas = new GFXcanvas16(imgDim, imgDim);
    Serial.println("Image fetched.Can start rotation loop.");
  } else {
    Serial.println("Process halted due to Download/Decode failure.");
  }
}

void enforceRotation(){
  if (decodedRawImage != NULL && maskedImage != NULL && canvas != NULL) {
    renderRotatedImageToCanvas(rotationAngle);

    // Push the compiled off-screen canvas onto the physical display
    tft.drawRGBBitmap(imgPosX, imgPosY, canvas->getBuffer(), imgDim, imgDim);

    rotationAngle += 0.05;        // Adjust step value to speed up or slow down
    if (rotationAngle >= 6.28) {  //2 * PI) {
      rotationAngle = 0;
    }
  }
}
