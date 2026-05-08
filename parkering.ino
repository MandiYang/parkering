#include <U8g2lib.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include "WiFiS3.h"
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h" 
#include "webpage.h"
//NEOPIXEL
#define PIN        8
#define NUMPIXELS 24

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
ArduinoLEDMatrix matrix;

const int seekPin1 = 4;
const int seekPin2 = 2;

const int maxPlatser = 4;
int ledigaPlatser = maxPlatser;
int direction;
int sensorIN;
int sensorOUT;
const int SERVO_PIN = 6;
//Buzzer
const int BUZZER_PIN=12;
bool buzzerActive = false;
unsigned long buzzerStart = 0;

const int BUZZER_DURATION = 800; // ms

//Servobom
const int OPEN_ANGLE = 0;
const int CLOSED_ANGLE = 90;
const int GATE_OPEN_TIME = 3000;
unsigned long GATE_TEMP_TIME = 0;
bool bomArOppen = false;
Servo bomServo;
int currentGateAngle = CLOSED_ANGLE;

//Wifi server
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

//Checkmovement stuff
static int checkState = 0;

int led =  10;
int status = WL_IDLE_STATUS;
WiFiServer server(80);
String displayMsg="Lediga platser: " + String(ledigaPlatser);
String displayMsg2="Max platser: " + String(maxPlatser);

void setup() {
  Serial.begin(9600);
  matrix.begin();
  pixels.begin();
  pixels.setBrightness(50); // Sätt ljusstyrkan (0-255)
  pinMode(seekPin1, INPUT_PULLUP);
  pinMode(seekPin2, INPUT_PULLUP);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  oledWrite(displayMsg, displayMsg2);
  updateLights();
  updateMatrix();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 5 seconds for connection:
    delay(5000);
  }
  server.begin();    // start the web server on port 80
  printWifiStatus(); // you're connected now, so print out the status
  bomServo.attach(SERVO_PIN);
  delay(500);
  setGateAngle(CLOSED_ANGLE);
}

void loop() {
  sensorIN = digitalRead(seekPin1);
  sensorOUT = digitalRead(seekPin2);
  direction = checkMovement(sensorIN, sensorOUT);
  /*
  Serial.print(sensorIN);
  Serial.print("   ");
  Serial.print(sensorOUT);
  Serial.print("   ");
  Serial.print(direction);
  Serial.print("   ");
  Serial.println(ledigaPlatser);
  */
  updateLedigaplatser();
  bomAction();
  webServer();
  // Måste alltid köras
  updateBuzzer();
}

// Returnerar: 0 = Ingen rörelse, 1 = IN, 2 = UT
int checkMovement(int seekIN, int seekOUT) {
  static unsigned long lastChange = 0;
  int result = 0;

  // SÄKERHET: Nollställ om sensorerna är blockerade för länge (t.ex. 30 sekunder)
  if (seekIN == LOW || seekOUT == LOW) {
    if (millis() - lastChange > 30000) {
      checkState = 0;
    }
  } else {
    lastChange = millis();
  }

  // 1. Vänta på start-trigger (endast en sensor täckt i taget för att undvika felstart)
  if (checkState == 0) {
    if (seekIN == LOW && seekOUT == HIGH) { 
      checkState = 1; 
    }  // Bil startar IN
    else if (seekOUT == LOW && seekIN == HIGH) {
      checkState = 2;
    }  // Bil startar UT
  }

  // 2. Logik för att fullfölja passagen
  if (checkState == 1 && seekOUT == LOW) {
    result = 1;  // IN-passagen bekräftad
    checkState = 3;   // Lås tills sensorerna är fria
  } else if (checkState == 2 && seekIN == LOW) {
    result = 2;  // UT-passagen bekräftad
    checkState = 3;   // Lås tills sensorerna är fria
  }

  // 3. Lås tillstånd tills båda sensorerna är fria igen
  if (checkState == 3 && seekIN == HIGH && seekOUT == HIGH) {
    checkState = 0;
  }
  return result;
}

void updateLedigaplatser() {
  if ((direction == 1) && (ledigaPlatser > 0)) {  // IN
    ledigaPlatser--;
    displayMsg = "Lediga platser: " + String(ledigaPlatser);
    displayMsg2 = "Max platser: " + String(maxPlatser);
    oledWrite(displayMsg, displayMsg2);
    updateMatrix();
    updateLights();
    direction = 0;
  } else if ((direction == 2) && (ledigaPlatser < maxPlatser)) {  // UT
    ledigaPlatser++;
    displayMsg = "Lediga platser: " + String(ledigaPlatser);
    displayMsg2 = "Max platser: " + String(maxPlatser);
    oledWrite(displayMsg, displayMsg2);
    updateMatrix();
    updateLights();
    direction = 0;
  }
  else{
    return;
  }
}

void oledWrite(String text1, String text2) {
  u8g2.firstPage();
  do {
    u8g2.drawStr(5, 45, text1.c_str());
    u8g2.drawStr(5, 30, text2.c_str());
  } while (u8g2.nextPage());
}

void setGateAngle(int angle) {
  if (currentGateAngle != angle) {
    bomServo.write(angle);
    currentGateAngle = angle;
  }
}

void openGate(){
  setGateAngle(OPEN_ANGLE);
  bomArOppen = true;
  GATE_TEMP_TIME = millis();
}

void closeGate(){
  setGateAngle(CLOSED_ANGLE);
  bomArOppen = false;
}

void bomAction() {
  if (sensorIN == LOW && ledigaPlatser > 0) {  // Bil kör in
    openGate();
  } 
  else if (sensorIN == LOW && ledigaPlatser <= 0) { // Bil kör in men inga lediga platser
    triggerBuzzerWarning();
    checkState=0;
  }
  else if (sensorOUT == LOW) {  // Bil kör ut
    openGate();
  }

  if ((millis() - GATE_TEMP_TIME) > GATE_OPEN_TIME && sensorIN == HIGH && sensorOUT == HIGH) {
    closeGate();
  }

  // --- 2. SMART-STÄNGNING ---
  // Om bommen är öppen, stäng den först när båda sensorerna är fria (HIGH)
  /*if (bomArOppen && sensorIN == HIGH && sensorOUT == HIGH) {
    checkState = false if (sensorIN == LOW || sensorOUT == LOW) {
      checkState = !checkState
    }
    if (checkState) {
      bomServo.write(CLOSED_ANGLE);
    }
    bomArOppen = false;
  }*/
}

// Funktion för att styra färg baserat på lediga platser
uint32_t lerpColor(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, float t) {
  t = constrain(t, 0.0, 1.0);

  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;

  return pixels.Color(r, g, b);
}

void updateLights() {
  float fillLevel = (float)(maxPlatser - ledigaPlatser) / maxPlatser;
  fillLevel = constrain(fillLevel, 0.0, 1.0);

  uint32_t color;

  if (fillLevel < 0.5) {
    // Grön - Gul
    float t = fillLevel * 2.0; // 0 → 1
    color = lerpColor(0, 255, 0,   255, 255, 0, t);

  } else {
    // Gul -> Röd
    float t = (fillLevel - 0.5) * 2.0; // 0 → 1
    color = lerpColor(255, 255, 0,   255, 0, 0, t);
  }

  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, color);
  }

  pixels.show();
}

// Hjälpfunktion för att sätta färg på alla pixlar
void setAllPixels(uint32_t color) {
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
}

// Funktion som ritar en pixel per ledig plats på den inbyggda matrisen
void updateMatrix() {
  // Matrisen på R4 är 8 rader hög och 12 kolumner bred
  uint8_t frame[8][12] = {0}; 

  int tanda = 0;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 12; c++) {
      if (tanda < (maxPlatser-ledigaPlatser)) {
        frame[r][c] = 1; // Tänd denna pixel
        tanda++;
      }
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

void triggerBuzzerWarning() {
  if (!buzzerActive) {
    buzzerActive = true;
    buzzerStart = millis();
    tone(BUZZER_PIN, 2000); // 2000 Hz
  }
}

void updateBuzzer() {
  if (buzzerActive && millis() - buzzerStart > BUZZER_DURATION) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

String generateHTML() {

  String html = webpage;

  html.replace("%MAX%", String(maxPlatser));

  String statusMsg;

  if (ledigaPlatser <= 0) {
    statusMsg = "<p class='full'>Parkeringen är full!</p>";
  }
  else if (ledigaPlatser <= (maxPlatser / 2)) {
    statusMsg = "<p class='warning'>Få platser kvar: " + String(ledigaPlatser) + "</p>";
  }
  else {
    statusMsg = "<p class='ledigt'>Lediga platser: " + String(ledigaPlatser) + "</p>";
  }

  html.replace("%STATUS%", statusMsg);

  return html;
}

void webServer() {
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out to the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:

            client.print(generateHTML()); 
            
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
      
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}