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
#include <avr/wdt.h>

#if 0
#define DEVMODE
#endif

/* GAME PREFERENCES */
/*registration 192.168.1.101-108*/
#define hardware_ID 101    /*Unique hardware ID used for identification*/
#define MAX_RETRIES 3   /*Maximum number of retries with acknowledge*/
#define ACK_TIMEOUT 500   /*Time limit of acknowledge reception*/
#define trigPin 6
#define echoPin 7
/*Variables*/

char json[150];
String userID = "";
volatile uint8_t type = 0;
volatile uint16_t result1 = 0;
volatile uint16_t result2 = 0;
volatile uint8_t status = 0;
/*game specific variables*/
long height = 0;
long weight = 0;
uint8_t  data[30];
int i = 0;
bool full_sentence = false;
uint8_t pointer = 7;

uint8_t length = 0;
long counter = 0;

#ifdef DEVMODE
int error = 0;
#endif

String ready = "{ \"Type\":3,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID)+"}}";
String ready5 = "{ \"Type\":5,\"Payload\":{\"DeviceId\":" + (String)(hardware_ID)+"}}";
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

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, hardware_ID};
IPAddress serverIP(192, 168, 1, 100); // server IP address
IPAddress ownIP(192, 168, 1, hardware_ID);
unsigned int serverPort = 50505;   //server remote port to connect to 
EthernetClient client;

//interrupt functions

void timerISR() {
  timerFlag = true;
}


void timeout() {
  timeoutFlag = true;
}

void reset(){
  wdt_enable(WDTO_15MS);
  while(1);
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

  Serial.begin(9600);
    pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

    userID.reserve(200);

//disable SD card
  pinMode(4,OUTPUT);
  digitalWrite(4,HIGH);

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
  Ethernet.begin(mac,ownIP); // we use DHCP


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
     if(idle_state){
      ConnectServerDefault();
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
    reset();
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
    

    if (retries >= MAX_RETRIES) {
      client.stop();
      reset();
      ///resetting, no mentionable below
      client.println(ready); // if too many retries happened, sending ready with status 3
#ifdef DEVMODE
      Serial.println("Max tries reached");
#endif

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

long  measure_height() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
long  duration = pulseIn(echoPin, HIGH);
  return (duration/2) / 29.1;
}

long meause_weight(){
  
    while (1)
    {
      uint8_t datain = Serial.read();
      Serial.print(datain, HEX);
      if (datain == 0x0A) 
        break;

      counter++;
      if (counter > 30) {
          Serial.println("Stuck");
        }
      

      
    }
   //end of aligning

   
    Serial.println("");
    Serial.println("Measuring: ");
    counter = 0;
    while (!full_sentence) {
      uint8_t datain = Serial.read();
      if (datain == 0x0A)
      {
        full_sentence = true;
      }

      data[i] = datain;

      i++;
      Serial.print(datain, HEX);

      counter++;
      if (counter >= 30) {
        Serial.println("Stuck");
      }

    }

    full_sentence = false;

    i = 0;

    Serial.println("Sentence");
    for (int x = 0; x < 30; x++) {
      //Serial.println("Data: " + (String)(data[x]) + "pos " + String(x));

    }
    Serial.println("");

    //Serial.println("Calculating");



    while (data[pointer] == 0x20) {
      pointer++;
    }

    //Serial.println("First data byte pos " + (String)(pointer)+"data" + (String)(data[pointer]));

    while (data[pointer] != 0x2E) {
      length++;
      pointer++;
    }

    uint8_t pow = 0;
    //Serial.println("Pointer " + (String)pointer + "length " + String(length));
    pointer --;

    switch (length) {

    case 1:
      Serial.print("Data ");
      Serial.println(data[pointer]);
      weight = (data[pointer]) - 48;
      Serial.println("One");
      break;

    case 2:
      weight = (data[pointer]) - 48 + ((data[pointer-1]) - 48)*10;
      Serial.println("Two");
      break;
    case 3:
      weight = (data[pointer]) - 48 + ((data[pointer - 1]) - 48) * 10 + ((data[pointer - 2]) - 48) * 100;
      Serial.println("Three");
      break;

    default:
      Serial.println("Error");
       break;



    }


    Serial.println("Calculated weight: " + (String)(weight));


    length = 0;
    pointer = 7;
    weight = 0;

  return weight;

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

        sendMessage(ack); //simple ack message, no answer 
        game_started = true;
       // Timer1.setPeriod(5000000);
       // Timer1.restart();
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
      long start = millis();
       while(1){
        height = measure_height();

        if(height!= 0){
          break;  //break if not zero data
        }

        if(start -millis() > 1000){
          break; // break if timeout
        }
       }
        for (int x = 0; x < 30; x++) {
         data[x] = 0;
      }
      //measure_weight();

      /*After measuring*/
      game_over = true;
      game_started = false;


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
    //Timer1.stop();
    result1 = 250-height;
    result2 = weight;

    //end of game over handling
    String result = "{\"Type\":2,\"UserId\" :" + (String)(userID)+",\"Result1\":" + (String)(result1)+",\"Result2\":" + (String)(result2)+"}";
    sendMessageWithTimeout(result);
    game_over = false;
    idle_state = true;
    clearData();
    
    
    
  }


}


