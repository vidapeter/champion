#include <TimerOne.h>
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

volatile int tick = 0;
volatile int timer = 0;

#if 1
#define DEVMODE
#endif

#define SENSOR1 3
#define SENSOR2 4
#define MAX_CYCLE 30*4

#ifdef DEVMODE
String startButton = "";
#endif

static int data[MAX_CYCLE-1]; //TODO: init
static int results[MAX_CYCLE-1];
int result;

bool timerAction = false;
bool isGameOn = false;
bool isGameOver = false;
bool isLeftAction = false;
bool isRightAction = false;

void systemTick(){
  if(timer < MAX_CYCLE){
    tick++;
    Timer1.restart();
    timerAction = true;
  }else{
    isGameOver = true;
  }

}

void rightAction(){
  isRightAction = true;
}

void leftAction(){
  isLeftAction = true;
  isGameOn = false; // ??
}

void setup() {
  //Timer setup, 250ms system tick;
  Timer1.initialize();
  Timer1.setPeriod(250000);
  Timer1.attachInterrupt(systemTick);

  pinMode(SENSOR1,INPUT);
  pinMode(SENSOR2,INPUT);

  attachPCINT(digitalPinToPCINT(SENSOR1), rightAction, RISING);
  attachPCINT(digitalPinToPCINT(SENSOR2), leftAction, RISING);

}

void loop() {
  if(isGameOn){

    if(timerAction){
      if(isRightAction ^ isLeftAction){
        if(isLeftAction){
          results[tick] = 1;
          isLeftAction = false;
        }
        if(isRightAction){
          results[tick] = 2;
          isLeftAction = false;
        }
      } else {results[tick] = 0;}
    }
  } else if (isGameOver) {
      isGameOver = false;
      result = calculateResult(data,results);
      #ifdef DEVMODE
      Serial.println("GAME OVER");
      Serial.println("Your result is: " + (String)(result));
      #endif
  } 
  #ifdef DEVMODE
  else {
    while (Serial.available() && isGameOn == false) {
       startButton = (char)Serial.read();
      if (startButton == "x") {
        result = 0;
        isGameOn = true;
        Serial.println("game started.");
      }
    }
  }
  #endif
  delay(5);
}

static int calculateResult(int data[], int results[]) {
  for (int i=0;i<MAX_CYCLE;i++) {

    switch (data[i]) {
      case 1:
        checkResult(1, i);
        break;
      case 2:
        checkResult(2, i);
        break;
      default:
        break;
    }
  }
}

void checkResult(int leg, int index) {
  if (results[index-1] == leg || results[index] == leg || results[index+1] == leg) {
    result++;
  }
}
