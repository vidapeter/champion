/******************************************************************

  ChampLab basic schema for designing game applications
  @Author Daniel Husztik
  husztikd@gmail.com

  // Received: [{"Status":1,"Type":0}]

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
/*ip address: 192.168.1.171*/
#define hardware_ID 174    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 900   /*Time limit of acknowledge reception*/

/*Pin definitions*/

#define ledPin 2
#define startPin 8
#define finishPin 7
#define ledfalPin 3
#define ledfaltimeout 100
#define ledfalresetsignal LOW
#define ledfaloffsetms 10000
#define MAX_UIDLENGTH 40
#define MAXJSONLENGTH 150
#define MAXJSONDATA 150
/*Variables*/
int timerCounter = 0;
unsigned long timer_delay = 0;
unsigned long start = 0;
unsigned long game_launched = 0;
unsigned long stop = 0;
uint16_t result = 0;


char json[MAXJSONLENGTH];
// String userID = "";
char userID[MAX_UIDLENGTH] = {'\0'};
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;

/*Game specific variables*/


int error = 0;

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701150118 }";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701150118 }";
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
StaticJsonBuffer<MAXJSONDATA> jsonBuffer;

//interrupt functions

void timerISR() {
  if (game_started) {
    timerCounter++;
    if (timerCounter == 20) {
      //      DEBUGLN("GameTimeout (from IRQ)");
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
  //  DEBUGLN("timeout irq");
  timeoutFlag = true;
}

void runStarted() {
  startFlag = true;
  DEBUGLN("Sensor:Start");

}

void runFinished() {
  finishFlag = true;
  startFlag = false;
  DEBUGLN("Sensor:Stop");
}

void reset(const char *message) {
  DEBUGLN(message);

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
//  Serial.begin(9600);
#if defined(DEVMODE)
    Serial.begin(9600);
#endif


  //  userID.reserve(200);

  //disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  if (hardware_ID != 175) {
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
  //  DEBUGLN("Setup finished");
}

void timerInit() {
  Timer1.initialize();
  Timer1.setPeriod(1000000);//interrupt every 1 seconds
  Timer1.attachInterrupt(timerISR);
  Timer1.stop();
}

void initEthernet() {
  DEBUGLN("initEthernet");
  Ethernet.begin(mac, ownIP);
  delay(1000); // give the Ethernet shield a second to initialize
  DEBUGLN("connecting...");

  if (client.connect(serverIP, serverPort)) {
    DEBUGLN("connected");
  } else {
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
  DEBUG("rcvSrvMsg: " );
  DEBUGLN(client.available());

  //  while (client.available()) {
  // while (c!='\n' && count<250 && maxwait>millis()) {
  while (c != '\n' && count < MAXJSONLENGTH && tries < 8000) {
  c = client.read();
    tries++;
    if (c != '\n' && c != '\r' && c != -1) {
      json[count++] = c;
    } else {
      delay(1);
    }
  }
  json[count] = 0; // end of string
  //  DEBUG"Buffer data bytes utana: " );
  //  DEBUGLN(client.available());

  //DEBUG"Received data:" );
  //DEBUGLN(json);

  if (count > 0) {
  DEBUGLN(count);
    DEBUG("Received: [" );
    DEBUG(json);
    DEBUGLN("]" );

    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
      DEBUGLN("parseObject() failed");
      error++;
      DEBUG("Errors: ");
      DEBUGLN(error);
      DEBUG("Time: ");
      DEBUGLN(millis());
      valid_pkt_received = false;
      return 0;
    }
    else {
      DEBUG("Valid pkt Errors: ");
      DEBUGLN(error);
      //deviceID = root["DeviceId"];
      //if (deviceID == hardware_ID) {
      const char *str = root["UserId"];
      if (str == NULL) {
        userID[0] = '\0';
      } else {
        strncpy(userID, str, MAX_UIDLENGTH - 1);
      }
#ifdef DEVMODE
      if (str != NULL) {
        DEBUG("str uid: ");
        DEBUGLN(str);
      }
      DEBUG("userID: ");
      DEBUGLN(userID);
#endif
      //type = root["Type"];
      //      timer_delay = root["Result1"];
      timer_delay = root["Result"];
      status = root["Status"];
      // ha userid = 0 és status = 1 akkor ack, ha
      // userid != 0 akkor start game
      memset(json, 0, MAXJSONLENGTH);
      valid_pkt_received = true;

      if (userID[0] == 0 && status == 1) {
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
    reset("connecterverdefault"); //   O.o
    while (!client.connect(serverIP, serverPort));
    //client.connect(serverIP, serverPort);
    sendMessageWithTimeout(ready);
  }
}

void clearData() {
  userID[0] = '\0';
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
  DEBUG("sending: ");
  DEBUGLN(message);

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

      //      DEBUGLN("ACK received");
      break;
    }
    else {

      retries++;
      timeoutFlag = false;
      MsTimer2::start();

      DEBUG("Timeout: ");
      DEBUGLN(retries);

    }

    client.stop(); // no ack, disconnecting
    client.connect(serverIP, serverPort); //reconnecting

    if (retries >= MAX_RETRIES) {

      client.stop();
      reset("maxretries");
      client.println(ready); // if too many retries happened, sending ready with status 3
      //      DEBUGLN("Max tries reached");

      break;

    } else {
      if (idle_state) {
        client.println(ready); // sending ready with status 3
      } else {
        client.println(ready5); // status 5 (RETRY!)
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

// azt szeretnénk, hogy a futás pályák ne egyszerre küldjék el az eredményt

void waitoffset() {
  unsigned long targettime = max(0, game_launched + 30000 + (hardware_ID - 171) * 1000 - millis());
  DEBUGLN("Waiting for forced async");
  DEBUGLN(targettime);
  DEBUGLN(game_launched);
  DEBUGLN(game_launched + targettime);
  DEBUGLN(millis());
  delay(targettime);
  DEBUGLN(millis());

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
          DEBUG("Sending ACK as answer to Start cmd");
          sendMessage(ack); //simple ack message, no answer
          DEBUG("Game start command received, waiting for ");
          DEBUG(timer_delay + (hardware_ID == 175 ? ledfaloffsetms : 0));
          DEBUGLN(" milliseconds ");
          start = millis();
          delay(max(0, timer_delay - ledfaltimeout + (hardware_ID == 175 ? ledfaloffsetms : 0)));
          digitalWrite(ledfalPin, ledfalresetsignal);
          delay(ledfaltimeout);
          digitalWrite(ledfalPin, !ledfalresetsignal);
          DEBUG("Waiting ended: ");
          //         DEBUGLN(millis() - start);
          DEBUGLN("Proceed to first sensor!");
          //         DEBUGLN(millis());
          Timer1.setPeriod(1000000);
          Timer1.restart();
          start = millis();
          game_launched = millis();
          stop = 0;
          idle_state = false;
          valid_pkt_received = false;
          game_started = true;
          timerCounter = 0;
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
      stop = 0;
      if (hardware_ID != 175) {
        detachPCINT(digitalPinToPCINT(startPin));
      }
      startFlag = false;
      timerCounter = 0;
      DEBUGLN("Started");
      // DEBUGLN(start);
      //      DEBUGLN(stop);
      //      DEBUGLN(millis());
    }

    if (finishFlag) {
      stop = millis();
      finishFlag = false;
      game_over = true;
      game_started = false;
      DEBUGLN("Stopped");
      //DEBUGLN(start);
      //DEBUGLN(stop);
      //DEBUGLN(millis());
    }
    if (timerFlag) {

    }
    //end of game handling here
  }

  if (game_over) {
    //handle game over here
    Timer1.stop();
    result1 = stop - start;

    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID) + "\",\"Result1\":" + (String)(result1) + "}";
    DEBUGLN("GameoverResult\nTime");
    DEBUG(result1);
    DEBUGLN(" msec\n");
    DEBUGLN(result);
    waitoffset();
    sendMessageWithTimeout(result);
    //      DEBUGLN("GameoverResultSent");
    game_over = false;
    idle_state = true;
    clearData();
    start = 0;
    stop = 0;
    if (hardware_ID != 175) {
      attachPCINT(digitalPinToPCINT(startPin), runStarted, FALLING);
    }
    reset("GameoverResultSentRestart");
  }
}


