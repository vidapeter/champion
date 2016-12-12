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

#if 1
#define DEVMODE
#endif

char json[150];

#define hardware_ID 1		/*Unique hardware ID used for identification*/
#define MAX_RETRIES 3		/*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500		/*Time limit of acknowledge reception*/

uint32_t userID = 0;
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;

#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID)+"}}";
String ready2 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID)+"}}";
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

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 117); // server IP address
unsigned int serverPort = 6280;		//server remote port to connect to 
EthernetClient client;

//interrupt functions

void timerISR() {
	timerFlag = true;
}

void timeout() {
	timeoutFlag = true;
}


//function prototypes
int receiveServerMessage();

void ConnectServer();
void clearData();
uint8_t  sendMessageWithTimeout(String message);
void initEthernet();
void timerInit();

void setup() {

#if defined(DEVMODE)
	Serial.begin(9600);
#endif
	MsTimer2::set(ACK_TIMEOUT, timeout); // 500ms period
	timerInit();
	initEthernet();
	sendMessageWithTimeout(ready);
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
	Ethernet.begin(mac); // we use DHCP


	delay(1000); // give the Ethernet shield a second to initialize
	#if defined(DEVMODE)
	Serial.println("connecting...");
	#endif

	// if you get a connection, report back via serial:
	if (client.connect(serverIP, serverPort)) {
		#if defined(DEVMODE)
		Serial.println("connected");
		#endif
		// Make a HTTP request:
		//client.println("Hello, a nevem János");
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
			userID = root["UserId"];
			//type = root["Type"];
			result1 = root["Result1"];
			status = root["Status"];
			// ha userid = 0 és status = 1 akkor ack, ha 
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
			memset(json, 0, 200);
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
		ConnectServer();
		return 0;
	}
	
}

void ConnectServer(){ //WARNING: BLOCKING STATEMENT

	if (!client.connected()) {
		client.stop();
		while (!client.connect(serverIP, serverPort));
		client.println(ready2);

	}

}

void clearData() {
	//deviceID = 0;
	userID = 0;
	type = 0; 
	result1 = 0;


}

uint8_t sendMessageWithTimeout(String message) {
	//String message = "{ \"Type\":" + (String)(3) + "\"DeviceId\":" + (String)(hardware_ID)+",\"Status\" :" + (String)(1)+"}";
	uint8_t retries = 0;

	ConnectServer();

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

		if (retries >= MAX_RETRIES) {

#ifdef DEVMODE
			Serial.println("Max tries reached");
#endif

			break;

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

		game_started = false;
		int status = 0;
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

				sendMessage(ack); //simple ack message, no answer 
				game_started = true;
				Timer1.setPeriod(5000000);
				Timer1.restart();
				idle_state = false;
				valid_pkt_received = false;

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

			game_over = true;
			game_started = false;
			timerFlag = false;
		}

		//end of game handling here
	}

	if (game_over) {
		//handle game over here
		Timer1.stop();
		result1 = 1234;

		//end of game over handling
		String result = "{\"Type\":2,\"UserId\" :" + (String)(userID)+",\"Result1\":" + (String)(result1)+"}";
		sendMessageWithTimeout(result);
		game_over = false;
		idle_state = true;
		clearData();
		
		
		
	}


}

