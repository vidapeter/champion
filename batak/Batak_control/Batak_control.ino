#include <TimerOne.h>
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

#if 1
#define DEVMODE
#endif

#define X0 2
#define X1 3
#define X2 4
#define X3 5

#define Y0 6
#define Y1 7
#define Y2 8
#define Y3 9

#define RESET 11
#define COUNTER 12
#define PULSE 50

#define OUT	1

String inputString = "";


int counter = 0;
int timer_counter = 0;

static boolean serialEventFlag = false;
static boolean isButtonPushed = false;
static boolean isGameStarted = false;
static boolean isGameOver = false;

static int start = 0;
static int stop = 0;

static uint8_t prevX = 0;
static uint8_t prevY = 0;

static int statX[4] = { 0,0,0,0 };
static int statY[4] = { 0,0,0,0 };

static int x_high = 0;

uint8_t x;
uint8_t y;

//interrupt handler 
void btn_pushed(){
	//digitalWrite(x_high, HIGH); // disable address 
	isButtonPushed = true;
}

//timer interrupt handler
void timer_handler() {
	timer_counter++;
	if (timer_counter == 6) {
		isGameOver = true;
		timer_counter = 0;
		//Timer1.stop();
	}
	else{isGameOver = false;}
}

void setup()
{
	Serial.begin(9600);

	for (int i = 2; i < 10; i++)
	{pinMode(i, OUTPUT);
	digitalWrite(i, HIGH);
	}

	pinMode(OUT, INPUT);
	pinMode(RESET, OUTPUT);
	digitalWrite(RESET, HIGH); //Default
	pinMode(COUNTER, OUTPUT);

	digitalWrite(COUNTER, HIGH);
	delay(PULSE);
	digitalWrite(RESET, LOW);
	
	
	attachPCINT(digitalPinToPCINT(OUT), btn_pushed, FALLING);

	Timer1.initialize();
	Timer1.setPeriod(5000000);//interrupt every 5 seconds
	Timer1.attachInterrupt(timer_handler);

	inputString.reserve(200);
	randomSeed(analogRead(5));

}

void zeroOut() {
	for (int i = 2; i < 10; i++)
	{
		pinMode(i, OUTPUT);
		digitalWrite(i, HIGH);
	}

}

uint8_t generateRandomX() {
	int i = random(4);
	switch (i) {
	case 0: //b0001
		
    //x_high = X0;
		return B0001;
	case 1: //b0010
	 // x_high = X1;
		return B0010;
	case 2://b0100
    //x_high = X2;
		return B0100;
	case 3://b1000
	  //x_high = X3;
		return B1000;
	default:
		return 0;
	}
}

uint8_t generateRandomY() {
	int i = random(4);
	switch (i) {
	case 0: //b0001
		
		return B0001;
	case 1: //b0010
		
		return B0010;
	case 2://b0100
		
		return B0100;
	case 3://b1000
		
		return B1000;
	default:
		return 0;
	}
}

void setRandomAddress() {


	prevX = x;
	prevY = y;

	for (int i = 2; i < 10; i++) {
	
		digitalWrite(i, HIGH);


	}

	x = generateRandomX();
	y = generateRandomY();

	while ((prevX == x) && (prevY == y)) {
		 x = generateRandomX();
		 y = generateRandomY();
   
	}

	Serial.print("X : ");
	Serial.println(x, BIN);
	Serial.print(" y: ");
	Serial.println(y, BIN);
	uint8_t address;

	address = y;
	address = address << 4;
	address = address | x;
#ifdef DEVMODE
	Serial.print("Address: ");
	Serial.println( address,BIN);
 #endif
	zeroOut();
	for (int i = 2; i < 10; i++) {
		uint16_t temp = (~((address >> (i - 2)) & B00000001))&B00000001;
		digitalWrite(i,temp);
   #ifdef DEVMODE
		Serial.print("Pin " + (String)i + " value ");
		Serial.println(temp, BIN);
    #endif

	}

}



void loop()
{
	

	if (isGameStarted) {

		if (isButtonPushed) {
			counter++;
      #ifdef DEVMODE
      Serial.println("Actual pin" + (String)(x_high)+ " value: "+ (String)(digitalRead(x_high)));
      #endif
			setRandomAddress();
			isButtonPushed = false;
			//pulse for counter
     digitalWrite(COUNTER,LOW);
     delay(PULSE);
     digitalWrite(COUNTER,HIGH);
		}

		if (isGameOver) {
			//process things, display result.
     #ifdef DEVMODE
			Serial.println("Game ended with " +(String) counter + " points");
     #endif
			stop = millis();
      #ifdef DEVMODE
			Serial.println("Seconds elapsed: " + (String)((stop - start)/1000));
      #endif
	

		
			counter = 0;
			timer_counter = 0;
			isGameStarted = false;
			isGameOver = false;

		}

		



	}

#ifdef DEVMODE
	if (serialEventFlag) {

		if (inputString.substring(0, 1) == "X") {
			Serial.println((isGameStarted) ? "Game started" : "Game Stopped");
			Serial.println((isButtonPushed) ? "Button pushed" : " Button not pushed");

		}
		else if ((inputString.substring(0, 5) == "START")) {
			//elindítjuk a játékot
			isGameStarted = true;
      //nullázzuk a számlálót
       digitalWrite(RESET,HIGH);
	   digitalWrite(PULSE, LOW);
       delay(PULSE);
       digitalWrite(RESET,LOW);
			Serial.println("Started");
			timer_counter = 0;
			counter = 0;
			Timer1.restart();
			start = millis();
			setRandomAddress();


		}



		else {
			Serial.println("unknown");
		}

		serialEventFlag = false;
		inputString = "";

	}
#endif
}

#ifdef DEVMODE
void serialEvent() {
		int i = 0;
		while (Serial.available()) {

			char inChar = (char)Serial.read();
			// add it to the inputString:
			inputString += inChar;
			
			// if the incoming character is a newline, set a flag
			// so the main loop can do something about it:
			if (inChar == '\n') {
				
				serialEventFlag = true;
			}
			i++;
		}
		
	}
 #endif
