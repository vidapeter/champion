/******************************************************************
*
* ChampLab basic schema for designing game applications
* @Author Daniel Husztik
* husztikd@gmail.com
*
*
******************************************************************/

/*INCLUDES*/
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
/*IP 192.168.1.161-166*/
#define hardware_ID 165    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500   /*Time limit of acknowledge reception*/

#define X0 2
#define X1 3
#define X2 4
#define X3 5

#define Y0 6
#define Y1 7
#define Y2 8
#define Y3 9

#define OUT 1

/*HW reset megoldás*/
#define resetPin A5 /*ez az ami a resetre kell kötni*/

/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;

/*Games specific variables*/
volatile int counter = 0;
volatile int timer_counter = 0;

static long start = 0;
static long stop = 0;
static uint8_t prevX = 0;
static uint8_t prevY = 0;
static int statX[4] = { 0,0,0,0 };
static int statY[4] = { 0,0,0,0 };
static int x_high = 0;
long current_time = 0;

uint8_t x;
uint8_t y;

#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152316 }";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID) + "}, \"Ver\":201701152316 }";
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
static boolean isButtonPushed = false;

/*INTERNET CONFIGURATION*/
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to 


EthernetClient client;

//interrupt functions

//TimerOne
void timerISR() {
  timerFlag = true;
  timer_counter++;
  if (timer_counter == 5) {  //5 because of the time period is 25 sec (5 sec at 5 times)
    game_over = true;
    timer_counter = 0;
    //Timer1.stop();
  }
  else{game_over = false;}
}

//MsTimer2
void timeout() {
  timeoutFlag = true;
}

/*Game specific interrupt routines*/
void btn_pushed(){
  isButtonPushed = true;
}

void reset(const char* message) {

  DEBUGLN(message);

  if (client.connected()) {
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
/*Gamde specific function prototypes*/
void setRandomAddress();
void ConnectServerDefault();

void setup() {


  //resethez
  digitalWrite(resetPin, 1);
  pinMode(resetPin, OUTPUT);

  wdt_disable(); // disable watchdog timer

#if defined(DEVMODE)
  Serial.begin(9600);
#endif

/*IO settings*/
  for (int i = 2; i < 10; i++)
  {pinMode(i, OUTPUT);
  digitalWrite(i, HIGH);
  }

  pinMode(OUT, INPUT);

  userID.reserve(200); // ez új dolog, most raktam bele, hogy ne legyen baj


  
/*Attach interrupt*/
  attachPCINT(digitalPinToPCINT(OUT), btn_pushed, FALLING);

  randomSeed(analogRead(5));

  MsTimer2::set(ACK_TIMEOUT, timeout); // 500ms period
  timerInit();
  initEthernet();
  sendMessageWithTimeout(ready);

  DEBUGLN("Setup finished");
//ezt kellene beletenni: 
/*
  current_time = millis();
  clearAll();
  setRandomAddress();
  */

}

void timerInit() {
  Timer1.initialize();
  Timer1.setPeriod(5000000);//interrupt every 5 seconds
  Timer1.attachInterrupt(timerISR);
  Timer1.stop();
}

void initEthernet() {
  Ethernet.begin(mac,ownIP); 


  delay(1000); // give the Ethernet shield a second to initialize
  DEBUGLN("connecting...");

  // if you get a connection, report back via serial:
  if (client.connect(serverIP, serverPort)) {
    DEBUGLN("connected");   
  }
  else {
    reset("connection failed");
  }

}

//új receive függvény
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
  
  if (count > 0) {

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
      DEBUGLN("Valid pkt");
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
     if(idle_state){
      ConnectServerDefault();
    }else{
      ConnectServer();
    }
    return 0;
  }
  
}*/

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
    

    if (retries >= MAX_RETRIES) {
      client.stop();
      reset("sendMessageWithTimeout");
      ///resetting, no mentionable below
      client.println(ready); // if too many retries happened, sending ready with status 3
      DEBUGLN("Max tries reached");

      break;

    }else{
     
      client.connect(serverIP, serverPort); //reconnecting
      
      if(idle_state){
        client.println(ready); // sending ready with status 5
      }else{
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

/*Game specific functions*/

void clearAll() {
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

  //Serial.print("X : ");
  //Serial.println(x, BIN);
  //Serial.print(" y: ");
  //Serial.println(y, BIN);  
  
  uint8_t address;

  address = y;
  address = address << 4;
  address = address | x;
  /*#ifdef DEVMODE
    Serial.print("Address: ");
    Serial.println( address,BIN);
  #endif*/
  DEBUG("Address: ");
  DEBUGLN((String)address);
 
  clearAll();
  for (int i = 2; i < 10; i++) {
    uint16_t temp = (~((address >> (i - 2)) & B00000001))&B00000001;
    digitalWrite(i,temp);
    DEBUG("Pin ");
    DEBUG((String)i);
    DEBUG(" value ");
    DEBUGLN((String)temp);

  }

}






void loop() {
  
  

  if (idle_state) {

  DEBUGLN("Idle state");

  if(millis() - current_time>1000){ //ha 1 sec letelt
  current_time = millis();
  clearAll();
  setRandomAddress();
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

        ConnectServer();
        sendMessage(ack); //simple ack message, no answer 
        game_started = true;
        Timer1.setPeriod(5000000);
        idle_state = false;
        valid_pkt_received = false;
        /*Game specific part*/
        timer_counter = 0;
        counter = 0;
        
        /****START LIGHTS****/
        digitalWrite(Y0,LOW);
        digitalWrite(Y1,LOW);
        digitalWrite(Y2,LOW);
        digitalWrite(Y3,LOW);

        for(int i = 0;i<5;i++){
          digitalWrite(i+1,LOW);
          delay(400);
          digitalWrite(i+1,HIGH);
        }

        clearAll();
        Timer1.setPeriod(5000000);//interrupt every 5 seconds
        Timer1.restart();
        start = millis();

        /********/        
        setRandomAddress();

        break;
      default:
        break;


      }

    }
    
  }  



  if (game_started) {
    //start and handle the game here

      DEBUGLN("Game is running");
      
      if (isButtonPushed) {
      counter++;
      DEBUG("Actual pin");
      DEBUG((String)(x_high));
      DEBUG(" value: ");
      DEBUGLN((String)(digitalRead(x_high)));
      
      setRandomAddress();
      isButtonPushed = false;


    }




    if (timerFlag) {
      // handle timer interrupt here

      //game_over = true;
      //game_started = false;
      //timerFlag = false;
    }

    //end of game handling here
  }

  if (game_over) {
    //handle game over here
    Timer1.stop();
    result1 = counter;
  
  
      DEBUG("Game ended with ");
      DEBUG((String)counter);
      DEBUGLN(" points");
     
      stop = millis();
      DEBUG("Seconds elapsed: ");
      DEBUGLN((String)((stop - start)/1000));
    

    //end of game over handling
    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID)+"\",\"Result1\":" + (String)(result1)+"}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    clearData();
    clearAll();
    counter = 0;
    timer_counter = 0;
    //szintén 

    
    //két játék között reset, hogy tiszta lappal induljunk
    reset("gameOver");
    /*
    current_time = millis();
    */
    
  }


}


