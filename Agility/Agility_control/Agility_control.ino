#include <TimerOne.h>
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

volatile int tick = 0;
volatile int timer = 0;

#define SENSOR1 3
#define SENSOR2 4
#define MAX_CYCLE 30*4

String startButton = "";

static int data[MAX_CYCLE-1]; //TODO: init
static int result[MAX_CYCLE-1];
int faults;

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
  isGameOn = false;
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
          result[tick] = 1;
          isLeftAction = false;
        }
        if(isRightAction){
          result[tick] = 2;
          isLeftAction = false;
        }
      } else {result[tick] = 0;}
    }
  } else if (isGameOver) {
      isGameOver = false;
      Serial.println("GAME OVER");
      faults = checkFaults(data,result);
      Serial.println("You had " + (String)(faults) + " faults.");
  } else {
    while (Serial.available() && isGameOn == false) {
       startButton = (char)Serial.read();
      if (startButton == "x") {
        isGameOn = true;
        Serial.println("game started.");
      }
    }
  }
  delay(5);
}

int checkFaults(int data[], int result[]) {
  int faults = 0;
  for (int i=0;i<MAX_CYCLE;i++) {
    if (data[i] != result[i]) {
      faults++;
    }
  }
  return faults;
}
