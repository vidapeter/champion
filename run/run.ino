
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>

#define ledPin 13
#define startPin 8
#define finishPin 7


int timerCounter = 0;
bool isGameOn = false;
bool isGameOver = false;
bool startFlag = false;
bool finishFlag = false;

String startButton = "";
long start = 0;
long stop = 0;
uint16_t result = 0;
//#define DEVMODE

void setup() {
  #if defined(DEVMODE)
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Run Forest, run!!");
  #endif

  pinMode(startPin, INPUT_PULLUP);
  pinMode(finishPin, INPUT_PULLUP);

  attachPCINT(digitalPinToPCINT(startPin), runStarted, FALLING);
  attachPCINT(digitalPinToPCINT(finishPin), runFinished, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(5000000);
  Timer1.attachInterrupt(timerHandler);
}

void loop() {
    if (isGameOn) {

      if (finishFlag) {

        stop = millis();
        finishFlag = false;
        isGameOver = true;
        isGameOn = false;

      }
    } else if (isGameOver) {
      isGameOver = false;
      Timer1.stop();
      Serial.println("GAME OVER");
      result = stop - start;
      Serial.println("Your result is: " + (String)(float(result)/1000));
    } else {
      while (Serial.available() && isGameOn == false) {
         startButton = (char)Serial.read();
        if  (startButton == "x") {
          if (startFlag) {
            start = millis();
            isGameOn = true;
            // green led would be useful.
            Serial.println("game started.");
          }
        }
      }
    }
    delay(5);
}

void timerHandler() {
  timerCounter++;
  if (timerCounter == 6) {
    isGameOver = true;
    isGameOn = false;
    timerCounter = 0;
    digitalWrite(ledPin,LOW);
  } else {
    isGameOver = false;
  }
}

void runStarted() {
  startFlag = true;
}

void runFinished() {
  finishFlag = true;
  startFlag = false;
  isGameOver = true;
}

// some count down logic for more UX
// void countDown(int round) {
//   for (int i=1; i <= round;i++) {
//     if (i == 3) {
//       Serial.println("JUMP!")
//     } else {
//       Serial.println((string)(i)+ "..")
//       delay(1000);
//     }
//   }
// }
