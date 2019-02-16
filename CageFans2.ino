#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* 3 fan Noctua rack panel
 _______   ______   _______
[ Fan 1 ] [ Fan2 ] [ Fan 3 ]
[ D5PWM ] [  D9  ] [ D6PWM ]

outTemp: 21 inTemp: 34 Calc: 3 Set: 31 Output: 6
Device 0 Address: 
Device 1 Address: 
*/

/*-----   Pins    ------*/
const int fanPin1 = 4;
const int pwmFanPin2 = 6;
const int pwmFanPin3 = 5;
const int DATA_PIN = 10;
const int CLOCK_PIN = 11;
const int oneWirePin = 3;
const int LedPin = LED_BUILTIN;
const int NUM_LEDS = 12;
const float SetHigh = 22.0; // some value over ambient to start calcing
const float SetScale = 0.2; // FanSpeed + (temp - SetTemp)*SetScale 
const float badRead = -127.00;
const int badReadNum = 10;
const int goodReadNum = 4;

/*----- Glow Balls ----*/
float cpuTemp = 0;
float pwrTemp = 0;
float ambTemp = 0;
int fanSpeed = 0;
int cpusBad = 0;
int pwrBad = 0;
int ambBad = 0;
bool readFlag = 0; //true if fake news
uint8_t gHue = 0;
bool animBuddy = 0; //for fun animation

//thermometer addresses
DeviceAddress pwrAddy = {0x28, 0xFF, 0x8E, 0x25, 0xC3, 0x16, 0x03, 0xC3};
DeviceAddress cpuAddy = {0x28, 0xFF, 0x9D, 0x24, 0xC3, 0x16, 0x03, 0x7E};
DeviceAddress ambAddy = {0x28, 0xFF, 0xBF, 0xFE, 0xC2, 0x16, 0x03, 0xB4};

//to find addys
//DeviceAddress cpuAddy, pwrAddy, ambAddy;

/*----- Li-berry stuff ----*/
//fastLED
CRGB leds[NUM_LEDS];

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
  FastLED.addLeds<LPD8806, DATA_PIN, CLOCK_PIN, BRG>(leds, NUM_LEDS);
  
  //tempers begarn
  sensors.begin();

  endCharge();
}

/*----- Loops ----*/
void loop() {
  tempReads();
  
  if(readFlag){
    fanCalc();
    fanControl();
  }
  
  tempAnim();
  serialPrints();
 // EVERY_N_MILLISECONDS( 20 ) { gHue++; } //use this later for better temp animations
  delay(200);
  //findSensors();
}

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
    
    //let's inform the user and give temp sensors a min
    endFlash();
    
    //this runs the sleep program
    //it will exit with good reads for the sensors
    getfucked();
  }

  //set the flag to ignore bad sensor read
  if(cpusBad == 0 || pwrBad == 0 || ambBad == 0){
    readFlag = 1;
  }else{
    readFlag = 0;
  }
}

void fanCalc(){
  float inTemp = max(cpuTemp, pwrTemp);
  
  float trigTemp = ambTemp + SetHigh;
  float delta = inTemp - trigTemp;

  float outCalc;
  outCalc = fanSpeed + ( delta * SetScale);
  fanSpeed = (int) constrain(outCalc, 0, 255);
  
//  if(inTemp >= trigTemp + 1 || inTemp <= trigTemp - 1 ){ 
//    float outCalc;
//    outCalc = fanSpeed + ( delta * SetScale);
//    fanSpeed = (int) constrain(outCalc, 0, 255);
//  }
}

//need global fSpeed 0-255
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
    }

    //animate here for fun
    if(animBuddy){
      for(int i = 0; i<NUM_LEDS; i++){
        leds[i] = CHSV(gHue++, 255, 255);
        FastLED.show();
        fadeall();
        animBuddy = 0;
        delay(10);
      }  
    }else{
      for(int i = (NUM_LEDS)-1; i >= 0; i--){
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
    for( int j = 0; j < NUM_LEDS; j++) {
      leds[j] = CRGB::Purple;
    }
    
    FastLED.show();    
    delay(200);
    
    for( int j = 0; j < NUM_LEDS; j++) {
      leds[j] = CRGB::Black;
    }
    
    FastLED.show();    
    delay(200);
    b++;
  }
}

DEFINE_GRADIENT_PALETTE( lightGradnt ){
  0,       0,  0,   0,   //black
  100,   128,  0, 128,   //purp
  200,   220, 20,   0  //orang
};

void tempAnim(){
  CRGBPalette256 lightPal = lightGradnt;

  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(lightPal, fanSpeed); 
  }
  FastLED.show();
}
