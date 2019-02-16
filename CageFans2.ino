#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* 3 fan Noctua rack panel
 _______   ______   _______
[ Fan 1 ] [ Fan2 ] [ Fan 3 ]
[ D5PWM ] [  D4  ] [ D6PWM ]

the math is weird, but a delta is calculated from the
set point, which scales to the ambient temp.
that delta is scaled and applied to a control
highest temp in PC controls the fans. 

everything else is just for fun, and to make it run automatically
if unplugged it should go to sleep.

testing: 
with the 3 fans running full and the PC idling and just doing whatever
(streams and what not) it's running at 43 degrees at the PSU. no fans is 49.
cpu is super cool, alas the sensor is just in air. 
ambient in the shop is 18.


todo:
proper background animations based off hue counter
I really want to keep all delays out of this things so it functions
for years at a time.

what's the second dallas semi bad read number? I forget.
*/

/*-----   Settings    ------*/
//needs offset for set probably, new setup gets way more direct heat read.
const float SetHigh = 22.0; // some value over ambient to start calcing
const float SetScale = 0.2; // FanSpeed + (temp - SetTemp)*SetScale 
const int badReadNum = 10;
const int goodReadNum = 4;

/*-----   Pins    ------*/
const int fanPin1 = 4;
const int pwmFanPin2 = 6;
const int pwmFanPin3 = 5;
const int ledDataPin = 10;
const int ledClockPin = 11;
const int oneWirePin = 3;
const int intLedPin = LED_BUILTIN;

/*-----   Fixed Settings    ------*/
const int numLed = 12;
const float badRead = -127.00;

/*----- Glow Balls ----*/
float cpuTemp = 0;
float pwrTemp = 0;
float ambTemp = 0;

int fanSpeed = 0;

int cpusBad = 0;
int pwrBad = 0;
int ambBad = 0;

bool readFlag = 0; //true if fake news

bool animBuddy = 0; //for fun animation
uint8_t gHue = 0;

//thermometer addresses
DeviceAddress pwrAddy = {0x28, 0xFF, 0x8E, 0x25, 0xC3, 0x16, 0x03, 0xC3};
DeviceAddress cpuAddy = {0x28, 0xFF, 0x9D, 0x24, 0xC3, 0x16, 0x03, 0x7E};
DeviceAddress ambAddy = {0x28, 0xFF, 0xBF, 0xFE, 0xC2, 0x16, 0x03, 0xB4};

//to find addys
//DeviceAddress cpuAddy, pwrAddy, ambAddy;

/*----- Li-berry stuff ----*/
//fastLED
CRGB leds[numLed];

//dallas 1w
OneWire oneWire(oneWirePin);
DallasTemperature sensors(&oneWire);

/*----- Setup ----*/
void setup() {
  //pins 
  pinMode(fanPin1, OUTPUT);
  pinMode(pwmFanPin2, OUTPUT);
  pinMode(pwmFanPin3, OUTPUT);

  //serial
  Serial.begin(115200);

  //LEDs
  FastLED.addLeds<LPD8806, ledDataPin, ledClockPin, BRG>(leds, numLed);
  
  //tempers begarn
  sensors.begin();

  //tell the user what's up in a fun way
  endCharge();
}

/*----- Looooops ----*/
void loop() {
  tempReads();

  //flag to ignore bad reads
  if(readFlag){
    fanCalc();
    fanControl();
  }
  
  tempAnim();
  
  serialPrints();
 
 // EVERY_N_MILLISECONDS( 20 ) { gHue++; } //use this later for better temp animations
  
  delay(200); //not a huge fan, I think the sensor read will delay it
  
  //troubleshooting/setup
  //findSensors();
}


//read temps into globals, set flags if bad reads
void tempReads(){
  sensors.requestTemperatures();

  //by index
//  cpuTemp = sensors.getTempCByIndex(0);
//  pwrTemp = sensors.getTempCByIndex(1);
//  ambTemp = sensors.getTempCByIndex(2);   

  //by addy
  cpuTemp = sensors.getTempC(cpuAddy);
  pwrTemp = sensors.getTempC(pwrAddy);
  ambTemp = sensors.getTempC(ambAddy); 

  //check for BAD reads
  if(cpuTemp == badRead){cpuBad++;}else{cpuBad = 0;}
  if(pwrTemp == badRead){pwrBad++;}else{pwrBad = 0;}
  if(ampTemp == badRead){ambBad++;}else{ambBad = 0;}

  //10 bad reads we should take a break, thread gets caught here.
  if(cpuSensBad >= badReadNum || pwrSensBad >= badReadNum || ambSensBad >= badReadNum){
    //housekeeping first
    cpuBad = 0;
    pwrBad = 0;
    ambBad = 0;
    
    //let's inform the user and give temp sensors a bit
    endFlash();
    
    //this runs the sleep program
    //it will exit with # good reads in a row for the sensors
    getfucked();
  }

  //set the flag to ignore bad sensor read
  if(cpusBad == 0 || pwrBad == 0 || ambBad == 0){
    readFlag = 1;
  }else{
    readFlag = 0;
  }
}

//we kinda tried to figure this one out on stream
//it's just a basic numerical control to lag behind activity
void fanCalc(){
  //ignore the cool ones, like relationships. 
  float inTemp = max(cpuTemp, pwrTemp);

  //temp above ambient is our *set point*
  float setTemp = ambTemp + SetHigh;
  float delta = inTemp - setTemp;

  float outCalc = fanSpeed + ( delta * SetScale);
  fanSpeed = (int) constrain(outCalc, 0, 255);
}

//need global fanSpeed 0-255
//I need to break my addiction to globals
void fanControl(){  
  if(fanSpeed > 235){
    digitalWrite(fanPin1, HIGH);
  }else{
    digitalWrite(fanPin1, LOW);
  }

  if(fanSpeed > 120){
    analogWrite(pwmFanPin2, fanSpeed);
  }else{
    analogWrite(pwmFanPin2, 0);
  }
  
  analogWrite(pwmFanPin3, fanSpeed);
}

//the walls in parenthesis look like butts
void serialPrints(){
  Serial.print(cpuTemp);
  Serial.print(" | ");  
  Serial.print(pwrTemp);
  Serial.print(" | ");  
  Serial.print(ambTemp);
  Serial.print(" | ");
  Serial.print(fanSpeed);
  Serial.println("");
}

//stuck in a while until we get 10 good sensor reads
//this is basically the whole fan thing on its own
void getFucked(){
  //manually write all the fan speeds to off
  digitalWrite(fanPin0, 0);
  analogWrite(pwmFanPin2, 0);
  analogWrite(pwmFanPin3, 0);
  
  //we use different flags in these parts
  int pwrGood = 0;
  int cpuGood = 0; 
  int ambGood = 0;
  bool exitFlag = 1;

  //get stuck here and periodically check sensors
  while(exitFlag){  
    //some troubleshooting here, if serial is plugged in I want to get data
    findSensors();
    
    sensors.requestTemperatures();

    //by index
    //  cpuTemp = sensors.getTempCByIndex(0);
    //  pwrTemp = sensors.getTempCByIndex(1);
    //  ambTemp = sensors.getTempCByIndex(2);   
    
    //by addy
    cpuTemp = sensors.getTempC(cpuAddy);
    pwrTemp = sensors.getTempC(pwrAddy);
    ambTemp = sensors.getTempC(ambAddy);

    //check for GOOD reads
    if(cpuTemp == badRead){cpuGood = 0;}else{cpuGood++;}
    if(pwrTemp == badRead){pwrGood = 0;}else{pwrGood++;}
    if(ampTemp == badRead){ambGood = 0;}else{ambGood++;}

    //can we kick it? yes we can.
    if(cpuGood == goodReadNum && pwrGood == goodReadNum && ambGood == goodReadNum){
      exitFlag = 0;
      Serial.prinln("oh we're back, huh?");
    }

    //animate here for fun
    if(animBuddy){
      for(int i = 0; i<numLed; i++){
        leds[i] = CHSV(gHue++, 255, 255);
        FastLED.show();
        fadeall();
        animBuddy = 0;
        delay(10);
      }  
    }else{
      for(int i = (numLed)-1; i >= 0; i--){
        leds[i] = CHSV(gHue++, 255, 255);
        FastLED.show();
        fadeall();
        animBuddy = 1;
        delay(10);
      }
    }
  }
  
}

//to find dallas temp sensors
void findSensors(){
  if (!sensors.getAddress(cpuAddy, 0)) Serial.println("Unable to find address for outside Sens"); 
  if (!sensors.getAddress(pwrAddy, 1)) Serial.println("Unable to find address for power Sens"); 
  if (!sensors.getAddress(ambAddy, 2)) Serial.println("Unable to find address for cpu Sens"); 

  Serial.print("Device 0 Address: ");
  printAddress(cpuAddy);
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(pwrAddy);
  Serial.println();

  Serial.print("Device 2 Address: ");
  printAddress(ambAddy);
  Serial.println();
}

//address serial printer
void printAddress(DeviceAddress deviceAddress){
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

//flashes the LED bar with delay
void endFlash(){
  int b = 0;
  
  while(b <= 3){
    for( int j = 0; j < numLed; j++) {
      leds[j] = CRGB::Purple;
    }
    
    FastLED.show();    
    delay(200);
    
    for( int j = 0; j < numLed; j++) {
      leds[j] = CRGB::Black;
    }
    
    FastLED.show();    
    delay(200);
    b++;
  }
}


//I want like a cylon purple wipe
void endCharge(){
  //persist perpal wiiiiipe
  for(int i = 0; i<numLed; i++){
    leds[i] = CRGB::Purple;
    FastLED.show();
    delay(10);
  }
  //fade out after
  for(int i = (numLed)-1; i >= 0; i--){
    FastLED.show();
    fadeall();
    delay(10);
  }
}

DEFINE_GRADIENT_PALETTE( lightGradnt ){
  0,       0,  0,   0,   //black
  130,   128,  0, 128,   //purp
  200,   218, 20,   0  //orang
};

void tempAnim(){
  CRGBPalette256 lightPal = lightGradnt;

  for( int i = 0; i < numLed; i++) {
    leds[i] = ColorFromPalette(lightPal, fanSpeed); 
  }
  FastLED.show();
}
