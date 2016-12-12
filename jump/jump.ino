#include <Wire.h>
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>

#define greenPin 12
#define rightButtonPin 8
#define leftButtonPin 7


bool interruptFlag = false;
int timerCounter = 0;
bool isGameOn = false;
bool isGameOver = false;
bool rightButtonFlag = false;
bool leftButtonFlag = false;

String startButton = "";
long start = 0;
long stop = 0;
long result = 0;
int roundCounter = 0;

void setup() {
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Attention! The game will start after you step into the white circles.");

  pinMode(leftButtonPin, INPUT_PULLUP);
  pinMode(rightButtonPin, INPUT_PULLUP);

  attachPCINT(digitalPinToPCINT(rightButtonPin), rightButtonPushed, FALLING);
  attachPCINT(digitalPinToPCINT(leftButtonPin), leftButtonPushed, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(5000000);
  Timer1.attachInterrupt(timerHandler);
}

void loop() {
    if (isGameOn) {

      if (rightButtonFlag || leftButtonFlag) {
        digitalWrite(greenPin, LOW);
        stop = millis();
        isGameOver = true;
        isGameOn = false;
        rightButtonFlag = false;
        leftButtonFlag = false;
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
          if (leftButtonFlag && rightButtonFlag) {
            digitalWrite(greenPin, HIGH);
            start = millis();
            isGameOn = true;
            // green led would be useful.
            leftButtonFlag = false;
            rightButtonFlag = false;
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
  } else {
    isGameOver = false;
  }
}

void rightButtonPushed() {
  rightButtonFlag = true;
}

void leftButtonPushed() {
  leftButtonFlag = true;
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
