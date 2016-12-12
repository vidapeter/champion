#include <Wire.h>
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>

#define redPin 13
#define greenPin 12
#define buttonPin 8


bool interruptFlag = false;
int timerCounter = 0;
bool isGameOn = false;
bool isGameOver = false;
bool isLedOn = false;
bool gameTimeCounter = false;
bool waiting = false;
String startButton = "";
long start = 0;
long stop = 0;
long result = 0;
int roundCounter = 0;
long results[3];

void setup() {
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Reaction time-test, be ready!!");

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);


  attachPCINT(digitalPinToPCINT(buttonPin), buttonPushed, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(5000000);
  Timer1.attachInterrupt(timerHandler);
}

void loop() {
    if (isGameOn) {
      if (interruptFlag) {

        roundCounter++;

        stop = millis();
        digitalWrite(redPin, HIGH);
        digitalWrite(greenPin, LOW);
        interruptFlag = false;
        results[roundCounter-1] = stop - start;

        if (roundCounter < 3) {
          initiateLed();
        }
        if (roundCounter == 3) {
          roundCounter = 0;
          interruptFlag = false;
          isGameOver = true;
          isGameOn = false;
          interruptFlag = false;
        }
      }
    } else if (isGameOver) {
      isGameOver = false;
      Timer1.stop();
      Serial.println("GAME OVER");
      result = getMinResult(results);
      Serial.println("Your result is: " + (String)(float(result)/1000) + " sec");
    } else {
      while (Serial.available() && isGameOn == false) {
         startButton = (char)Serial.read();
         digitalWrite(redPin, HIGH);
        if  (startButton == "x") {
          isGameOn = true;
          Serial.println("game started.");
          initiateLed();
        }
      }
    }
    delay(5);
}

void buttonPushed() {
  if (digitalRead(greenPin) == HIGH) { //only after the led is on..
    interruptFlag = true;
  }
}

void initiateLed() {
  delay(random(2000,5000));
  digitalWrite(greenPin, HIGH);
  digitalWrite(redPin, LOW);
  start = millis();
}

void timerHandler() {
  timerCounter++;
  if (timerCounter == 6) {
    isGameOver = true;
    isGameOn = false;
    timerCounter = 0;
    digitalWrite(redPin,HIGH);
    digitalWrite(greenPin,LOW);
  } else {
    isGameOver = false;
  }
}

long getMinResult(long results[]) {
  long min = results[0];
  for (int i=0;i<3;i++) {
    if (min > results[i]) {
      min = results[i];
    }
  }
  return min;
}
