// ESP32 Rotary Encoder with WiFi Web Server
// Connect encoder pins A and B to GPIO pins defined below

#include <WiFi.h>
#include <WebServer.h>

#define ENC_A 34  // Encoder pin A
#define ENC_B 35  // Encoder pin B
#define TICKS_PER_ROTATION 1300  // Encoder ticks per full rotation

// WiFi credentials
const char* ssid = "esp32_server";
const char* password = "12345678";

// Create web server on port 80
WebServer server(80);

volatile long encoderPos = 0;
volatile bool lastA = false;
volatile bool lastB = false;
volatile unsigned long lastTickTime = 0;

// Variables for speed calculation
long lastPos = 0;
unsigned long lastCalcTime = 0;
float currentSpeedTicks = 0;
float currentSpeedRadS = 0;
String currentDirection = "STOPPED";

void IRAM_ATTR encoderISR() {
  bool currA = digitalRead(ENC_A);
  bool currB = digitalRead(ENC_B);
  
  // Determine direction based on state transitions
  if (currA != lastA) {
    lastTickTime = micros();
    
    if (currA == currB) {
      encoderPos++;
    } else {
      encoderPos--;
    }
  }
  
  lastA = currA;
  lastB = currB;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Encoder</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
  html += ".data-box { background: #e8f4f8; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #2196F3; }";
  html += ".label { font-weight: bold; color: #555; }";
  html += ".value { font-size: 1.3em; color: #2196F3; margin: 5px 0; }";
  html += ".positive { color: #4CAF50; }";
  html += ".negative { color: #f44336; }";
  html += ".stopped { color: #9E9E9E; }";
  html += ".refresh { text-align: center; margin-top: 20px; font-size: 0.9em; color: #666; }";
  html += "</style>";
  html += "<script>";
  html += "setInterval(function() { location.reload(); }, 500);";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🔄 Rotary Encoder Monitor</h1>";
  
  html += "<div class='data-box'>";
  html += "<div class='label'>Position (ticks)</div>";
  html += "<div class='value'>" + String(encoderPos) + "</div>";
  html += "</div>";
  
  html += "<div class='data-box'>";
  html += "<div class='label'>Speed (ticks/s)</div>";
  String speedClass = currentSpeedTicks > 0 ? "positive" : (currentSpeedTicks < 0 ? "negative" : "stopped");
  html += "<div class='value " + speedClass + "'>";
  if (currentSpeedTicks > 0) html += "+";
  html += String(currentSpeedTicks, 2) + "</div>";
  html += "</div>";
  
  html += "<div class='data-box'>";
  html += "<div class='label'>Speed (rad/s)</div>";
  html += "<div class='value " + speedClass + "'>";
  if (currentSpeedRadS > 0) html += "+";
  html += String(currentSpeedRadS, 3) + "</div>";
  html += "</div>";
  
  html += "<div class='data-box'>";
  html += "<div class='label'>Direction</div>";
  html += "<div class='value " + speedClass + "'>" + currentDirection + "</div>";
  html += "</div>";
  
  html += "<div class='refresh'>Auto-refreshing every 0.5s</div>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"position\":" + String(encoderPos) + ",";
  json += "\"speed_ticks\":" + String(currentSpeedTicks, 2) + ",";
  json += "\"speed_rad\":" + String(currentSpeedRadS, 3) + ",";
  json += "\"direction\":\"" + currentDirection + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32 Rotary Encoder with WiFi");
  
  // Configure encoder pins as inputs with pull-ups
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  
  // Read initial states
  lastA = digitalRead(ENC_A);
  lastB = digitalRead(ENC_B);
  
  // Attach interrupts to both pins for any change
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);
  
  Serial.println("Encoder initialized");
  Serial.println("Ticks per rotation: " + String(TICKS_PER_ROTATION));
  
  // Set up Access Point
  Serial.println("\nConfiguring Access Point...");
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP Started!");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  
  server.begin();
  Serial.println("Web server started");
  Serial.println("Connect to WiFi and visit: http://" + IP.toString());
  Serial.println("\n--- Encoder Data ---");
}

void loop() {
  server.handleClient();
  
  unsigned long currentTime = millis();
  
  // Calculate speed every 100ms
  if (currentTime - lastCalcTime >= 100) {
    noInterrupts();
    long currentPos = encoderPos;
    unsigned long lastTick = lastTickTime;
    interrupts();
    
    // Calculate elapsed time
    float deltaTime = (currentTime - lastCalcTime) / 1000.0; // seconds
    
    // Calculate position change
    long deltaPos = currentPos - lastPos;
    
    // Calculate speed in ticks per second
    currentSpeedTicks = deltaPos / deltaTime;
    
    // Convert to radians per second
    currentSpeedRadS = currentSpeedTicks * (2.0 * PI / TICKS_PER_ROTATION);
    
    // Check if encoder is moving
    unsigned long timeSinceLastTick = millis() - (lastTick / 1000);
    bool isMoving = timeSinceLastTick < 200;
    
    // Update direction
    if (currentSpeedRadS > 0) {
      currentDirection = "CW";
    } else if (currentSpeedRadS < 0) {
      currentDirection = "CCW";
    } else {
      currentDirection = "STOPPED";
    }
    
    // Print to Serial
    if (isMoving || abs(currentSpeedTicks) > 0.1) {
      Serial.print("Pos: ");
      Serial.print(currentPos);
      Serial.print(" | Speed: ");
      
      if (currentSpeedTicks > 0) {
        Serial.print("+");
      }
      Serial.print(currentSpeedTicks, 2);
      Serial.print(" ticks/s | ");
      
      if (currentSpeedRadS > 0) {
        Serial.print("+");
      }
      Serial.print(currentSpeedRadS, 3);
      Serial.print(" rad/s | ");
      Serial.println(currentDirection);
    }
    
    lastPos = currentPos;
    lastCalcTime = currentTime;
  }
  
  delay(10);
}

// Optional: Function to reset position
void resetEncoder() {
  noInterrupts();
  encoderPos = 0;
  interrupts();
}

// Optional: Function to set position to specific value
void setEncoderPosition(long pos) {
  noInterrupts();
  encoderPos = pos;
  interrupts();
}
