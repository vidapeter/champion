/******************************************************************

  ChampLab basic schema for designing game applications
  @Author Daniel Husztik
  husztikd@gmail.com


******************************************************************/

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include "PinChangeInterrupt.h"
#include "PinChangeInterruptBoards.h"
#include "PinChangeInterruptPins.h"
#include "PinChangeInterruptSettings.h"
#include <avr/wdt.h>

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
/*192.168.1.121-126*/
#define hardware_ID 122    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500   /*Time limit of acknowledge reception*/

#define greenPin 3
#define redPin 2
#define SENSOR1 5
#define SENSOR2 7

/*HW reset megoldás*/
#define resetPin A5 /*ez az ami a resetre kell kötni*/

/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;

volatile int timerCounter = 0;
long start = 0;
long stop = 0;
long result = 0;
int roundCounter = 0;

#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152318 }";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152318 }";
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

bool interruptFlag = false;
bool rightButtonFlag = false;
bool leftButtonFlag = false;
bool down_flag = false;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to
EthernetClient client;

//interrupt functions

void timerISR() {

  timerCounter++;
  timerFlag = true;
  if (timerCounter == 6) { // 6 because of the 30 sec time period (5 secs at 6 times)
    game_over = true;
    game_started = false;
    timerCounter = 0;
  } else {
    game_over = false;
  }

  //timerFlag = true;
}


void timeout() {

  timeoutFlag = true;
}


void rightButtonPushed() {
  rightButtonFlag = true;
}

void leftButtonPushed() {
  leftButtonFlag = true;
}

void down() {
  down_flag = true;
}

void reset(const char* message) {
  DEBUGLN(message);
  
  //HW reset:
  digitalWrite(resetPin, 0);

  //SW reset:
  //wdt_enable(WDTO_15MS);
  //while (1);
}

//function prototypes
int receiveServerMessage();

void ConnectServer();
void ConnectServerDefault();
void clearData();
uint8_t  sendMessageWithTimeout(String message);
void initEthernet();
void timerInit();

void setup() {

  //resethez
  digitalWrite(resetPin, 1);
  pinMode(resetPin, OUTPUT);

  wdt_disable(); // disable watchdog timer
  
#if defined(DEVMODE)
  Serial.begin(9600);
#endif

  pinMode(SENSOR1, INPUT_PULLUP);
  pinMode(SENSOR2, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);

  attachPCINT(digitalPinToPCINT(SENSOR1), down, FALLING);
  attachPCINT(digitalPinToPCINT(SENSOR2), down, FALLING); //alapból 1

  userID.reserve(200);

  //disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);


  MsTimer2::set(ACK_TIMEOUT, timeout); // 500ms period
  timerInit();
  initEthernet();
  sendMessageWithTimeout(ready);
  digitalWrite(redPin, LOW);
  digitalWrite(greenPin, HIGH);

  DEBUGLN("Setup finished");

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
  DEBUGLN("connecting...");

  // if you get a connection, report back via serial:
  if (client.connect(serverIP, serverPort)) {
    DEBUGLN("connecting...");

  }
  else {
    reset("connection failed");
  }

}


int receiveServerMessage() { // WARNING: BLOCKING STATEMENT
  //  String received = "";
  valid_pkt_received = false;
  int count = 0;
  int tries = 0;
  char c = '%';
  //  unsigned long maxwait=millis()+ACK_TIMEOUT;
  DEBUG("rcvSrvMsg: ");
  DEBUGLN((String)client.available());

  //  while (client.available()) {
  // while (c!='\n' && count<250 && maxwait>millis()) {
  while (c != '\n' && count < 250 && tries < 8000) {
    c = client.read();
    tries++;
    if (c != '\n' && c != '\r' && c != -1) {
      json[count++] = c;
    } else {
      delay(1);
    }
  }
  json[count] = 0; // end of string
#if defined(DEVMODE)
  //  Serial.print("Buffer data bytes utana: " );
  //  Serial.println(client.available());
#endif

#if defined(DEVMODE)
  //Serial.print("Received data:" );
  //Serial.println(json);
#endif

  if (count > 0) {
    DEBUG(count);
    DEBUG("Received: [");
    DEBUG(json);
    DEBUGLN("]");

    StaticJsonBuffer<150> jsonBuffer;
    //    received.toCharArray(json, received.length());
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
#ifdef DEVMODE      
      DEBUGLN("parseObject() failed");
      error++;
      DEBUG("Errors: ");
      DEBUGLN((String)error);
#endif
      valid_pkt_received = false;
      return 0;
    }
    else {
      DEBUG("Valid pkt");
      DEBUG("Errors: ");
      DEBUGLN((String)error);

      //deviceID = root["DeviceId"];
      //if (deviceID == hardware_ID) {
      String uID = root[(String)("UserId")];

      userID = uID;
      //type = root["Type"];
      //      timer_delay = root["Result1"];
      //      timer_delay = root["Result"];
      status = root["Status"];
      // ha userid = 0 és status = 1 akkor ack, ha
      // userid != 0 akkor start game
//#if defined(DEVMODE)
      //      Serial.print("UId: ");
      //      Serial.println(userID);
      //      Serial.print("Type: ");
      //      Serial.println(type);
      //      Serial.print("Delay: ");
      //      Serial.println(timer_delay);
      //      Serial.print("Status: ");
      //      Serial.println(status);
//#endif
      memset(json, 0, 150);
      valid_pkt_received = true;

      if (userID == 0 && status == 1) {
        return ACK;
      } else {
        if (userID == 0 && status != 0) {
          return 0;
        } else {
          return START;
        }
      }
    }
  }
  else {
    if (idle_state) {
      ConnectServerDefault();
    } else {
      ConnectServer();
    }
    return 0;
  }

}
/*
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
    if (idle_state) {
      ConnectServerDefault();
    } else {
      ConnectServer();
    }
    return 0;
  }

}*/

void ConnectServer() { //WARNING: BLOCKING STATEMENT

  if (!client.connected()) {
    client.stop();
    while (!client.connect(serverIP, serverPort));
    client.println(ready5);

  }

}

void ConnectServerDefault() { //WARNING: BLOCKING STATEMENT

  if (!client.connected()) {
    client.stop();
    reset("ConnectServerDefault");
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

  if (idle_state) {
    ConnectServerDefault();
  } else {
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

      DEBUGLN("ACK received");
      break;
    }
    else {

      retries++;
      timeoutFlag = false;
      MsTimer2::start();

      DEBUG("Timeout: ");
      DEBUGLN((String)retries);

    }
    client.stop(); // no ack, disconnecting
    client.connect(serverIP, serverPort); //reconnecting

    if (retries >= MAX_RETRIES) {
      client.stop();
      reset("sendMessageWithTimeout");
      ///resetting, no mentionable below
      client.println(ready); // if too many retries happened
      DEBUGLN("Max tries reached");

      break;

    } else {
      if (idle_state) {
        client.println(ready); // sending ready with status 5
      } else {
        client.println(ready5);
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

    DEBUGLN("Idle state");

    if ((!digitalRead(SENSOR1)) || (!digitalRead(SENSOR2))) {
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, LOW); //ha belepunk vöröss

    } else {
      digitalWrite(redPin, LOW); //ha nem lépünk be, zöld
      digitalWrite(greenPin, HIGH);
    }

    game_started = false;
    int status = 0;
    ConnectServerDefault();
    status = receiveServerMessage(); // waiting for real messages
//#ifdef DEVMODE
    //status = START;
    //valid_pkt_received = true;
//#endif
    if (valid_pkt_received) {

      switch (status) {
        case READY:
          clearData();
          break;
        case RESULT:
          clearData();
          break; //skip packet
        case START:
          DEBUGLN("Game started");

          sendMessage(ack); //simple ack message, no answer
          game_started = true;
          Timer1.setPeriod(5000000);

          idle_state = false;
          valid_pkt_received = false;
          Timer1.stop();
          Timer1.restart();
          //először megvárjuk, hogy a sugárba lépjen
          start = millis();
          DEBUGLN("varjuk hogy a sugarba lepjen");
          
          while (1) {
            if (digitalRead(SENSOR1) == 0 || digitalRead(SENSOR2) == 0) {

              digitalWrite(greenPin, LOW);
              digitalWrite(redPin, HIGH);

              DEBUGLN("belelepett");
              
              break;
            }
            if (game_over) {
              break;
            }
          }

          // ugrásra várunk
          DEBUGLN("Waiting for jump");
          stop = 0;
          start = millis();

          while (1) {


            if ((digitalRead(SENSOR1)) && (digitalRead(SENSOR2))) {
              digitalWrite(redPin, LOW);
              digitalWrite(greenPin, HIGH);
              DEBUGLN("felugrott");
              break;

            }

            if (game_over) {
              break;
            }

          }

          start = millis();
          delay(80);
          stop = 0;
          down_flag = false;

          rightButtonFlag = false;
          leftButtonFlag = false;


          break;
          //  default:
          // break;
      }
    }
  }

  if (game_started) {
    //start and handle the game here

    DEBUGLN("Game is running");

    if (down_flag) {

      digitalWrite(redPin, LOW);
      digitalWrite(greenPin, HIGH);
      stop = millis();
      game_over = true;
      game_started = false;
      rightButtonFlag = false;
      leftButtonFlag = false;
      down_flag = false;

      DEBUGLN("leerkezett");
    }

    //end of game handling here
  }

  if (game_over) {
    //handle game over here
    Timer1.stop();
    result1 = stop - start;
    if (result1 > 5000) {
      result = 0;
    }
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, HIGH);


    //end of game over handling
    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID) + "\",\"Result1\":" + (String)(result1) + "}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    clearData();
    timerCounter = 0;
    result1 = 0;
    start = 0;
    stop = 0;

    //két játék között reset, hogy tiszta lappal induljunk
    reset("gameOver");

    //attachPCINT(digitalPinToPCINT(SENSOR1), rightButtonPushed, FALLING);
    //attachPCINT(digitalPinToPCINT(SENSOR2), leftButtonPushed, FALLING);


  }


}
