unsigned long last_run = 0;


void init_controls() {
  Serial.println("Setting up controls");
  // attachInterrupt(digitalPinToInterrupt(4), shaft_moved, FALLING);  // Clk -> D3/D2 for interrupt
  // pinMode(5, INPUT);                                                // DT
  // pinMode(6, INPUT);                                                // SW

  // First reading processed here since it leads to a false spike
  cap1 = touchRead(CAP1_PIN);
  cap2 = touchRead(CAP2_PIN);
  soda1 = touchRead(SODA1_PIN);
}

void shaft_moved() {
  if (millis() - last_run > 10) {
    if (digitalRead(5) == 1) {
      rot_counter++;
      rot_dir = "CW";
      volUp();
    }
    if (digitalRead(5) == 0) {
      rot_counter--;
      rot_dir = "CCW";
      volDown();
    }
    last_run = millis();
  }
}
