/******************************************************************
*
* ChampLab basic schema for designing game applications
* @Author Daniel Husztik
* husztikd@gmail.com
*
*
******************************************************************/

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <TimerOne.h>
#include <MsTimer2.h>
//--
#include "Usb.h"
#include "PM_usb.h"
#include "PM_dev.h"
#include "max3421e.h"
#include <avr/wdt.h>
//--



#if 0
#define DEVMODE
#endif

#ifdef DEVMODE
#define DEBUGLN(x) Serial.println(x);Serial.flush();
#define DEBUG(x) Serial.print(x);Serial.flush();
#else
#define DEBUGLN(x) ;
#define DEBUG(x) ;
#endif

/* GAME PREFERENCES */
/*Rowing ip 151-156*/
#define hardware_ID 153		/*Unique hardware ID used for identification*/
#define MAX_RETRIES 3		/*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500		/*Time limit of acknowledge reception*/

/*HW reset megoldás*/
#define resetPin A5 /*ez az ami a resetre kell kötni*/

/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;
volatile uint8_t timer_counter = 0;

//game spec
// The main objects
USB     Usb;
PM_usb  PmUsb(&Usb);
PM_dev  PmDev(&PmUsb);
uint32_t start_meters = 0;
uint32_t stop_meters = 0;
uint32_t result = 0;
uint8_t state = 0;
long start = 0;


#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152337 }";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152337 }";
String ack = "{\"Status\":1,\"Type\":1}";


typedef enum message_type {
	START = 1,
	RESULT = 2,
	READY = 3,
	ACK = 4
};


//booleans for controlling the game
bool valid_pkt_received = false;	//true if valid  packet is received from the server
bool game_started = false;			//true, if game started
bool game_over = false;				//true, if game ended
bool idle_state = true;				//true, if no game runs, and waits for UDP package
bool timerFlag = false;
bool timeoutFlag = false;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID };
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;		//server remote port to connect to 
EthernetClient client;

//interrupt functions

void timerISR() {
	timerFlag = true;
}


void timeout() {
	timeoutFlag = true;
}

void reset(const char* message) {
  DEBUGLN(message);

  if (!client.connected()) {
    client.stop();
  }
  
  //HW reset:
  digitalWrite(resetPin, 0);

  //SW reset:
  //wdt_enable(WDTO_15MS);
  //while (1);
}

//function prototypes
int receiveServerMessage();

void ConnectServer();

void clearData();
uint8_t  sendMessageWithTimeout(String message);


void initEthernet();
void timerInit();

void setup() {

  //resethez
  digitalWrite(resetPin, 1);
  pinMode(resetPin, OUTPUT);
  
  wdt_disable(); // disable watchdog timerdt
  
#if defined(DEVMODE)
	Serial.begin(9600);
#endif

  userID.reserve(200);

//disable SD card
  pinMode(4,OUTPUT);
  digitalWrite(4,HIGH);
    
	MsTimer2::set(ACK_TIMEOUT, timeout); // 500ms period
	timerInit();
	initEthernet();
	sendMessageWithTimeout(ready);
	//usb shield setup 

	// Setup the SPI select pins so the boards don't interfere during initialization
	// Ethernet SS_PIN 8
	// USB       SS_PIN 10

	pinMode(10, OUTPUT);
	digitalWrite(10, HIGH);



	// For our Freeduino with Usb Host, we have to de-assert the hardware reset pin
	// for the usb chip (the Usb library has no knowledge of this pin)

	pinMode(7, OUTPUT);
	digitalWrite(7, HIGH);

	pinMode(9, INPUT);
	digitalWrite(9, HIGH);

	//spi.init hi�nyzik, usbhost.h-ban deklar�lt
	//max begin

	spi::init();
	MAX3421e <Pb2, Pb1> Max3421e();
	MAX3421e <Pb2, Pb1> reset();
	MAX3421e <Pb2, Pb1> begin();
	MAX3421e <Pb2, Pb1> init();


	if (Usb.Init() == -1) {
#ifdef DEVMODE
		Serial.println("Usb.Init failed: OSC did not start.");
#endif
	}
	else {
#ifdef DEVMODE
		Serial.println("USB init sucess");
#endif
	}
	uint8_t status = 0;
 long start = millis();
	while (!status) {
		Usb.Task();
		status = PmDev.getDataIsValid();

    if(millis() - start > 5000){
      stop_meters += 10;
      break;
    }
    
	}

	



#ifdef DEVMODE
	Serial.println("Setup finished");
#endif

}

void timerInit() {
	Timer1.initialize();
	Timer1.setPeriod(5000000);//interrupt every 5 seconds
	Timer1.attachInterrupt(timerISR);
	Timer1.stop();
}

void initEthernet() {
	Ethernet.begin(mac, ownIP); // we use DHCP


	delay(1000); // give the Ethernet shield a second to initialize
#if defined(DEVMODE)
	Serial.println("connecting...");
#endif

	// if you get a connection, report back via serial:
	if (client.connect(serverIP, serverPort)) {
#if defined(DEVMODE)
		Serial.println("connected");
#endif

	}
	else {
		// if you didn't get a connection to the server:
#if defined(DEVMODE)
		Serial.println("connection failed");
#endif
	}

}

int receiveServerMessage() { // WARNING: BLOCKING STATEMENT
	String received = "";
	valid_pkt_received = false;
	while (client.available()) {
		char c = client.read();
		received += c;

	}


	if (received != "") {
		StaticJsonBuffer<150> jsonBuffer;
		received.toCharArray(json, received.length());
		JsonObject& root = jsonBuffer.parseObject(json);

#ifdef DEVMODE
		Serial.println("Received:" + received);
#endif

		if (!root.success()) {
#ifdef DEVMODE
			Serial.println("parseObject() failed");
			error++;
			Serial.println("Errors: " + (String)error);
#endif

			valid_pkt_received = false;
			return 0;
		}
		else {
#ifdef DEVMODE
			Serial.println("Valid pkt");
			Serial.println("Errors: " + (String)error);
#endif
			//deviceID = root["DeviceId"];
			//if (deviceID == hardware_ID) {
			String uID = root[(String)("UserId")];
			userID = uID;
			//type = root["Type"];
			//result1 = root["Result1"];
			status = root["Status"];
			// ha userid = 0 �s status = 1 akkor ack, ha 
			// userid != 0 akkor start game
#if defined(DEVMODE)

			Serial.print("UId: ");
			Serial.println(userID);
			Serial.print("Type: ");
			Serial.println(type);
			Serial.print("Result: ");
			Serial.println(result1);
			Serial.print("Status: ");
			Serial.println(status);

#endif
			memset(json, 0, 150);
			valid_pkt_received = true;

			if (userID == 0 && status == 1) {
				return ACK;
			}
			else if (userID == 0 && status != 0)
				return 0;
			else
				return START;


		}
	}

	else {
	 if(idle_state){
      ConnectServer();
    }else{
    ConnectServer();
    }
		return 0;
	}

}


void ConnectServer(){ //WARNING: BLOCKING STATEMENT

  if (!client.connected()) {
    client.stop();
    while (!client.connect(serverIP, serverPort));
    client.println(ready5);

  }

}


void ConnectServerDefault(){ //WARNING: BLOCKING STATEMENT

  if (!client.connected()) {
    client.stop();
    reset("reset ConnectServerDefault");
    while (!client.connect(serverIP, serverPort));
    client.println(ready);

  }

}



void clearData() {
	//deviceID = 0;
	userID = "";
	type = 0;
	result1 = 0;


}

uint8_t sendMessageWithTimeout(String message) {
	//String message = "{ \"Type\":" + (String)(3) + "\"DeviceId\":" + (String)(hardware_ID)+",\"Status\" :" + (String)(1)+"}";
	uint8_t retries = 0;

	if(idle_state){
  ConnectServerDefault();
}else{
  ConnectServer();
}


	MsTimer2::start();
	while (1) {
		bool ack = false;
		client.println(message);
		while (timeoutFlag == false) {
			if (receiveServerMessage() == ACK) {
				ack = true;
				break;
			}
		}
		if (ack) {
			MsTimer2::stop();

#ifdef DEVMODE
			Serial.println("ACK received");
#endif
			break;
		}
		else {

			retries++;
			timeoutFlag = false;
			MsTimer2::start();

#ifdef DEVMODE
			Serial.println("Timeout: " + (String)retries);
#endif

		}
   
    client.stop(); // no ack, disconnecting
    client.connect(serverIP, serverPort); //reconnecting


		if (retries >= MAX_RETRIES) {
     client.stop();
      reset("reset sendMessageWithTimeout");
      client.println(ready); // if too many retries happened, sending ready with status 3
#ifdef DEVMODE
			Serial.println("Max tries reached");
#endif

			break;

		}else{
     

  if(idle_state){
      client.println(ready); // sending ready with status 3
      }else{
       client.println(ready5); // sending ready with status 5
      }
      
    }

	}
	return client.connected();
}

uint8_t sendMessage(String message) {
	ConnectServer();
	client.println(message);
	return client.connected();

}



void loop() {



	if (idle_state) {

#ifdef DEVMODE
		//Serial.println("Idle state");
		//delay(50);
#endif
    Usb.Task();
		game_started = false;
		int status = 0;
    ConnectServerDefault();
		status = receiveServerMessage(); // waiting for real messages
#ifdef DEVMODE
										 //status = START;
										 //valid_pkt_received = true;
#endif	
		if (valid_pkt_received) {

			switch (status) {
			case READY:
				clearData();
				break;
			case RESULT:
				clearData();
				break; //skip packet
			case START:
#ifdef DEVMODE
				Serial.println("Game started");
#endif
        ConnectServer();
				sendMessage(ack); //simple ack message, no answer 
				game_started = true;
				//usb shield init

				Usb.Task();

         start = millis();
				while (!state) {
					Usb.Task();
         
					state = PmDev.getDataIsValid();	

           if(millis() - start > 5000){
            stop_meters += 10;
              break;
            }
				}
				

				start_meters = PmDev.getMeters();

				Timer1.setPeriod(5000000);
				Timer1.restart();
				idle_state = false;
				valid_pkt_received = false;
       start = 0;

				break;
			default:
				break;


			}

		}

	}



	if (game_started) {
		//start and handle the game here

#ifdef DEVMODE
		Serial.println("Game is running");
#endif





		if (timerFlag) {
			
			// handle timer interrupt here
			timer_counter++;

			if (timer_counter == 6) {

				Usb.Task();
				stop_meters = PmDev.getMeters();

				game_over = true;
				game_started = false;
				
			}
			timerFlag = false;
		
		}

		//end of game handling here
	}

	if (game_over) {
		//handle game over here
		Timer1.stop();
		result1 =  stop_meters - start_meters;

		//end of game over handling
		String result = "{\"Type\":2,\"UserId\":\"" + (String)(userID)+"\",\"Result1\":" + (String)(result1)+"}";
		sendMessageWithTimeout(result);
		game_over = false;
		idle_state = true;
		stop_meters = 0;
		start_meters = 0;
		timer_counter = 0;
		clearData();

    //két játék között reset, hogy tiszta lappal induljunk
    reset("gameOver");

	}


}

