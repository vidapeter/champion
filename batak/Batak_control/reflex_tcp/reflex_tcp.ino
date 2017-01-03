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

#if 0
#define DEVMODE
#endif

/* GAME PREFERENCES */

#define hardware_ID 7    /*Unique hardware ID used for identification*/
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

uint8_t x;
uint8_t y;

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
/*Game specific flags*/
static boolean isButtonPushed = false;

/*INTERNET CONFIGURATION*/
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 104); // server IP address
//IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to 


EthernetClient client;

//interrupt functions

//TimerOne
void timerISR() {
  timerFlag = true;
  timer_counter++;
  if (timer_counter == 5) {
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


//function prototypes
int receiveServerMessage();

void ConnectServer();
void clearData();
uint8_t  sendMessageWithTimeout(String message);
void initEthernet();
void timerInit();
/*Game specific function prototypes*/
void setRandomAddress();
void ConnectServerDefault();

void setup() {

#if defined(DEVMODE)
  Serial.begin(9600);
#endif

/*IO settings*/
  for (int i = 2; i < 10; i++)
  {pinMode(i, OUTPUT);
  digitalWrite(i, HIGH);
  }

  pinMode(OUT, INPUT);
  
/*Attach interrupt*/
  attachPCINT(digitalPinToPCINT(OUT), btn_pushed, FALLING);

  randomSeed(analogRead(5));

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
    ConnectServerDefault();
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
  clearAll();
  for (int i = 2; i < 10; i++) {
    uint16_t temp = (~((address >> (i - 2)) & B00000001))&B00000001;
    digitalWrite(i,temp);
   #ifdef DEVMODE
    Serial.print("Pin " + (String)i + " value ");
    Serial.println(temp, BIN);
    #endif

  }

}






void loop() {
  
  

  if (idle_state) {

#ifdef DEVMODE
    //Serial.println("Idle state");
    //delay(50);
#endif

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

#ifdef DEVMODE
   // Serial.println("Game is running");
#endif
      if (isButtonPushed) {
      counter++;
      #ifdef DEVMODE
      Serial.println("Actual pin" + (String)(x_high)+ " value: "+ (String)(digitalRead(x_high)));
      #endif
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
    
    #ifdef DEVMODE
      Serial.println("Game ended with " +(String) counter + " points");
     #endif
      stop = millis();
      #ifdef DEVMODE
      
      Serial.println("Seconds elapsed: " + (String)((stop - start)/1000));
      #endif
    

    //end of game over handling
    String result = "{\"Type\":2,\"UserId\" :\"" + (String)(userID)+"\",\"Result1\":" + (String)(result1)+"}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    clearData();
    clearAll();
    counter = 0;
    timer_counter = 0;
    
    
    
  }


}


