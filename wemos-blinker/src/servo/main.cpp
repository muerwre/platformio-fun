#include <Servo.h>
Servo myservo; // create servo object to control a servo
// twelve servo objects can be created on most boards

int pos;
void setup()
{
  Serial.begin(115200);
  delay(500);                         // wait for the servo to initialize
  pos = myservo.read();               // read the current position of the servo
  myservo.writeMicroseconds(pos);     // set servo to mid-point position
  myservo.attach(D1, 500, 2400, pos); // attaches the servo on GIO2 to the servo object
  delay(500);                         // wait for the servo to initialize
  Serial.printf("Servo attached to pin D4, initial pos: %d\n", pos);
}

void loop()
{
  static int delta = 5;

  myservo.write(pos); // tell servo to go to position in variable 'pos'
  pos += delta;
  if (pos >= 180 || pos <= 0)
    delta = -delta;
  Serial.printf("Servo moved to pos: %d\n", pos);
  delay(100); // waits 15ms for the servo to reach the position
}