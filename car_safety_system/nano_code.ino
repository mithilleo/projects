// Sensor Pins
const int MQ135_PIN = A6; // Air Quality
const int MQ3_PIN   = A7; // Alcohol

// Communication Pins to 89S52
const int D4_OUT = 4; // Connects to P2.5
const int D3_OUT = 3; // Connects to P2.6
const int D2_OUT = 2; // Connects to P2.7

// Thresholds (Adjust these based on testing)
const int MQ135_THRESHOLD = 400; 
const int MQ3_THRESHOLD   = 400;

void setup() {
  pinMode(D4_OUT, OUTPUT);
  pinMode(D3_OUT, OUTPUT);
  pinMode(D2_OUT, OUTPUT);
  
  // Initialize communication pins to LOW (000)
  digitalWrite(D4_OUT, LOW);
  digitalWrite(D3_OUT, LOW);
  digitalWrite(D2_OUT, LOW);

  Serial.begin(9600); // For debugging thresholds
}

void loop() {
  int mq135_val = analogRead(MQ135_PIN);
  int mq3_val   = analogRead(MQ3_PIN);

  // Logic for 89S52 Communication
  if (mq135_val > MQ135_THRESHOLD) {
    // Binary 001: D4=0, D3=0, D2=1
    sendSignal(LOW, LOW, HIGH);
  } 
  else if (mq3_val > MQ3_THRESHOLD) {
    // Binary 010: D4=0, D3=1, D2=0
    sendSignal(LOW, HIGH, LOW);
  } 
  else {
    // Binary 000: System Normal
    sendSignal(LOW, LOW, LOW);
  }

  // Debugging output to Serial Monitor
  Serial.print("MQ135: "); Serial.print(mq135_val);
  Serial.print(" | MQ3: "); Serial.println(mq3_val);

  delay(200); // Small delay to prevent jitter
}

// Helper function to set the pins
void sendSignal(bool d4, bool d3, bool d2) {
  digitalWrite(D4_OUT, d4);
  digitalWrite(D3_OUT, d3);
  digitalWrite(D2_OUT, d2);
}
