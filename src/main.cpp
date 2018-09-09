#include <Arduino.h>
#include <Nextion.h>
#define nextion Serial2

Nextion myNextion(nextion, 9600);

const int interlockInput = 3;
const int interlockReset = 2;
const int hopperLimitSwitch = 18;
// const int rpm0In = 19;
const int rpm1In = 20;
const int rpm2In = 21;

// relays
const int relay1 = 22;
const int relay2 = 23;
const int relaySmallReelUp = 24;
const int relaySmallReelDn = 25;
const int relayBigReelUp = 26;
const int relayBigReelDn = 27;
const int relayBoomUp = 28;
const int relayBoomDn = 29;

//values
float vaRate = 136;    // Flow rate in lpm
int vaDelay = 15;     // Hopper delay timer
int vaRpm = 180;     // RPM reference
volatile float kgprev;
volatile float currVal;
volatile float vaTotal;

int targetVal;
int curr;
int pumpCount = 0;

unsigned long startMillis;
unsigned long currentMillis;
unsigned long hopperStartMillis;

bool auto1RequestStatus = false;
bool auto2RequestStatus = false;
bool hopperFillAllowed = true;
bool hopperFlag = true;

bool charging = false;
bool manualCharging = false;
bool counting = false;
bool autoChargingComplete = false;
bool interlockFlag = false;

void getVariables() {
  vaRate = myNextion.getComponentValue("va0");
  vaDelay = myNextion.getComponentValue("va1");
  vaTotal = myNextion.getComponentValue("n3");
  vaRpm = myNextion.getComponentValue("va2");
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

void startAuto1() {
  getVariables();
  Serial.print("Auto1 Start. Code: ");
  Serial.print("Hopper Delay: ");
  Serial.println(vaDelay);
  auto1RequestStatus = !auto1RequestStatus;
}

void in1ISR() {
  Serial.println("Interupsi 1");
}

void chargeCount() {
  if (manualCharging) {
    curr++;
    currVal += kgprev;
    vaTotal += kgprev;
    Serial.print("counting: ");
    Serial.print("Current: ");
    Serial.println(currVal);
    Serial.print("Total: ");
    Serial.println(vaTotal);
    counting = true;
    // update value ke layar, if counting yg menjalankan
  } else {
    if (curr < pumpCount ) {
      curr++;
      vaTotal += kgprev;
      currVal += kgprev;
      Serial.print(curr);
      Serial.print(" / ");
      Serial.println(pumpCount);
      Serial.print("Current: ");
      Serial.println(currVal);
      Serial.print("Target: ");
      Serial.println(targetVal);
      counting = true;
    } else {
      detachInterrupt(3);
      autoChargingComplete = true;
      auto2RequestStatus = false;
    }
  }

}

void setup() {
    // put your setup code here, to run once:
    pinMode(relay1, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(relaySmallReelUp, OUTPUT);
    pinMode(relaySmallReelDn, OUTPUT);
    pinMode(relayBigReelUp, OUTPUT);
    pinMode(relayBigReelDn, OUTPUT);
    pinMode(relayBoomUp, OUTPUT);
    pinMode(relayBoomDn, OUTPUT);

    pinMode(interlockInput, INPUT);
    pinMode(interlockReset, INPUT);
    pinMode(rpm1In, INPUT);
    pinMode(rpm2In, INPUT);
    pinMode(hopperLimitSwitch, INPUT);

    digitalWrite(relay1, LOW);
    digitalWrite(relay2, LOW);
    digitalWrite(relaySmallReelUp, LOW);
    digitalWrite(relaySmallReelDn, LOW);
    digitalWrite(relayBigReelUp, LOW);
    digitalWrite(relayBigReelDn, LOW);
    digitalWrite(relayBoomUp, LOW);
    digitalWrite(relayBoomDn, LOW);

    Serial.begin(9600);
    myNextion.init();
}

void loop() {
    // put your main code here, to run repeatedly:
    String message = myNextion.listen();

    if (message == "65 0 2 1 ffff ffff ffff") {
        Serial.println(message);
        startAuto1();
    }

    if (message == "65 0 3 1 ffff ffff ffff") {
        Serial.print("Auto2 Start. Code: ");
        Serial.println(message);
        auto2RequestStatus = !auto2RequestStatus;
        targetVal = myNextion.getComponentValue("n1");
        curr = myNextion.getComponentValue("n2");
        attachInterrupt(digitalPinToInterrupt(rpm1In), chargeCount, FALLING);
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

    if (auto1RequestStatus && hopperFillAllowed) {
      if(!digitalRead(hopperLimitSwitch)) {
        digitalWrite(relay1, HIGH);
      } else if (digitalRead(hopperLimitSwitch)) {
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

    if (!auto1RequestStatus) {
      digitalWrite(relay1, LOW);
    }

    currentMillis = millis();
    allowHopperFill();

    if (counting) {
      int roundedCurrVal = ceil(currVal);
      int roundedTotalVal = ceil(vaTotal);
      myNextion.setComponentValue("n2", roundedCurrVal);
      myNextion.setComponentValue("n3", roundedTotalVal);
      counting = false;
    }

    if (autoChargingComplete) {
      myNextion.sendCommand("click bt1,1");
      digitalWrite(relay2, HIGH);
      autoChargingComplete = false;
    }

    // interlock function
    if (digitalRead(interlockInput)) {
      interlockFlag = true;
    }

    if (interlockFlag) {
      digitalWrite(relay1, LOW);
      digitalWrite(relay2, LOW);
    }

    if (!digitalRead(interlockReset)) {
      interlockFlag = false;
    }
}
