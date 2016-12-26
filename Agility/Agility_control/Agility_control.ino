#include <TimerOne.h>
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

volatile int tick = 0;

#define SENSOR1 3
#define SENSOR2 4
#define MAX_CYCLE 32

#define rightLED 5
#define leftLED 6

String startButton = "";

static int data[MAX_CYCLE] = {0, 1,1,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int results[MAX_CYCLE-1];
int result;

bool timerAction = false;
bool isGameOn = false;
bool isGameOver = false;
bool isLeftAction = false;
bool isRightAction = false;

void systemTick(){
  if(tick < MAX_CYCLE){
    tick++;
    Serial.println(tick);
    timerAction = true;
  }else{
    isGameOn = false;
    isGameOver = true;
    timerAction = false;
    tick = 0;
  }

}

void rightAction(){
  isRightAction = true;
}

void leftAction(){
  isLeftAction = true;
}

void setup() {
  Serial.begin(9600);
  //Timer setup, 250ms system tick;
  Timer1.initialize(250000);
  Timer1.attachInterrupt(systemTick);
  Timer1.stop();

  pinMode(SENSOR1,INPUT_PULLUP);
  pinMode(SENSOR2,INPUT_PULLUP);

  pinMode(rightLED, OUTPUT);
  pinMode(leftLED, OUTPUT);

  attachPCINT(digitalPinToPCINT(SENSOR1), rightAction, FALLING);
  attachPCINT(digitalPinToPCINT(SENSOR2), leftAction, FALLING);
  Serial.println("setup finished!");
}

void loop() {
  if(isGameOn){

    if(timerAction){
      if(isRightAction ^ isLeftAction){
        Serial.println("INT");
        if(isLeftAction){
          results[tick] = 1;
          isLeftAction = false;
          digitalWrite(leftLED, HIGH);
        }
        if(isRightAction){
          results[tick] = 2;
           isRightAction = false;
          digitalWrite(rightLED, HIGH);
        }
      } else {
        results[tick] = 0;
        digitalWrite(leftLED, LOW);
        digitalWrite(rightLED, LOW);
        isRightAction = false;
        isLeftAction = false;
      }
      timerAction = false;
    }
  } else if (isGameOver) {
      Timer1.stop();
      isGameOver = false;
      Serial.print("Data: ");
      for(int i = 0;i<MAX_CYCLE;i++){
        Serial.print(data[i],DEC);
        Serial.print(" ");
      }
      Serial.println("");
      Serial.print("Res : ");
     for(int i = 0;i<MAX_CYCLE;i++){
        Serial.print(results[i],DEC);
        Serial.print(" ");
      }
   
      Serial.println("GAME OVER");
      result = calculateResult(data,results);
      Serial.println("Your result is: " + (String)(result));
   
      
  } else {
    while (Serial.available() && isGameOn == false) {
       startButton = (char)Serial.read();
      if (startButton == "x") {
        Timer1.start();
        result = 0;
        isGameOn = true;
        Serial.println("game started.");
      }
    }
  }
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
  return result;
}

void checkResult(int leg, int index) {
  if (results[index-1] == leg || results[index] == leg || results[index+1] == leg) {
    result++;
  }
}
