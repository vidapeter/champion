
#define trigPin 11
#define echoPin 12

#if 1
#define DEVMODE
#endif

bool isRegistrationOn = false;
bool isRegistrationOver = false;
int counter;

String startButton = "";
long duration, cm, inches, result;
long results[3] = {0,0,0};

void setup() {
  #if defined(DEVMODE)
    Serial.begin(9600);
  #endif

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  #ifdef DEVMODE
    Serial.println("setup finished");
  #endif
}

void loop() {
    if (isRegistrationOn) {
      if (counter < 3) {
        measure();
        delay(250);
        counter++;
      } else {
        counter = 0;
        isRegistrationOn = false;
        isRegistrationOver = true;
      }
    } else if (isRegistrationOver) {
      isRegistrationOn = false;
      isRegistrationOver = false;
      #ifdef DEVMODE
        Serial.println("MEASURING DONE");
      #endif

      result = getMinResult(results);

      #ifdef DEVMODE
        Serial.println("Your height is: " + (String)(result));
      #endif
    } else {
      while (Serial.available() && isRegistrationOn == false) {
         startButton = (char)Serial.read();
        if  (startButton == "x") {

            isRegistrationOn = true;
            #ifdef DEVMODE
              Serial.println("measuring started.");
            #endif
          }
        }
      }
    delay(5);
}

void measure() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  results[counter] = (duration/2) / 29.1;
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
