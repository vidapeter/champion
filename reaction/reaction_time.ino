#include <Wire.h>
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>

bool interruptFlag = false;
int timerCounter = 0;
bool isGameOn = false;
bool isGameOver = false;
bool isLedOn = false;
bool gameTimeCounter = false;
bool waiting = false;
String startButton = "";
float start = 0;
float stop = 0;
float result = 0;

void timerHandler() {
  timerCounter++;
  Serial.println(timerCounter);
  if (timerCounter == 2) {
    isGameOver = true;
    isGameOn = false;
    timerCounter = 0;
  } else {
    isGameOver = false;
  }
}

void setup() {
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Reaction time-test, be ready!!");

  pinMode(8, INPUT_PULLUP);
  pinMode(13, OUTPUT);


  attachPCINT(digitalPinToPCINT(8), buttonPushed, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(5000000); // 5 mp
  Timer1.attachInterrupt(timerHandler);
}

void loop() {
    if (isGameOn) {

      if (interruptFlag) {
        stop = millis();
        digitalWrite(13, LOW);
        isGameOn = false;
        isGameOver = true;
        interruptFlag = false;
      }

    } else if (isGameOver) {
      isGameOver = false;
      Timer1.stop();
      Serial.println("GAME OVER");
      result = stop - start;
      Serial.println("Your result is: " + (String)(result/1000) + " sec");
    } else {
      while (Serial.available() && isGameOn == false) {
         startButton = (char)Serial.read();
        if  (startButton == "x") {
          isGameOn = true;
          waiting = true;
          Serial.println("game started.");
          delay(random(2000,5000)); 
          digitalWrite(13, HIGH);
          start = millis();
        }
      }
    }
    delay(50);
}

void buttonPushed() {
  interruptFlag = true;
}
