#include <Wire.h>
#include "Adafruit_MPR121.h"
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <TimerOne.h>


Adafruit_MPR121 cap = Adafruit_MPR121();

// Right: PIN1 -> 2
// Middle: PIN3 -> 8
// Left: PIN5 -> 32

volatile int counter = 0;
int timer_counter = 0;
bool interruptFlag = false;
bool isGameOn = false;
bool isGameOver = false;
String start = "";
uint8_t electrodeRegister;

uint8_t lastTouched;

void setup() {
  while (!Serial);
  Serial.begin(9600);
  Serial.println("Adafruit MPR121 Capacitive Touch sensor test");

  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  pinMode(8, INPUT_PULLUP);

  attachPCINT(digitalPinToPCINT(8), interruptHappened, FALLING);

  Timer1.initialize();
  Timer1.setPeriod(1000000); // 1 mp
  Timer1.attachInterrupt(timerHandler);

}

void loop() {
    if (isGameOn) {
	timer_counter = 0; // figyelni kell arra, hogy itt újra, és újra elinduló játék van, mindent alaphelyzetbe kell állítani. 
      if (interruptFlag == true) {
        electrodeRegister = cap.readRegister8(0x00);
		
		// jó lenne, ha ezeket a számokat maszkokká alakítanád, hogy akkor is működjön a kód, ha véletlenül a többit megnyomják. (de ez nem prioritás)
		// lesz még itt LED kezelés is, arról később. 
        uint8_t touchedElectrode = electrodeRegister - 8;

        if ((touchedElectrode == 32 || touchedElectrode == 2) && lastTouched != electrodeRegister) {
          counter++;
          lastTouched = electrodeRegister;
          Serial.println(counter);
        }
		interruptFlag = false; // ez fontos, különben örökké belépsz ebbe az ágba

      }
      delay(50);
    } else if (isGameOver) {
      isGameOn = false;
      isGameOver = false;
      Serial.println("GAME OVER");
      Serial.println("Your result is: " + (String)(counter));
      counter = 0;
    } else {
      while (Serial.available() && isGameOn == false) {
        start = (char)Serial.read(); // ha működik a cucc, akkor majd mutatom a netes sémát, és átalakítjuk. 
        if  (start == "x") {
          isGameOn = true;
          Serial.println("game started.");
          Timer1.start();
        }
      }
    }
}

void interruptHappened() {
  interruptFlag = true;
}

void timerHandler() {
if(isGameOn){ // ha csak akkor számolsz, ha játék van, akkor, ha eléred a tizet, mindenképp vége a játéknak
  timer_counter++; // ez az interrupt rutin eléggé káoszos :D 
  if (timer_counter == 10) {
  isGameOver = true; 
   // Timer1.stop(); // nem kell megállítani, majd ha kezdődik a játék, indítod előröl 
  }
}
}
