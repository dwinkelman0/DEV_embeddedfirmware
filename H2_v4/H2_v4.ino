#include <i2c_t3.h>
#include <Metro.h>
#include <stdio.h>
#include "INA.h"

#define LED1 10
#define LED2 11
#define LED3 13

#define SUPPLY_VALVE 5
#define PURGE_VALVE 6
#define FAN 4
#define FAN_READ 17
#define THERM 21

#define BUZZER 9
#define SHORT_CIRCUIT 8
#define PASS 3
int incomingByte = 0;
double fanSpeed = .4;
double voltage = 0;
double current = 0;
double power = 0;
double fan_current = 0;
int short_timer = 0;
float prct = 0;
double temp = 0;

float flow = 0;
float h2energy = 0;
float eff = 0;
float flowcurrent = 0;
float leak = 0;
int purgeCount = 0;

#define FLOWMETER_BUF_SIZE 100
char flowmeterBuf[FLOWMETER_BUF_SIZE];
uint32_t flowmeterBufPos = 0;

const uint32_t Short_StartupIntervals[6] = {0, 616, 616, 616, 313, 313};
const uint32_t Short_StartupDurations[6] = {50, 50, 50, 50, 100, 50};
uint8_t Short_StartupIndex = 0;

uint32_t Short_Intervals[1] = {100};
const uint32_t Short_Durations[1] = {20};
uint8_t Short_ind = 0;
uint32_t Purge_Intervals[1] = {300};
const uint32_t Purge_Durations[1] = {175};
uint8_t Purge_ind = 0;

uint32_t Short_Interval = Short_Intervals[0]*1000;
uint32_t Short_Duration = Short_Durations[0];
uint32_t Purge_Interval = Purge_Intervals[0]*1000;
uint32_t Purge_Duration = Purge_Durations[0];
Metro Short_IntervalTimer = Metro(Short_Interval);
Metro Short_DurationTimer = Metro(Short_Duration);
Metro Purge_IntervalTimer = Metro(Purge_Interval);
Metro Purge_DurationTimer = Metro(Purge_Duration);

void setup() {
  Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);
  Serial.begin(115200);
  Serial1.begin(57600);
  UART0_C3 = 16;//tx invert
  UART0_S2 = 16;//rx invert

  analogReadResolution(12);
  
  INAinit();
  
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  pinMode(PASS, OUTPUT);
  digitalWrite(PASS, HIGH);
  pinMode(SHORT_CIRCUIT, OUTPUT);
  digitalWrite(SHORT_CIRCUIT, LOW);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(SUPPLY_VALVE, OUTPUT);
  digitalWrite(SUPPLY_VALVE, HIGH);
  pinMode(PURGE_VALVE, OUTPUT);
  digitalWrite(PURGE_VALVE, LOW);
  pinMode(FAN, OUTPUT);
  analogWriteFrequency(FAN,100000);
  pinMode(FAN_READ, INPUT);
  pinMode(THERM, INPUT);

  bootup();
  
}

float readFlowmeter()
{
  Serial1.print("A\r");
  while(Serial1.available())
  {
    flowmeterBuf[flowmeterBufPos++] = Serial1.read();

    if(flowmeterBufPos >= FLOWMETER_BUF_SIZE)
      flowmeterBufPos = FLOWMETER_BUF_SIZE - 1;
    
    if(flowmeterBuf[flowmeterBufPos - 1] == '\r')
      break;
  }

  flowmeterBuf[flowmeterBufPos] = 0;
  Serial.println(flowmeterBuf);

  char* buf;

  float massFlow = 0;
  int pos = 0;
  
  buf = strtok (flowmeterBuf," ");
  while (buf != NULL)
  {
//    Serial.println(buf);
    if(pos == 4)
      massFlow = atof(buf);
    
    pos++;
    buf = strtok (NULL, " ");
  }
  
  flowmeterBufPos = 0;

//  Serial.println(massFlow,4);
  
  return massFlow;
}

void loop() {
  digitalWrite(LED2, digitalRead(LED1));
  digitalWrite(LED1, !digitalRead(LED1));
  voltage = INAvoltage();
  current = INAcurrent();
  power = voltage*current;
  fan_current = analogRead(FAN_READ) / 4096.0 * 3.3 / 0.20;
  prct = analogRead(THERM)/4096.0;
  temp= .95*temp + .05*((20 + (prct/(1-prct)*1208 - 1076)/3.8) * 9/5 + 32);

  
  Serial.print("FC: ");
  Serial.print(voltage, 4);
  Serial.print("V ");
  Serial.print(current, 4);
  Serial.print("A ");
  Serial.print(power, 4);
  Serial.println("W");

  Serial.print("Fan: ");
  Serial.print(fan_current);
  Serial.print("A ");
  Serial.print(fan_current*5);
  Serial.println("W ");

  Serial.print("Temp: ");
  Serial.print(prct,4);
  Serial.print(" ");
  Serial.println(temp);
  
  
  delay(200);
  
  flow = readFlowmeter()/1000;  // g/s
  h2energy = flow*119.96e3;     // W
  if(flow > 0)
    eff = .95*eff + .05*power/h2energy*100;     // %
  else eff = 0;
  flowcurrent = flow/2.01588 * 2 * 6.0221409e23 * 1.60217733e-19 / 20; // A
  leak = (flowcurrent - current)/flowcurrent*100;
  Serial.print("eff: ");
  Serial.print(eff,4);
  Serial.print("% leak: ");
  Serial.print(leak, 4);
  Serial.println("%");
  /*while(Serial1.available())
  {
    Serial.print((char)Serial1.read());
  }
  Serial.println();*/

  if (Serial.available() > 0) {
                // read the incoming byte:
                incomingByte = Serial.read();

                // say what you got:
                if(incomingByte=='p'){
                  purge(150);
                  }
                if(incomingByte=='s'){
                  FCShort(20);
//                  delay(1000);
                }
                if(incomingByte >= '0' && incomingByte <= '9'){
                  float val = (incomingByte - '0') / 10.0;
                  changeFan(val);                
                }
                if(incomingByte == 'b'){
                  bootup();
                }
//                Serial.print("I received: ");
//                Serial.println(incomingByte, DEC);
                // p = 112, s = 115
   }
   int setpoint = 17;
   Serial.print("Health : ");
   Serial.println(current - (voltage - setpoint)/(-0.75));
   if(voltage > 13.5 && voltage < setpoint && current < (voltage - setpoint)/(-0.75) && (millis() - short_timer) > 10000){
    FCShort(20);
    short_timer = millis();
    Serial.println(short_timer);
    purgeCount++;
    if(purgeCount % 3 == 0){
      delay(1000);
      purge(150);
    }
//    delay(3000);
   }
  
}

void bootup(){
  Serial.println("Booting Up");
  digitalWrite(FAN,LOW);
  
  purge(3000);

  for(uint8_t ind = 0; ind<(sizeof(Short_StartupIntervals)/sizeof(uint32_t)); ind++){
    delay(Short_StartupIntervals[ind]);
    FCShort(Short_StartupDurations[ind]);
  }

  analogWrite(FAN,HIGH);
  delay(100);
  analogWrite(FAN,fanSpeed*255);
  
  
}

void changeFan(double fanSpeed){
  analogWrite(FAN,HIGH);
  delay(100);
  analogWrite(FAN,fanSpeed*255);
}

void purge(int duration)
{
  Serial.println("Purging");
  digitalWrite(PURGE_VALVE,HIGH);
  delay(duration);
  digitalWrite(PURGE_VALVE,LOW);
}

void FCShort(uint32_t duration)
{
  Serial.println("Shorting");
  digitalWrite(PASS, LOW);
  delay(1);
  digitalWrite(SHORT_CIRCUIT, HIGH);
  delay(duration);
  digitalWrite(SHORT_CIRCUIT, LOW);
  delay(2);
  digitalWrite(PASS, HIGH);
}




