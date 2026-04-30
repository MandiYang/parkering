#include <U8g2lib.h>
#include <Servo.h>
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
const int seekPin1 = 5;
const int seekPin2 = 3;

const int maxPlatser = 6;
int ledigaPlatser = maxPlatser;
int direction;
int sensorIN;
int sensorOUT;
const int SERVO_PIN = 6;

const int OPEN_ANGLE = 0;
const int CLOSED_ANGLE = 90;
const int GATE_OPEN_TIME = 3000;
int GATE_TEMP_TIME = 0;
bool bomArOppen = false;
Servo bomServo;


void setup() {
  Serial.begin(9600);
  bomServo.attach(SERVO_PIN);
  pinMode(seekPin1, INPUT);
  pinMode(seekPin2, INPUT);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  bomServo.write(CLOSED_ANGLE);
  oledWrite(String(ledigaPlatser).c_str());
}

void loop() {
  sensorIN = digitalRead(seekPin1);
  sensorOUT = digitalRead(seekPin2);
  direction = checkMovement(sensorIN, sensorOUT);
  Serial.print(sensorIN);
  Serial.print("   ");
  Serial.print(sensorOUT);
  Serial.print("   ");
  Serial.print(direction);
  Serial.print("   ");
  Serial.println(ledigaPlatser);
  updateLedigaplatser();
  bomAction();
}

// Returnerar: 0 = Ingen rörelse, 1 = IN, 2 = UT
int checkMovement(int seekIN, int seekOUT) {
  static int state = 0;
  static unsigned long lastChange = 0;
  int result = 0;

  // SÄKERHET: Nollställ om sensorerna är blockerade för länge (t.ex. 2 sekunder)
  if (seekIN == LOW || seekOUT == LOW) {
    if (millis() - lastChange > 2000) {
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
    oledWrite(String(ledigaPlatser).c_str());
  } else if ((direction == 2) && (ledigaPlatser < maxPlatser)) {  // UT
    ledigaPlatser++;
    oledWrite(String(ledigaPlatser).c_str());
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