#include <U8g2lib.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include "WiFiS3.h"
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h" 
#define PIN        9
#define NUMPIXELS 24

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
ArduinoLEDMatrix matrix;

const int seekPin1 = 5;
const int seekPin2 = 3;

const int maxPlatser = 6;
int ledigaPlatser = maxPlatser;
int direction;
int sensorIN;
int sensorOUT;
const int SERVO_PIN = 6;

//Servobom
const int OPEN_ANGLE = 0;
const int CLOSED_ANGLE = 90;
const int GATE_OPEN_TIME = 3000;
int GATE_TEMP_TIME = 0;
bool bomArOppen = false;
Servo bomServo;

//Wifi server
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  10;
int status = WL_IDLE_STATUS;
WiFiServer server(80);
String displayMsg="Lediga platser: " + String(ledigaPlatser);

void setup() {
  Serial.begin(9600);
  matrix.begin();
  pixels.begin();
  pixels.setBrightness(50); // Sätt ljusstyrkan (0-255)
  bomServo.attach(SERVO_PIN);
  pinMode(seekPin1, INPUT);
  pinMode(seekPin2, INPUT);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  bomServo.write(CLOSED_ANGLE);
  oledWrite(displayMsg);
  updateLights();
  updateMatrix();
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
    // wait 6 seconds for connection:
    delay(6000);
  }
  server.begin();    // start the web server on port 80
  printWifiStatus(); // you're connected now, so print out the status
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
}

// Returnerar: 0 = Ingen rörelse, 1 = IN, 2 = UT
int checkMovement(int seekIN, int seekOUT) {
  static int state = 0;
  static unsigned long lastChange = 0;
  int result = 0;

  // SÄKERHET: Nollställ om sensorerna är blockerade för länge (t.ex. 60 sekunder)
  if (seekIN == LOW || seekOUT == LOW) {
    if (millis() - lastChange > 60000) {
      state = 0;
    }
  } else {
    lastChange = millis();
  }

  // 1. Vänta på start-trigger (endast en sensor täckt i taget för att undvika felstart)
  if (state == 0) {
    if (seekIN == LOW && seekOUT == HIGH) { state = 1; }  // Bil startar IN
    else if (seekOUT == LOW && seekIN == HIGH) {
      state = 2;
    }  // Bil startar UT
  }

  // 2. Logik för att fullfölja passagen
  if (state == 1 && seekOUT == LOW) {
    result = 1;  // IN-passagen bekräftad
    state = 3;   // Lås tills sensorerna är fria
  } else if (state == 2 && seekIN == LOW) {
    result = 2;  // UT-passagen bekräftad
    state = 3;   // Lås tills sensorerna är fria
  }

  // 3. Lås tillstånd tills båda sensorerna är fria igen
  if (state == 3 && seekIN == HIGH && seekOUT == HIGH) {
    state = 0;
  }
  return result;
}

void updateLedigaplatser() {
  if ((direction == 1) && (ledigaPlatser > 0)) {  // IN
    ledigaPlatser--;
    displayMsg = "Lediga platser: " + String(ledigaPlatser);
    oledWrite(displayMsg);
    updateMatrix();
    updateLights();
  } else if ((direction == 2) && (ledigaPlatser < maxPlatser)) {  // UT
    ledigaPlatser++;
    displayMsg = "Lediga platser: " + String(ledigaPlatser);
    oledWrite(displayMsg);
    updateMatrix();
    updateLights();
  }
}

void oledWrite(String text) {
  u8g2.firstPage();
  do {
    u8g2.drawStr(5, 45, text.c_str());
  } while (u8g2.nextPage());
}

void bomAction() {
  if (sensorIN == 0 && ledigaPlatser > 0) {  // Bil kör in
    GATE_TEMP_TIME = millis();
    bomServo.write(OPEN_ANGLE);
    bomArOppen = true;
  } else if (sensorOUT == 0) {  // Bil kör ut
    GATE_TEMP_TIME = millis();
    bomServo.write(OPEN_ANGLE);
    bomArOppen = true;
  }

  if ((millis() - GATE_TEMP_TIME) > GATE_OPEN_TIME && sensorIN == HIGH && sensorOUT == HIGH) {
    bomServo.write(CLOSED_ANGLE);
    bomArOppen = false;
  }

  // --- 2. SMART-STÄNGNING ---
  // Om bommen är öppen, stäng den först när båda sensorerna är fria (HIGH)
  /*if (bomArOppen && sensorIN == HIGH && sensorOUT == HIGH) {
    state = false if (sensorIN == LOW || sensorOUT == LOW) {
      state = !state
    }
    if (state) {
      bomServo.write(CLOSED_ANGLE);
    }
    bomArOppen = false;
  }*/
}

// Funktion för att styra färg baserat på lediga platser
void updateLights() {
  if (ledigaPlatser <= 0) {
    setAllPixels(pixels.Color(255, 0, 0)); // RÖTT - Fullt
  } else {
    setAllPixels(pixels.Color(0, 255, 0)); // GRÖNT - Ledigt
  }
  /*
  if (bomArOppen) {
    setAllPixels(pixels.Color(255, 165, 0)); // GULT - Bom öppen
  } else if (ledigaPlatser <= 0) {
    setAllPixels(pixels.Color(255, 0, 0)); // RÖTT - Fullt
  } else {
    setAllPixels(pixels.Color(0, 255, 0)); // GRÖNT - Ledigt
  }*/
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
      if (tanda < ledigaPlatser) {
        frame[r][c] = 1; // Tänd denna pixel
        tanda++;
      }
    }
  }
  matrix.renderBitmap(frame, 8, 12);
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
            // FIXED
            String htmlContent;
            if (ledigaPlatser <= 0) {
              htmlContent = "<html><head>"
                  "<meta charset='UTF-8'>"
                  "<meta http-equiv='refresh' content='3'>"
                  "</head><body>"
                  "<h1>Parkeringsplatser</h1>"
                  "<p>Max antal platser: " + String(maxPlatser) + "</p>"
                  "<p style='color:red;'><b>Parkeringen är full!</b></p>"
                  "</body></html>";
            } else {
              htmlContent = "<html><head>"
                  "<meta charset='UTF-8'>"
                  "<meta http-equiv='refresh' content='3'>"
                  "</head><body>"
                  "<h1>Parkeringsplatser</h1>"
                  "<p>Max antal platser: " + String(maxPlatser) + "</p>"
                  "<p style='color:green;'>Lediga platser: " + String(ledigaPlatser) + "</p>"
                  "</body></html>";
            }
            client.print(htmlContent);
            
            
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