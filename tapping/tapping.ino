#include <Wire.h>
#include "Adafruit_MPR121.h"
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>
#include "MsTimer2.h"

#define rightLED 13
#define middLED 12
#define leftLED 11

#if 0
#define DEVMODE
#endif


Adafruit_MPR121 cap = Adafruit_MPR121();

// Right: PIN1 -> 2
// Middle: PIN3 -> 8
// Left: PIN5 -> 32

volatile int counter = 0;
int timer_counter = 0;
bool touchedFlag = false;
bool isGameOn = false;
bool isGameOver = false;
#ifdef DEVMODE
String valami = "alma";
String start = "";
#endif
uint8_t electrodeRegister;

uint8_t lastTouched;

void flash() {
 digitalWrite(rightLED, LOW);
 digitalWrite(leftLED, LOW);
}


void setup() {
  #ifdef DEVMODE
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Adafruit MPR121 Capacitive Touch sensor test");
  #endif

  pinMode(leftLED,OUTPUT);
  pinMode(middLED,OUTPUT);
  pinMode(rightLED,OUTPUT);

  if (!cap.begin(0x5A)) {
    #ifdef DEVMODE
    Serial.println("MPR121 not found, check wiring?");
    while (1);
    #endif
  }
  #ifdef DEVMODE
  Serial.println("MPR121 found!");
  #endif

  pinMode(8, INPUT_PULLUP);

  attachPCINT(digitalPinToPCINT(8), touched, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(1000000); // 1 mp
  Timer1.attachInterrupt(timerHandler);

  MsTimer2::set(50,flash);

}

void loop() {
    if (isGameOn) {
      if (touchedFlag == true) {
        electrodeRegister = cap.readRegister8(0x00);

        (electrodeRegister >= 2 && electrodeRegister != 4) ? digitalWrite(middLED, HIGH) : digitalWrite(middLED, LOW);

        uint8_t touchedElectrode = electrodeRegister - 2;

        if ((touchedElectrode == 4 || touchedElectrode == 1) && lastTouched != electrodeRegister) {
          MsTimer2::stop();
          if (touchedElectrode == 4) {
            digitalWrite(leftLED,HIGH);
            MsTimer2::start();
          } else if (touchedElectrode == 1) {
            digitalWrite(rightLED,HIGH);
            MsTimer2::start();
          }
          counter++;
          lastTouched = electrodeRegister;
          Serial.println(counter);
        }

      }
      delay(50);
    } else if (isGameOver) {
      digitalWrite(middLED, LOW);
      isGameOn = false;
      isGameOver = false;
      #ifdef DEVMODE
      Serial.println("GAME OVER");
      Serial.println("Your result is: " + (String)(counter));
      #endif
      counter = 0;
    }
    #ifdef DEVMODE
    else {
      while (Serial.available() && isGameOn == false) {
        start = (char)Serial.read();
        if  (start == "x") {
          isGameOn = true;
          Serial.println("game started.");
          Timer1.start();
        }
      }
    }
    #endif
}


void touched() {
  touchedFlag = true;
}


void timerHandler() {
  timer_counter++;
  if (timer_counter == 10) {
    if (isGameOn) { isGameOver = true; }
    isGameOn = false;
    timer_counter = 0;
    Timer1.stop();
  }
}
