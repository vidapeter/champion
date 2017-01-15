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
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>
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
/*ip address 192.168.1.141-146*/
#define hardware_ID 141
/*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 900   /*Time limit of acknowledge reception*/
/*Game specific*/
#define SENSOR1 5
#define SENSOR2 4  //ez 7 volt de soknál nincs bekötve
#define MAX_CYCLE 120 // 32 // ez a tömb hossza   <--- ez 120 elvileg

#define VIDEO_PIN1 A2//0 //ezt a video pint kellene használni
#define VIDEO_PIN2 A2

#define rightLED 3
#define leftLED 2

/*HW reset megoldás*/
#define resetPin 12 /*ez az ami a resetre kell kötni*/

/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;
/*game specific*/
volatile int tick = 0;
volatile int timer = 0;
static int data[MAX_CYCLE] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 0, 0, 1, 0, 2, 0, 1, 0, 0, 0, 1, 2, 1, 2, 0, 1, 0, 2, 0, 2, 0, 1, 0, 0, 1, 0, 0, 2, 0, 1, 0, 2, 0, 1, 2, 0, 1, 2, 1, 2}; //ide kellene betölteni a 120-as tömböt
static int results[MAX_CYCLE - 1];
int result;



#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701150128 }";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701150128 }";
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
//
bool isLeftAction = false;
bool isRightAction = false;
bool timerAction = false;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to
EthernetClient client;
//interrupt functions

void systemTick() {
  timerAction = true;
}

void reset(const char* message) {

  DEBUGLN(message);
  
  //HW reset:
  digitalWrite(resetPin, 0);

  //SW reset:
  //wdt_enable(WDTO_15MS);
  //while (1);
}

void selectVideo() {

  digitalWrite(VIDEO_PIN1, HIGH);
  delay(100);
  digitalWrite(VIDEO_PIN1, LOW);


}

void timeout() {
  timeoutFlag = true;
}

void rightAction() {
  isRightAction = true;

  DEBUGLN("rightAciton");
}

void leftAction() {
  isLeftAction = true;

  DEBUGLN("leftAction");
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

#if defined(DEVMODE)
  Serial.begin(9600);
#endif

  pinMode(SENSOR1, INPUT_PULLUP);
  pinMode(SENSOR2, INPUT_PULLUP);

  pinMode(rightLED, OUTPUT);
  pinMode(leftLED, OUTPUT);

  pinMode(VIDEO_PIN1, OUTPUT);
  pinMode(VIDEO_PIN2, OUTPUT);

  attachPCINT(digitalPinToPCINT(SENSOR1), rightAction, FALLING);
  attachPCINT(digitalPinToPCINT(SENSOR2), leftAction, FALLING);

  userID.reserve(200);

  //disable SD card
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  MsTimer2::set(ACK_TIMEOUT, timeout); // 500ms period
  timerInit();
  initEthernet();
  sendMessageWithTimeout(ready);
  
  DEBUGLN("Setup finished");

}

void timerInit() {
  Timer1.initialize();
  Timer1.setPeriod(250000);
  Timer1.attachInterrupt(systemTick);
  Timer1.stop();
}

void initEthernet() {
  Ethernet.begin(mac, ownIP); // we use DHCP


  delay(1000); // give the Ethernet shield a second to initialize

  DEBUGLN("connecting...");

  // if you get a connection, report back via serial:
  if (client.connect(serverIP, serverPort)) {

    DEBUGLN("connected");

  }
  else {
    // if you didn't get a connection to the server:

    DEBUGLN("connection failed");
  }

}

int receiveServerMessage() { // WARNING: BLOCKING STATEMENT
  //  String received = "";
  valid_pkt_received = false;
  int count = 0;
  int tries = 0;
  char c = '%';
  //  unsigned long maxwait=millis()+ACK_TIMEOUT;
  //DEBUG("rcvSrvMsg: ");
  //DEBUGLN(client.available());

  //  while (client.available()) {
  // while (c!='\n' && count<250 && maxwait>millis()) {
  while (c != '\n' && count < 250 && tries < 3000) {
    c = client.read();
    tries++;
    if (c != '\n' && c != '\r' && c != -1) {
      json[count++] = c;
    } else {
      delay(1);
    }
  }
  json[count] = 0; // end of string
  
  //DEBUG("Buffer data bytes utana: ");
  //DEBUGLN(client.available());

  //DEBUG("Received data: ");
  //DEBUGLN(json);

  if (count > 0) {

  //DEBUGLN(count);
  //DEBUG("Received: [");  
  //DEBUG(json);
  //DEBUGLN("]");

    StaticJsonBuffer<150> jsonBuffer;
    //    received.toCharArray(json, received.length());
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
#ifdef DEVMODE      
      DEBUGLN("parseObject() failed");
      error++;
      DEBUGLN("Errors: " + (String)error);
#endif
      valid_pkt_received = false;
      return 0;
    }
    else {
      DEBUGLN("Valid pkt");
      DEBUGLN("Errors: " + (String)error);

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

      DEBUGLN("Timeout: " + (String)retries);

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

static int calculateResult(int data[], int results[]) {
  for (int i = 0; i < MAX_CYCLE; i++) {
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
  if (results[index - 1] == leg || results[index] == leg || results[index + 1] == leg) {
    result++;
  }
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
          DEBUGLN("Game started");

          sendMessage(ack); //simple ack message, no answer
          Timer1.start();
          idle_state = false;
          valid_pkt_received = false;
          game_started = true;
          isRightAction = false;
          isLeftAction = false;

          selectVideo();
          //itt kellene egy olyan késleltetés, hogy akkor induljon a video, amikor a játék


          break;

        default:
          break;


      }

    }

  }



  if (game_started) {
    //start and handle the game here

    if (timerAction) {
      tick++;
      if (isRightAction ^ isLeftAction) {
        //Serial.println("INT"); ///?? ez miért volt itt devmode-on kívül?

        if (isLeftAction) {

          DEBUGLN("leftAction");

          results[tick] = 1;
          isLeftAction = false;
          digitalWrite(leftLED, HIGH);
          digitalWrite(rightLED, LOW);
        }
        if (isRightAction) {

          DEBUGLN("rightAction");

          results[tick] = 2;
          isRightAction = false;
          digitalWrite(rightLED, HIGH);
          digitalWrite(leftLED, LOW);
        }
      } else {
        results[tick] = 0;
        digitalWrite(leftLED, LOW);
        digitalWrite(rightLED, LOW);
        isRightAction = false;
        isLeftAction = false;
      }

      timerAction = false;

      DEBUGLN(tick);

    }

    if (tick == (MAX_CYCLE - 1)) {
      Serial.println("Max reached");
      game_over = true;
      game_started = false;
    }




    //end of game handling here
  }

  if (game_over) {
    //handle game over here
    Timer1.stop();

    //EDIT!!4
    if (!calculateResult(data, results)) {
      result1 = random(1, 30);

    } else {
      result1 = calculateResult(data, results);
    }


#ifdef DEVMODE
    Serial.print("Data: ");
    for (int i = 0; i < MAX_CYCLE; i++) {
      Serial.print(data[i], DEC);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.print("Res : ");
    for (int i = 0; i < MAX_CYCLE; i++) {
      Serial.print(results[i], DEC);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.flush();
#endif

    //end of game over handling
    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID) + "\",\"Result1\":" + (String)(result1) + "}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    clearData();
    result1 = 0;
    timer = 0;
    tick = 0;

    //két játék között reset, hogy tiszta lappal induljunk
    reset("gameOver");


  }


}
