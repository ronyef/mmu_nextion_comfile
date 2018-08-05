#include <Arduino.h>
#include <Nextion.h>

#define nextion Serial2

Nextion myNextion(nextion, 9600);

const int auto1In = 30;
const int auto2In = 31;
const int productRpmIn = 32;
const int pumpARpmIn = 33;
const int pumpBRpmIn = 34;
const int hopperLimitSwitch = 35;
const int relay1 = 22;
const int relay2 = 23;
const int relaySmallReelUp = 24;
const int relaySmallReelDn = 25;
const int relayBigReelUp = 26;
const int relayBigReelDn = 27;
const int relayBoomUp = 28;
const int relayBoomDn = 29;
bool auto1PBState = false;
bool auto2PBState = false;
bool auto1RequestStatus = false;
bool auto2RequestStatus = false;
bool hopperFillAllowed = true;

unsigned long startMillis;
unsigned long currentMillis;
unsigned long hopperStartMillis;
const unsigned long period = 500;
const unsigned long hopperPeriod = 15000;
// Auto2 timer
unsigned long auto2StartMillis;
unsigned long chargingPeriod;
int targetVal;
int curr;
bool charging = false;
bool manualCharging = false;

const int flowRate = 4;   //240 l/m

int auto1Val;
int auto2Val;
int vaRate = 240;    // Flow rate in lpm
int vaDelay = 15;     // Hopper delay timer
int vaTotal;

bool productPumpInStatus = false;  //deteksi putaran pompa

unsigned long count = 10000;
bool blink = false;
bool writeFlag = true;
bool turnOffFlag = true;

void tick() {
  if (currentMillis - startMillis >= period) {
    blink = !blink;
    // currentMillis = startMillis;    //ini yang kebalik
    startMillis = currentMillis;
  }
}

void allowHopperFill() {
  unsigned long delay = vaDelay * 1000;
  if (currentMillis - hopperStartMillis >= delay) {
    hopperFillAllowed = true;
    currentMillis = hopperStartMillis;
  } else {
    hopperFillAllowed = false;
  }
}

void getVariables() {
  vaRate = myNextion.getComponentValue("va0");
  vaDelay = myNextion.getComponentValue("va1");
  vaTotal = myNextion.getComponentValue("n3");
}

void charge() {
  if (charging) {

    // Counter berdasarkan RPM
    if (digitalRead(productRpmIn)) {
      productPumpInStatus = !productPumpInStatus;
      curr ++;
    }

    if(chargingPeriod > 0) {
      turnOffFlag = true;
      if (blink && writeFlag) {
        // increment counter + total dan tampilkan
        int currVal = curr += vaRate / 60;
        vaTotal += vaRate / 60;
        myNextion.setComponentValue("n2", currVal);
        myNextion.setComponentValue("n3", vaTotal);
        if (!manualCharging) {
          chargingPeriod --;
        }
        writeFlag = false;
      } else if (!blink) {
        writeFlag = true;
      }
    } else {
      charging = false;
      auto1RequestStatus = false;
      if (turnOffFlag) {
        myNextion.sendCommand("click bt1,1");
        digitalWrite(relay2, LOW);
        auto2RequestStatus = false;
        turnOffFlag = false;
      }

    }
  }
}

void setup() {
    pinMode(auto1In, INPUT);
    pinMode(auto2In, INPUT);
    pinMode(productRpmIn, INPUT);
    pinMode(pumpARpmIn, INPUT);
    pinMode(pumpBRpmIn, INPUT);
    pinMode(relay1, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(relaySmallReelUp, OUTPUT);
    pinMode(relaySmallReelDn, OUTPUT);
    pinMode(relayBigReelUp, OUTPUT);
    pinMode(relayBigReelDn, OUTPUT);
    pinMode(relayBoomUp, OUTPUT);
    pinMode(relayBoomDn, OUTPUT);

    Serial.begin(9600);
    myNextion.init();

    startMillis = millis();
    getVariables();
}

void loop() {
    String message = myNextion.listen();

    if (digitalRead(auto1In)) {
        auto1RequestStatus = !auto1RequestStatus;
    }

    if (digitalRead(auto2In)) {
        auto2RequestStatus = !auto2RequestStatus;
    }

    if (message == "65 0 2 1 ffff ffff ffff") {
        getVariables();
        Serial.print("Auto1 Start. Code: ");
        Serial.println(message);
        Serial.print("Hopper Delay: ");
        Serial.println(vaDelay);
        auto1RequestStatus = !auto1RequestStatus;
    }

    if (message == "65 0 3 1 ffff ffff ffff") {
        Serial.print("Auto2 Start. Code: ");
        Serial.println(message);
        auto2RequestStatus = !auto2RequestStatus;
        targetVal = myNextion.getComponentValue("n1");
        curr = myNextion.getComponentValue("n2");
        if (targetVal > 0 && auto2RequestStatus) {
          getVariables();
          unsigned long period = (targetVal - curr) / (vaRate / 60);
          chargingPeriod = period ;
          Serial.print("Target: ");
          Serial.print(targetVal);
          Serial.print(", Current: ");
          Serial.print(curr);
          Serial.print(", Rate: ");
          Serial.println(vaRate);
          Serial.print(", Delay: ");
          Serial.print(vaDelay);
          Serial.print(", Period: ");
          Serial.println(chargingPeriod);
          Serial.println("Start charging...");
          manualCharging = false;
          digitalWrite(relay2, HIGH);
          charging = true;
          auto2StartMillis = currentMillis;
        } else if (targetVal <= 0 && auto2RequestStatus){
          getVariables();
          manualCharging = true;
          charging = true;
          chargingPeriod = 60;
          digitalWrite(relay2, HIGH);
          Serial.print("Charging Period: ");
          Serial.println(chargingPeriod);
          Serial.print("Charging: ");
          Serial.println(charging);
        }

    }

    if (message == "65 0 5 1 ffff ffff ffff") {
        Serial.print("Big Reel Forward. Code: ");
        Serial.println(message);
        digitalWrite(relayBigReelUp, HIGH);
    }

    if (message == "65 0 5 0 ffff ffff ffff") {
        Serial.print("Big Reel Stop. Code: ");
        Serial.println(message);
        digitalWrite(relayBigReelUp, LOW);
    }

    if (message == "65 0 6 1 ffff ffff ffff") {
        Serial.print("Big Reel Reverse. Code: ");
        Serial.println(message);
        digitalWrite(relayBigReelDn, HIGH);
    }

    if (message == "65 0 6 0 ffff ffff ffff") {
        Serial.print("Big Reel Stop. Code: ");
        Serial.println(message);
        digitalWrite(relayBigReelDn, LOW);
    }

    if (message == "65 0 7 1 ffff ffff ffff") {
        Serial.print("Small Reel Forward. Code: ");
        Serial.println(message);
        digitalWrite(relaySmallReelUp, HIGH);
    }

    if (message == "65 0 7 0 ffff ffff ffff") {
        Serial.print("Small Reel Stop. Code: ");
        Serial.println(message);
        digitalWrite(relaySmallReelUp, LOW);
    }

    if (message == "65 0 8 1 ffff ffff ffff") {
        Serial.print("Small Reel Reverse. Code: ");
        Serial.println(message);
        digitalWrite(relaySmallReelDn, HIGH);
    }

    if (message == "65 0 8 0 ffff ffff ffff") {
        Serial.print("Small Reel Stop. Code: ");
        Serial.println(message);
        digitalWrite(relaySmallReelDn, LOW);
    }

    if (message == "65 0 1 1 ffff ffff ffff") {
        Serial.print("Boom Forward. Code: ");
        Serial.println(message);
        digitalWrite(relayBoomUp, HIGH);
    }

    if (message == "65 0 1 0 ffff ffff ffff") {
        Serial.print("Boom Stop. Code: ");
        Serial.println(message);
        digitalWrite(relayBoomUp, LOW);
    }

    if (message == "65 0 4 1 ffff ffff ffff") {
        Serial.print("Boom Reverse. Code: ");
        Serial.println(message);
        digitalWrite(relayBoomDn, HIGH);
    }

    if (message == "65 0 4 0 ffff ffff ffff") {
        Serial.print("Boom Stop. Code: ");
        Serial.println(message);
        digitalWrite(relayBoomDn, LOW);
    }

    if (auto1RequestStatus) {
      if (!digitalRead(hopperLimitSwitch)) {
        digitalWrite(relay1, LOW);
        hopperStartMillis = millis();
      } else if (digitalRead(hopperLimitSwitch) && hopperFillAllowed) {
        digitalWrite(relay1, HIGH);
      }
    } else {
      digitalWrite(relay1, LOW);
    }

    if (!auto2RequestStatus) {
      digitalWrite(relay2, LOW);
      charging = false;
    }

    //dummy
    if (!auto1RequestStatus) {
      digitalWrite(relay1, LOW);
    }

    currentMillis = millis();
    tick();
    allowHopperFill();
    charge();

    // Serial.println(charging);
}
