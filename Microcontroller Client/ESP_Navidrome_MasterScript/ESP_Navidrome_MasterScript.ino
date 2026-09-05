#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <SPI.h>
#include <string.h>

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>


// SPI pins (Shared)
#define SPI_SCLK 5
#define SPI_MISO 6
#define SPI_MOSI 7


// Function prototypes
void ffwd(bool start);
void rwd(bool start);
void skip(bool forward);
void play();
void newDisc(char* val);
void mute();
void volUp();
void volDown();
void enforceRotation();
void checkSongChange();
void scrolltitle();
bool read_rfid();
void setSleep(bool flag);



// Input-related variables
const int debounceTime = 150;
const int CAP1_PIN = 1;
const int CAP2_PIN = 2;
const int SODA1_PIN = 4;
const int cap1_thresh = 27000;
const int cap2_thresh = 27000;
const int soda1_thresh = 26000;
const int long_press_thresh = 500;
bool cap1_press = false;
bool cap2_press = false;
bool soda1_press = false;
bool cap1_long_press = false;
bool cap2_long_press = false;
unsigned long cap1_time = 0;
unsigned long cap2_time = 0;
unsigned long soda1_time = 0;

int cap1 = 0;
int cap2 = 0;
int soda1 = 0;
int currTime = 0;

// Variables for loop intervals
const unsigned long INTERVAL_FAST = 100;     // Fast checks
const unsigned long INTERVAL_MEDIUM = 505;  // Medium tasks
const unsigned long INTERVAL_SLOW = 1007;   // Slow tasks
unsigned long previousMillisFast = 0;
unsigned long previousMillisMed = 0;
unsigned long previousMillisSlow = 0;
unsigned long currentMillis = 0;

int songUpdatePoint = 0;

#define RFID_CS 38 
#define RFID_RST 21

// Initialize the MFRC522v2 Driver wrapper
MFRC522DriverPinSimple csPin(RFID_CS);
MFRC522DriverSPI driver{ csPin, SPI};
MFRC522 mfrc522{ driver };

// Misc. variables
int rot_counter = 0;
char charBuffer[10];
String rot_dir = "";
char rfidChar[32];
const char* imageUrl = "getArt";  // Loading jpeg
char lastSong[32] = "",currSongName[128]="", currSongArtist[128]="",currSongAlbum[128]="";
char url[64];
bool sleepState = false;


//test variables
bool display_debug = false;  //disables network when set to true - prevents brown-outs by reducing power-draw when connected to USB
bool network_debug = false;   //disables display when set to true - prevents brown-outs by reducing power-draw when connected to USB


void setup() {



  Serial.begin(115200);
  SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, -1);
  delay(500);
  Serial.println("Initialized Serial and SPI communications");

  digitalWrite(LED_BUILTIN, HIGH);

  // Initialize PSRAM and Allocate Memory
  if (!psramInit()) {
    Serial.println("PSRAM initialization failed!");
  }


  delay(500);
  init_controls();

  if (!display_debug) {
    delay(500);
    init_wifi();
  }

  delay(500);
  init_rfid();
  

  if (!network_debug) {
    delay(500);
    init_display();
  }

    // set up loop2
  xTaskCreatePinnedToCore(
    loop2,    // Function to implement the task
    "loop2",  // Name of the task
    2000,     // Stack size in words
    NULL,     // Task input parameter
    0,        // Priority of the task
    NULL,     // Task handle.
    0         // Core where the task should run
  );

  digitalWrite(LED_BUILTIN, LOW);
}


// the setup function runs once when you press reset or power the board


// the loop function runs over and over again forever
void loop() {


  // Serial.print("loop() running in core ");
  // Serial.println(xPortGetCoreID());
  // itoa(rot_counter,charBuffer,10);
  // display_text(charBuffer);
  // Serial.print("counter : ");
  // Serial.print(rot_counter);

  currentMillis = millis();

  // Fast-freqeuency code
  if (currentMillis - previousMillisFast >= INTERVAL_FAST) {
    previousMillisFast = currentMillis;

    
    if(!sleepState){
      // Scrolling song title text
      scrolltitle();
    }

    ///////////////////  Input section
    // Read the raw value
    cap1 = touchRead(CAP1_PIN);
    cap2 = touchRead(CAP2_PIN);
    soda1 = touchRead(SODA1_PIN);
    currTime = millis();

    // SODA1 - Play/Pause
    if ((currTime - soda1_time) > debounceTime) {
      if (soda1 > soda1_thresh) {
        if (!soda1_press) {
          soda1_press = true;
          soda1_time = currTime;
          Serial.println("Soda1 Pressed");
        }
      } else if (soda1_press) {
        soda1_press = false;
        Serial.println("Soda1 Released");
        play();
      }
    }

    // CAP1 - skip-ahead/ffw
    if ((currTime - cap1_time) > debounceTime) {
      if (cap1 > cap1_thresh) {
        if (!cap1_press) {
          cap1_press = true;
          cap1_long_press = false;
          cap1_time = currTime;
          Serial.println("Cap1 Pressed");
        } else if (!cap1_long_press && (currTime - cap1_time) > long_press_thresh) {
          cap1_long_press = true;
          // Trigger long-press action
          ffwd(true);
        }
      } else if (cap1_press) {
        cap1_press = false;

        if (cap1_long_press) {
          // End long-press action
          Serial.println("Cap1 Long-press released");
          ffwd(false);
        } else {
          // Execute short-press action
          Serial.println("Cap1 short-press release");
          skip(true);
        }
      }
    }

    // CAP2 - back/rwd
    if ((currTime - cap2_time) > debounceTime) {
      if (cap2 > cap2_thresh) {
        if (!cap2_press) {
          cap2_press = true;
          cap2_long_press = false;
          cap2_time = currTime;
          Serial.println("Cap2 Pressed");
        } else if (!cap2_long_press && (currTime - cap2_time) > long_press_thresh) {
          cap2_long_press = true;
          // Trigger long-press action
          rwd(true);
        }
      } else if (cap2_press) {
        cap2_press = false;
        Serial.println("Cap2 Released");
        if (cap2_long_press) {
          // End long-press action
          rwd(false);
        } else {
          // Execute short-press action
          skip(false);
        }
      }
    }

    ///////////////////  END - Input section
  }

  // Medium-freqeuency code
  if (currentMillis - previousMillisMed >= INTERVAL_MEDIUM) {
    previousMillisMed = currentMillis;

    if(!sleepState) enforceRotation();

    // New disc read
    if (read_rfid()) {
      Serial.println(rfidChar);
      newDisc(rfidChar);
    }
  }

  // Slow-freqeuency code
  if (currentMillis - previousMillisSlow >= INTERVAL_SLOW) {
    previousMillisSlow = currentMillis;

    

    // check for song changes every 5 cycles
    if(songUpdatePoint<5){
      songUpdatePoint++;
      Serial.println("+");
    }
    else{
      songUpdatePoint = 0;
      checkSongChange();
    }
  }



  delay(10);
}

// the loop2 function also runs forver but as a parallel task
void loop2(void* pvParameters) {
  while (1) {
    // delay(1000);
    // Serial.print("Hello World from loop2() at ");
    // Serial.println(xPortGetCoreID());
    // Update rotation only if image arrays and canvas exist
    // enforceRotation();
    delay(10000);
    setSleep(true);
    delay(10000);  // Frame pacing
    setSleep(false);

  }
}
