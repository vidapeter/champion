
#define trigPin 11
#define echoPin 12

bool isRegistrationOn = false;
bool isRegistrationOver = false;
int counter;

String startButton = "";
long duration, cm, inches, result;
long results[3] = {0,0,0};

void setup() {
  while (!Serial);
  Serial.begin(9600);
  Serial.println("start measuring height");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

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
      Serial.println("MEASURING DONE");

      result = getMinResult(results);

      Serial.println("Your height is: " + (String)(result));
    } else {
      while (Serial.available() && isRegistrationOn == false) {
         startButton = (char)Serial.read();
        if  (startButton == "x") {

            isRegistrationOn = true;

            Serial.println("measuring started.");
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
