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

static int data[MAX_CYCLE-1]; //TODO: init
static int result[MAX_CYCLE-1];

bool timerAction = false;
bool isGameStarted = false;
bool isGameOver = false;
bool isLeftAction = false;
bool isRightAction = false;

void system_tick(){
  if(timer < MAX_CYCLE){
    tick++;
    Timer1.restart();
    timerAction = true;
  }else{
    isGameOver = true;
  }
  
}

void right_action(){
  isRightAction = true;
}

void left_action(){
  isLeftAction = true;
}

void setup() {
  //Timer setup, 250ms system tick;
  Timer1.initialize();
  Timer1.setPeriod(250000);
  Timer1.attachInterrupt(system_tick);

  pinMode(SENSOR1,INPUT);
  pinMode(SENSOR2,INPUT);

  attachPCINT(digitalPinToPCINT(SENSOR1), right_action, RISING);
  attachPCINT(digitalPinToPCINT(SENSOR2), left_action, RISING);

}

void loop() {
  if(isGameStarted){


    if(timerAction){
      if(isRightAction ^ isLeftAction){
        if(isLeftAction){
          data[tick] = 1;
          isLeftAction = false;
        }
        if(isRightAction){
          data[tick] = 2;
          isLeftAction = false;
        }
      }else{data[tick] = 0;}
    }
  
    
  }

}
