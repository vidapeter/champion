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

#if 1
#define DEVMODE
#endif

/* GAME PREFERENCES */
/*ip address: 192.168.1.171*/
#define hardware_ID 175    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 1900   /*Time limit of acknowledge reception*/

/*Pin definitions*/

#define ledPin 2
#define startPin 8
#define finishPin 7
#define ledfalPin 3
#define ledfaltimeout 100
#define ledfalresetsignal LOW
#define ledfaloffsetms 10000
/*Variables*/
int timerCounter = 0;
unsigned long timer_delay = 0;
unsigned long start = 0;
unsigned long stop = 0;
uint16_t result = 0;


char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;

/*Game specific variables*/


#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}}";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}}";
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

/*Game specific flags*/
bool startFlag = false;
bool finishFlag = false;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to
EthernetClient client;

//interrupt functions

void timerISR() {
  if (game_started) {
    timerCounter++;
    if (timerCounter == 20) {
#ifdef DEVMODE
      Serial.println("GameTimeout");
#endif
      game_over = true;
      game_started = false;
      timerCounter = 0;
      stop = millis();
      start = stop;
    }
  } else {
    timerFlag = true;
  }
}


void timeout() {
  timeoutFlag = true;
}

void runStarted() {
  startFlag = true;
#ifdef DEVMODE
      Serial.println("Sensor:Start");
#endif

}

void runFinished() {
  finishFlag = true;
  startFlag = false;
#ifdef DEVMODE
      Serial.println("Sensor:Stop");
#endif

}

void reset() {
  wdt_enable(WDTO_15MS);
  while (1);
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
  wdt_disable(); // disable watchdog timer
#if defined(DEVMODE)
  Serial.begin(9600);
#endif


  userID.reserve(200);

  //disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  if (hardware_ID!=175) {
   pinMode(startPin, INPUT_PULLUP);
   pinMode(finishPin, INPUT_PULLUP);
   attachPCINT(digitalPinToPCINT(startPin), runStarted, FALLING);
   attachPCINT(digitalPinToPCINT(finishPin), runFinished, FALLING);
  }
  else {
   pinMode(ledfalPin, OUTPUT);
   digitalWrite(ledfalPin, !ledfalresetsignal);
  }



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
  Timer1.setPeriod(1000000);//interrupt every 1 seconds
  Timer1.attachInterrupt(timerISR);
  Timer1.stop();
}

void initEthernet() {
#if defined(DEVMODE)
  Serial.println("initEthernet");
#endif
  Ethernet.begin(mac, ownIP); 
  delay(1000); // give the Ethernet shield a second to initialize
#if defined(DEVMODE)
  Serial.println("connecting...");
#endif

#if defined(DEVMODE)
  if (client.connect(serverIP, serverPort)) {
    Serial.println("connected");
  } else {
    Serial.println("connection failed");
  }
#endif
}

int receiveServerMessage() { // WARNING: BLOCKING STATEMENT
  //  String received = "";
  valid_pkt_received = false;
  int count = 0;
  char c = '%';
  unsigned long maxwait=millis()+ACK_TIMEOUT;
#if defined(DEVMODE)
  Serial.print("Buffer data bytes elotte: " );
  Serial.println(client.available());
#endif

//  while (client.available()) {
  while (c!='\n' && count<250 && maxwait>millis()) {
    c = client.read();
    if (c!='\n' && c!='\r' && c!=-1) {
      json[count++] = c;
    } else {
      delay(5);
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
#ifdef DEVMODE
    Serial.println(count);
    Serial.print("Received: [" );
    Serial.print(json);
    Serial.println("]" );
#endif
    StaticJsonBuffer<150> jsonBuffer;
    //    received.toCharArray(json, received.length());
    JsonObject& root = jsonBuffer.parseObject(json);

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
#if defined(DEVMODE)
      Serial.print("x1");
#endif

      userID = uID;
      //type = root["Type"];
//      timer_delay = root["Result1"];
      timer_delay = root["Result"];
#if defined(DEVMODE)
      Serial.print("x2");
#endif
      status = root["Status"];
#if defined(DEVMODE)
      Serial.print("x3");
#endif
      // ha userid = 0 és status = 1 akkor ack, ha
      // userid != 0 akkor start game
#if defined(DEVMODE)
      Serial.print("UId: ");
      Serial.println(userID);
      //      Serial.print("Type: ");
      //      Serial.println(type);
      Serial.print("Delay: ");
      Serial.println(timer_delay);
      Serial.print("Status: ");
      Serial.println(status);
#endif
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
    reset(); //   O.o
    while (!client.connect(serverIP, serverPort));
    //client.connect(serverIP, serverPort);
    sendMessageWithTimeout(ready);
  }
}

void clearData() {
  //deviceID = 0;
  userID = "";
  type = 0;
  result1 = 0;
}

uint8_t sendMessageWithTimeout(String message) {
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
      reset();
      client.println(ready); // if too many retries happened, sending ready with status 3
#ifdef DEVMODE
      Serial.println("Max tries reached");
#endif

      break;

    } else {
      if (idle_state) {
        client.println(ready); // sending ready with status 3
      } else {
        client.println(ready5); // status 5
      }

    }

  }
  return client.connected();
}

uint8_t sendMessage(String message) {
#ifdef DEVMODE
      Serial.println("Sendmsg1");
#endif
  ConnectServer();
#ifdef DEVMODE
//      Serial.println("Sendmsg2");
#endif
  client.println(message);
#ifdef DEVMODE
//      Serial.println("Sendmsg3");
#endif
  return client.connected();
}

void loop() {
  if (idle_state) {
    game_started = false;
    int status = 0;
    ConnectServerDefault();
    status = receiveServerMessage(); // waiting for real messages
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
          Serial.print("Sending ACK to start");
#endif

          sendMessage(ack); //simple ack message, no answer
#ifdef DEVMODE
          Serial.print("Game start command received, ");
          Serial.print("waiting for ");
          Serial.print(timer_delay);
          Serial.print(" milliseconds "); 
          if (hardware_ID==175) {
            Serial.print(ledfaloffsetms);
            Serial.print(" because ledfal");
          }
          Serial.println(".");
#endif
          start = millis();
          delay(max(0,timer_delay-ledfaltimeout+(hardware_ID==175?ledfaloffsetms:0)));
          digitalWrite(ledfalPin, ledfalresetsignal);
          delay(ledfaltimeout);
          digitalWrite(ledfalPin, !ledfalresetsignal);
/*          while (1) {
            stop = millis();
            if ((stop - start) >= timer_delay) {
#ifdef DEVMODE
              Serial.print("Stop condition");
              Serial.println(stop - start);
#endif
              break;
            }
          }
          */
#ifdef DEVMODE
          Serial.print("Waiting ended: ");
          Serial.println(millis() - start);
          Serial.println("Proceed to first sensor!");
          Serial.println(millis());
#endif
          start = 0;
          stop = 0;
          Timer1.setPeriod(1000000);
          Timer1.restart();
          idle_state = false;
          valid_pkt_received = false;
          game_started = true;
          timerCounter=0;
          finishFlag = false;
          startFlag = false;
          break;
        default:
          break;
      }
    }
  }

  if (game_started) {
    //start and handle the game here
    if (startFlag) {
      start = millis();
      stop=0;
  if (hardware_ID!=175) {
      detachPCINT(digitalPinToPCINT(startPin));
  }
      startFlag = false;
      timerCounter=0;
#ifdef DEVMODE
      Serial.println("Started");
      Serial.println(start);
      Serial.println(stop);
      Serial.println(millis());
#endif
    }

    if (finishFlag) {
      stop = millis();
      finishFlag = false;
      game_over = true;
      game_started = false;
#ifdef DEVMODE
      Serial.println("Stopped");
      Serial.println(start);
      Serial.println(stop);
      Serial.println(millis());
#endif
    }
    if (timerFlag) {

    }
    //end of game handling here
  }

  if (game_over) {
    //handle game over here
    Timer1.stop();
    result1 = stop - start;

    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID)+"\",\"Result1\":" + (String)(result1)+"}";
#ifdef DEVMODE
      Serial.println("GameoverResult");
      Serial.println("Time: "+(String)(result1)+" msec");
      Serial.println(result);
#endif
    sendMessageWithTimeout(result);
#ifdef DEVMODE
      Serial.println("GameoverResultSent");
#endif
    game_over = false;
    idle_state = true;
    clearData();
    start = 0;
    stop = 0;
  if (hardware_ID!=175) {
    attachPCINT(digitalPinToPCINT(startPin), runStarted, FALLING);
  }
#ifdef DEVMODE
      Serial.println("GameoverResultSentRestart");
#endif

  }
}


