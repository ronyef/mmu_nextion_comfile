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
float vaRate = 136;    // Flow rate in lpm
int vaDelay = 15;     // Hopper delay timer
int vaRpm = 180;     // RPM reference
float kgprev;
float currVal;
float vaTotal;


bool productPumpInStatus = false;  //deteksi putaran pompa
int pumpCount = 0;
bool countEnable = false;

unsigned long count = 10000;
bool blink = false;
bool writeFlag = true;
bool turnOffFlag = true;
bool hopperFlag = true;

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
    if (hopperFlag) {
      hopperFillAllowed = true;
    } else {
      hopperFillAllowed = false;
    }
  }
}

void getVariables() {
  vaRate = myNextion.getComponentValue("va0");
  vaDelay = myNextion.getComponentValue("va1");
  vaTotal = myNextion.getComponentValue("n3");
  vaRpm = myNextion.getComponentValue("va2");
}

void charge() {
  if (charging) {
    if (curr < pumpCount) {
      turnOffFlag = true;
      if (digitalRead(productRpmIn)  && countEnable) {
        curr++;
        vaTotal += kgprev;
        currVal += kgprev;
        myNextion.setComponentValue("n2", currVal);
        Serial.print("Current: ");
        Serial.println(currVal);
        Serial.println("total smntr");
        myNextion.setComponentValue("n3", vaTotal);
        countEnable = false;
      }
      else if (!digitalRead(productRpmIn)) {
        countEnable = true;
      }
    } else {
      charging = false;
      curr = 0;
      // currVal = 0;
      if (turnOffFlag) {
        float dev = targetVal - currVal;
        Serial.print("Deviasi: ");
        Serial.println(dev);
        if(dev > 0) {
          vaTotal += dev;
          // currVal = 0;
          Serial.print("Total: ");
          Serial.println(vaTotal);
          myNextion.setComponentValue("n2", 0);
          myNextion.setComponentValue("n3", vaTotal);
          Serial.println("total akhir");
        }
        delay(1000);
        myNextion.sendCommand("click bt1,1");
        digitalWrite(relay2, LOW);
        auto2RequestStatus = false;
        turnOffFlag = false;

      }
    }
  }
  if (manualCharging) {
    // Serial.println("Manual Charging");
    if (digitalRead(productRpmIn)  && countEnable) {
      Serial.print("counting: ");
      curr++;
      currVal += kgprev;
      vaTotal += kgprev;
      Serial.print("Current: ");
      Serial.println(currVal);
      Serial.print("Total: ");
      Serial.println(vaTotal);
      myNextion.setComponentValue("n2", currVal);
      myNextion.setComponentValue("n3", vaTotal);
      countEnable = false;
    }

    if (!digitalRead(productRpmIn)) {
      countEnable = true;
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

    // if (digitalRead(auto1In)) {
    //     auto1RequestStatus = !auto1RequestStatus;
    // }
    //
    // if (digitalRead(auto2In)) {
    //     auto2RequestStatus = !auto2RequestStatus;
    // }

    if (message == "65 0 2 1 ffff ffff ffff" || digitalRead(auto1In)) {
        getVariables();
        Serial.print("Auto1 Start. Code: ");
        Serial.println(message);
        Serial.print("Hopper Delay: ");
        Serial.println(vaDelay);
        auto1RequestStatus = !auto1RequestStatus;
    }

    if (message == "65 0 3 1 ffff ffff ffff" || digitalRead(auto2In)) {
        Serial.print("Auto2 Start. Code: ");
        Serial.println(message);
        auto2RequestStatus = !auto2RequestStatus;
        targetVal = myNextion.getComponentValue("n1");
        curr = myNextion.getComponentValue("n2");
        if (targetVal > 0 && auto2RequestStatus) {
          getVariables();
          kgprev = vaRate/vaRpm;
          pumpCount = targetVal / kgprev;
          Serial.print("Target: ");
          Serial.print(targetVal);
          Serial.print(", Current: ");
          Serial.println(curr);
          Serial.print("Flowrate: ");
          Serial.println(vaRate);
          Serial.print("@RPM: ");
          Serial.println(vaRpm);
          Serial.print("Kg/Rev: ");
          Serial.println(kgprev);
          Serial.print("Hopper Delay: ");
          Serial.println(vaDelay);
          Serial.print("num Rotation: ");
          Serial.println(pumpCount);
          Serial.println("Start charging...");
          Serial.println("");
          curr = 0;
          currVal = 0;
          manualCharging = false;
          digitalWrite(relay2, HIGH);
          charging = true;
          manualCharging = false;
          // auto2StartMillis = currentMillis;
        } else if (targetVal <= 0 && auto2RequestStatus){
          getVariables();
          kgprev = vaRate/vaRpm;
          curr = 0;
          currVal = 0;
          manualCharging = true;
          charging = false;
          //chargingPeriod = 60;
          digitalWrite(relay2, HIGH);
          Serial.print("Manual Charging: ");
          Serial.println(manualCharging);
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

    // if (auto1RequestStatus) {
    //   if (!digitalRead(hopperLimitSwitch)) {
    //     digitalWrite(relay1, LOW);
    //     hopperStartMillis = millis();
    //   } else if (digitalRead(hopperLimitSwitch) && hopperFillAllowed) {
    //     digitalWrite(relay1, HIGH);
    //   }
    // } else {
    //   digitalWrite(relay1, LOW);
    // }

    if (auto1RequestStatus && hopperFillAllowed) {
      if(digitalRead(hopperLimitSwitch)) {
        digitalWrite(relay1, HIGH);
      } else if (!digitalRead(hopperLimitSwitch)) {
        digitalWrite(relay1, LOW);
        hopperFillAllowed = false;
        hopperFlag = false;
        hopperStartMillis = millis();
      }
    }

    if (!auto2RequestStatus) {
      digitalWrite(relay2, LOW);
      charging = false;
      manualCharging = false;
    }

    //dummy
    if (!auto1RequestStatus) {
      digitalWrite(relay1, LOW);
    }

    currentMillis = millis();
    tick();
    allowHopperFill();
    charge();

    // Serial.println(hopperFillAllowed);
}
