#include <stdint.h>

uint8_t pwm = 51;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  TCCR1B = (TCCR1B & 0b11111000) | 2;
  pinMode(9, OUTPUT);
  analogWrite(9, pwm);
}

void loop() {
  // put your main code here, to run repeatedly:
  int a[6];
  
  a[0] = analogRead(A0);
  a[1] = analogRead(A1);
  a[2] = analogRead(A2);
  a[3] = analogRead(A3);
  a[4] = analogRead(A4);
  a[5] = analogRead(A5);

  for(int i=0;i<6;i++){
    Serial.print(i);
    Serial.print(":");
    Serial.println(a[i],DEC);
  }
  delay(2000);
  
  if (Serial.available()){
    char c;
    c = (char) Serial.read();
    switch(c){
      case '+':
        pwm = (pwm < 230) ? pwm + 25 : 255;
        break;
      case '-':
        pwm = (pwm > 25) ? pwm - 25 : 0;
        break;
      case 'M':
        pwm = 255;
        break;
      case 's':
        pwm = 0;
        break;
    }
    analogWrite(9, pwm);
    Serial.print("PWM: ");
    Serial.println(pwm, DEC);
  }
}
