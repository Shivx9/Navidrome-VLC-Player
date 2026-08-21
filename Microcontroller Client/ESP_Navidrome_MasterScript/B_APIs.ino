

// Prototypes
void refreshVisuals();

char* base = "http://192.168.1.13:5000";


void volUp() {
  snprintf(url, sizeof(url), "%s/vol?action=incr", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}

void volDown() {
  snprintf(url, sizeof(url), "%s/vol?action=decr", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}

void mute() {
  snprintf(url, sizeof(url), "%s/vol?mute=true", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}

void skip(bool forward = true) {
  if (forward) snprintf(url, sizeof(url), "%s/skip/next", base);
  else snprintf(url, sizeof(url), "%s/skip/previous", base);
  Serial.println(GET_Request(url));
  refreshVisuals();
  delay(50);
  return;
}

void newDisc(char* val) {
  snprintf(url, sizeof(url), "%s/clearq", base);
  Serial.println(GET_Request(url));
  delay(50);
  snprintf(url, sizeof(url), "%s/add/album/%s", base, val);
  Serial.println(GET_Request(url));
  delay(50);
  skip();
  return;
}

void play() {
  snprintf(url, sizeof(url), "%s/play", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}

void ffwd(bool start = true) {
  if (start) snprintf(url, sizeof(url), "%s/ffwd/start", base);
  else snprintf(url, sizeof(url), "%s/ffwd/stop", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}

void rwd(bool start = true) {
  if (start) snprintf(url, sizeof(url), "%s/rwd/start", base);
  else snprintf(url, sizeof(url), "%s/rwd/stop", base);
  Serial.println(GET_Request(url));
  delay(50);
  return;
}


void checkSongChange(){
  Serial.println("Checking for new song");
  HTTPClient http;

  // Initial check if song has changed
  snprintf(url, sizeof(url), "%s/getSong", base);
  Serial.println("beginning req");
  http.begin(url);
  int httpResponseCode = http.GET();
  Serial.println("checking response");
  if (httpResponseCode > 0) {
    if(httpResponseCode == 400){
      Serial.println("Empty playlist");
    }
    else{
      String response = http.getString();

      // Parse JSON using ArduinoJson
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, response);

      if (error) {
        Serial.println("JSON Parse failed: ");
        Serial.println(error.c_str());
        http.end();
        return; 
      }

      // Get current playing song ID
      const char* songID = doc["id"]; //.as<const char*>();
      if (songID == NULL) {
        Serial.println("Error: 'id' key missing from JSON response");
        http.end();
        return;
      }
      Serial.println(songID);

      // Register a song change
      if(strcmp(songID,lastSong)!=0){
        Serial.println("New song detected");
        strncpy(lastSong,songID, sizeof(lastSong) - 1);
        Serial.println("copied new song id");
        // Get all details for new song
        snprintf(url, sizeof(url), "%s/getSong?full=true", base);
        Serial.println("Sending request for full details");
        String response2 = GET_Request(url);
        Serial.println("Got response");
        doc.clear(); // Clear previous doc state
        Serial.println("Checking for errors");
        error = deserializeJson(doc, response2);

        if (!error) {
          Serial.println("No error");
          // Fix 3: Safeguard the secondary strcpy targets
          Serial.println("Assigning name");
          if (doc["name"]!=NULL)   strncpy(currSongName, doc["name"], sizeof(currSongName) - 1);
          Serial.println("Artist");
          if (doc["artist"]!=NULL) strncpy(currSongArtist, doc["artist"], sizeof(currSongArtist) - 1);
          Serial.println("Album");
          if (doc["album"]!=NULL)  strncpy(currSongAlbum, doc["album"], sizeof(currSongAlbum) - 1);
          
          // Ensure null-termination for safety
          // currSongName[sizeof(currSongName) - 1] = '\0';
          // currSongArtist[sizeof(currSongArtist) - 1] = '\0';
          // currSongAlbum[sizeof(currSongAlbum) - 1] = '\0';
          Serial.println("Refreshing art");
          refreshVisuals();
        } else {
          Serial.println("Failed to parse full song info JSON");
        }
      }
    }

  }
  Serial.println("Check end");
  http.end();
}



