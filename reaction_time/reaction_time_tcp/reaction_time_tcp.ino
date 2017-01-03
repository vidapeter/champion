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
/* Additional libraries */
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
/*End of additional libraries /*

#if 0
#define DEVMODE
#endif

/* GAME PREFERENCES */

#define hardware_ID 7    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500   /*Time limit of acknowledge reception*/

/* Pin definitions */

#define redPin 2
#define greenPin 3
#define buttonPin 1

/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;
/*Game defined variables*/
int timerCounter = 0;
long start = 0;
long stop = 0;
long result = 0;
int roundCounter = 0;
long results[3] = {0,0,0};
/*End of game defined variables*/

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
bool valid_pkt_received = false;  //true if valid  packet is received from the server
bool game_started = false;      //true, if game started
bool game_over = false;       //true, if game ended
bool idle_state = true;       //true, if no game runs, and waits for UDP package
bool timerFlag = false;
bool timeoutFlag = false;
/*Additional booleans*/
bool interruptFlag = false;
bool isLedOn = false;
bool gameTimeCounter = false;
bool waiting = false;
bool isGreenON = false;
/*End of additional booleans*/

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 104); // server IP address
unsigned int serverPort = 50505;   //server remote port to connect to 
//IPAddress ownIP(192, 168, 1, hardware_ID);
EthernetClient client;

//interrupt functions



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
  /*Pin setup*/
  pinMode(buttonPin, INPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  digitalWrite(greenPin, LOW);
  digitalWrite(redPin, HIGH);

  attachPCINT(digitalPinToPCINT(buttonPin), buttonPushed, FALLING);

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
  Timer1.attachInterrupt(timerHandler);
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

void ConnectServerDefault(){ //WARNING: BLOCKING STATEMENT

  if (!client.connected()) {
    client.stop();
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

/*Game functions only used in looop*/

void buttonPushed() {
 // if (digitalRead(greenPin) == HIGH) { //only after the led is on..
    interruptFlag = true;
 // }
}

void initiateLed() {
  delay(random(2000,5000));
  digitalWrite(redPin, LOW);
  digitalWrite(greenPin, HIGH);
  isGreenON = true;
  start = millis();
}

void timerHandler() {
  if (game_started){
  timerCounter++;
  if (timerCounter == 4) {
    game_over = true;
    game_started = false;
    timerCounter = 0;
    digitalWrite(redPin,HIGH);
    digitalWrite(greenPin,LOW);
  } 
  }
}

long getMinResult(long results[]) {
  long min = results[0];
  for (int i=0;i<3;i++) {
    if (min > results[i] && (results[i] != 0)) {
      min = results[i];
    }
    results[i] = 0;
  }
  return min;
}
/*End of game functions*/


void loop() {
  
  

  if (idle_state) {



    game_started = false;
    int status = 0;
    ConnectServerDefault();
    status = receiveServerMessage(); // waiting for real messages
#ifdef DEVMODE
    //status = START;
    //valid_pkt_received = true;fc
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
        Timer1.attachInterrupt(timerHandler);
        Timer1.stop();
        Timer1.restart();
        idle_state = false;
        valid_pkt_received = false;
        /*Game starting*/
         digitalWrite(redPin, HIGH);
         initiateLed();
         timerFlag = false;
         timeoutFlag = false;

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





    if (interruptFlag) {
      // handle timer interrupt here
      if(isGreenON){
        isGreenON = false;
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
          game_over = true;
          game_started = false;
          interruptFlag = false;
        }
      }
        //..
      }
    }

    //end of game handling here
  

if (game_over) {
    //handle game over here
    Timer1.stop();
    
    //end of game over handling
    result1 = getMinResult(results);
    String result = "{\"Type\":2,\"UserId\":\"" + userID+"\",\"Result1\":" + (String)(result1)+"}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    interruptFlag = false;
    clearData();
    
    
    
    
    
  }


}




