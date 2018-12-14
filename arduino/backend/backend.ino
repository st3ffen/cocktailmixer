/*
  Arduino Yún Bridge example

  This example for the YunShield/Yún shows how 
  to use the Bridge library to access the digital and
  analog pins on the board through REST calls.
  It demonstrates how you can create your own API when
  using REST style calls through the browser.

  Possible commands created in this shetch:

  "/ardunio/fedderA/1/bootleA/100......

  "/arduino/digital/13"     -> digitalRead(13)
  "/arduino/digital/13/1"   -> digitalWrite(13, HIGH)
  "/arduino/analog/2/123"   -> analogWrite(2, 123)
  "/arduino/analog/2"       -> analogRead(2)
  "/arduino/mode/13/input"  -> pinMode(13, INPUT)
  "/arduino/mode/13/output" -> pinMode(13, OUTPUT)

  This example code is part of the public domain

  http://www.arduino.cc/en/Tutorial/Bridge

*/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include "AccelStepper.h" //Stepper
#include "HX711.h"

// Listen to the default port 5555, the Yún webserver
// will forward there all the HTTP requests you send
BridgeServer server;
//Pins
const int HOME_SWITCH_BEGIN = 12;
const int HOME_SWITCH_END = 11;
const int Y_AXE_DOWN = 7;
const int Y_AXE_UP = 6;
const int VALVE_A = 5;
const int VALVE_B = 4;
//Stepper
const double STEPPER_INIT_MAX_SPEED = 100.0;
const double STEPPER_INIT_ACCELERATION = 100.0;
const double STEPPER_PROD_MAX_SPEED = 1400.0;
const double STEPPER_PROD_ACCELERATION = 1000.0;
//Mixer
const int FEEDER_POSITION_A = 6650;
const int FEEDER_POSITION_B = 11200;
const int FEEDER_POSITION_C = 15900;
const int FEEDER_POSITION_D = 21150;
const int Y_AXE_TIME = 2500;
const int FEEDER_WAIT_TIME = 4000;

//other
AccelStepper stepperX(AccelStepper::DRIVER, 9, 8);
HX711 scale(A0, A1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
                                  
void setup() {
  Serial.begin(9600);
  //Pins
  pinMode(HOME_SWITCH_BEGIN, INPUT_PULLUP);
  pinMode(HOME_SWITCH_END, INPUT_PULLUP);
  pinMode(Y_AXE_DOWN, OUTPUT);
  pinMode(Y_AXE_UP, OUTPUT);
  pinMode(VALVE_A, OUTPUT);
  pinMode(VALVE_B, OUTPUT);
  digitalWrite(Y_AXE_UP, HIGH);
  digitalWrite(Y_AXE_DOWN, HIGH);
  digitalWrite(VALVE_A, HIGH);
  digitalWrite(VALVE_B, HIGH);

  //display
    // initialize the LCD
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.setCursor(0, 0); // top left

  lcd.print("DringMixer 3000");
  
  //homeing
  lcd.setCursor(0, 1); // bottom left
  lcd.print("Suche Null Punkt");
  homeingStepper();
  //y axe

  driveYaxe(Y_AXE_DOWN);
  // Bridge startup
  Serial.println("Start Bridge");
  lcd.setCursor(0, 1); // bottom left
  lcd.print("Starte Bridge   ");
  Bridge.begin();

  //Waage
  lcd.setCursor(0, 1); // bottom left
  lcd.print("Starte Waage    ");
  scale.set_scale(-905.286666667); // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();                    // reset the scale to 0
  scale.power_down();             // put the ADC in sleep mode
  lcd.setCursor(0, 1); // bottom left
  lcd.print("bereit          ");
  server.listenOnLocalhost();
  server.begin();
}

void loop() {
  // Get clients coming from server
  BridgeClient client = server.accept();

  // There is a new client?
  if (client) {
    // Process request
    process(client);

    // Close connection and free resources.
    client.stop();
  }

  delay(50); // Poll every 50ms
}

void process(BridgeClient client) {
  lcd.setCursor(0, 1); // bottom left
  lcd.print("Bitte Warten... ");
  while(client.available()){
    String ingredient = client.readStringUntil('/');
    ingredient.toLowerCase();
    int value = client.readStringUntil('/').toInt();
    client.println(ingredient);
    client.println(value);
    if(true){
      if(ingredient.equals("home")){
        goToPosition(0);
      }
      if(ingredient.equals("feedera")){
        goToPosition(FEEDER_POSITION_A);
        takeFromFeeder(value);
      }
      if(ingredient.equals("feederb")){
        goToPosition(FEEDER_POSITION_B);
        takeFromFeeder(value);
      }
      if(ingredient.equals("feederc")){
        goToPosition(FEEDER_POSITION_C);
        takeFromFeeder(value);
      }
      if(ingredient.equals("feederd")){
        goToPosition(FEEDER_POSITION_D);
        takeFromFeeder(value);
      }
      if(ingredient.equals("up")){
        driveYaxe(Y_AXE_UP);
      }
      if(ingredient.equals("down")){
        driveYaxe(Y_AXE_DOWN);
      }
      if(ingredient.equals("scale")){
        scaleUntil(value);
      }
      if(ingredient.equals("valveatest")){
        digitalWrite(VALVE_A, LOW);
        delay(2000);
        digitalWrite(VALVE_A, HIGH);
      }
      if(ingredient.equals("valvebtest")){
        digitalWrite(VALVE_B, LOW);
        delay(2000);
        digitalWrite(VALVE_B, HIGH);
      }
      if(ingredient.equals("valvea")){
        valveUntil(value, VALVE_A);
      }
      if(ingredient.equals("valveb")){
        valveUntil(value, VALVE_B);
      }
    }
  }
  lcd.setCursor(0, 0); // bottom left
  lcd.print("DringMixer 3000");
  
  // Send feedback to client
  client.print(F("DONE"));
 
}

void digitalCommand(BridgeClient client) {
  int pin, value;

  // Read pin number
  pin = client.parseInt();

  // If the next character is a '/' it means we have an URL
  // with a value like: "/digital/13/1"
  if (client.read() == '/') {
    value = client.parseInt();
    digitalWrite(pin, value);
  } else {
    value = digitalRead(pin);
  }

  // Send feedback to client
  client.print(F("Pin D"));
  client.print(pin);
  client.print(F(" set to "));
  client.println(value);

  // Update datastore key with the current pin value
  String key = "D";
  key += pin;
  Bridge.put(key, String(value));
}

void analogCommand(BridgeClient client) {
  int pin, value;

  // Read pin number
  pin = client.parseInt();

  // If the next character is a '/' it means we have an URL
  // with a value like: "/analog/5/120"
  if (client.read() == '/') {
    // Read value and execute command
    value = client.parseInt();
    analogWrite(pin, value);

    // Send feedback to client
    client.print(F("Pin D"));
    client.print(pin);
    client.print(F(" set to analog "));
    client.println(value);

    // Update datastore key with the current pin value
    String key = "D";
    key += pin;
    Bridge.put(key, String(value));
  } else {
    // Read analog pin
    value = analogRead(pin);

    // Send feedback to client
    client.print(F("Pin A"));
    client.print(pin);
    client.print(F(" reads analog "));
    client.println(value);

    // Update datastore key with the current pin value
    String key = "A";
    key += pin;
    Bridge.put(key, String(value));
  }
}

void modeCommand(BridgeClient client) {
  int pin;

  // Read pin number
  pin = client.parseInt();

  // If the next character is not a '/' we have a malformed URL
  if (client.read() != '/') {
    client.println(F("error"));
    return;
  }

  String mode = client.readStringUntil('\r');

  if (mode == "input") {
    pinMode(pin, INPUT);
    // Send feedback to client
    client.print(F("Pin D"));
    client.print(pin);
    client.print(F(" configured as INPUT!"));
    return;
  }

  if (mode == "output") {
    pinMode(pin, OUTPUT);
    // Send feedback to client
    client.print(F("Pin D"));
    client.print(pin);
    client.print(F(" configured as OUTPUT!"));
    return;
  }

  client.print(F("error: invalid mode "));
  client.print(mode);
}

void goToPosition(int position){
  Serial.println("goToPosition:" );
  Serial.print(position);
  stepperX.moveTo(position);  
  while (stepperX.distanceToGo() != 0 && (digitalRead(HOME_SWITCH_BEGIN) == 1) && (digitalRead(HOME_SWITCH_END) == 1)) {  // Make the Stepper move CCW until the switch is activated   
    Serial.println(stepperX.currentPosition());
    stepperX.run();  // Start moving the stepper
  }
}

void homeingStepper(){
  long initial_homing=-1;  // Used to Home Stepper at startup
     delay(5);  // Wait for EasyDriver wake up

   //  Set Max Speed and Acceleration of each Steppers at startup for homing
  stepperX.setMaxSpeed(STEPPER_INIT_MAX_SPEED);      // Set Max Speed of Stepper (Slower to get better accuracy)
  stepperX.setAcceleration(STEPPER_INIT_ACCELERATION);  // Set Acceleration of Stepper
 

// Start Homing procedure of Stepper Motor at startup

  Serial.print("Stepper is Homing . . . . . . . . . . . ");

  while (digitalRead(HOME_SWITCH_BEGIN)) {  // Make the Stepper move CCW until the switch is activated   
    stepperX.moveTo(initial_homing);  // Set the position to move to
    initial_homing--;  // Decrease by 1 for next move if needed
    stepperX.run();  // Start moving the stepper
    delay(5);
  }

  stepperX.setCurrentPosition(0);  // Set the current position as zero for now
  stepperX.setMaxSpeed(STEPPER_INIT_MAX_SPEED);      // Set Max Speed of Stepper (Slower to get better accuracy)
  stepperX.setAcceleration(STEPPER_INIT_ACCELERATION);  // Set Acceleration of Stepper
  initial_homing=1;

  while (!digitalRead(HOME_SWITCH_BEGIN)) { // Make the Stepper move CW until the switch is deactivated
    stepperX.moveTo(initial_homing);  
    stepperX.run();
    initial_homing++;
    delay(5);
  }
  
  stepperX.setCurrentPosition(0);
  Serial.println("Homing Completed");
  Serial.println("");
  stepperX.setMaxSpeed(STEPPER_PROD_MAX_SPEED);      // Set Max Speed of Stepper (Faster for regular movements)
  stepperX.setAcceleration(STEPPER_PROD_ACCELERATION);  // Set Acceleration of Stepper
}
void driveYaxe(int zylinder){
  Serial.println("Zylinder wird gefahren");
  digitalWrite(zylinder, LOW);
  delay(Y_AXE_TIME);
  digitalWrite(zylinder, HIGH);
  delay(100);
}
void takeFromFeeder(int times){
  for (int i=0; i < times; i++){
    driveYaxe(Y_AXE_UP);
    delay(FEEDER_WAIT_TIME);
    driveYaxe(Y_AXE_DOWN);
  }
}

void scaleUntil(int weight){
  scale.power_up();
  Serial.println((int) scale.get_units());
  Serial.println(weight);
  while(true){
    if(((int) scale.get_units(10)) > weight){
      scale.power_down(); 
      driveYaxe(Y_AXE_UP);
      driveYaxe(Y_AXE_DOWN);
      break;
    }
  }
}

void valveUntil(int weight, int valve){
  scale.power_up();
  lcd.setCursor(0, 0); // bottom left
  lcd.print("Vorher: ");
  int vorher = (int) scale.get_units();
  lcd.print(vorher);
  delay(2000);
  digitalWrite(valve, LOW);
  while(true){
    lcd.setCursor(0, 1);
    lcd.print("Aktuell: ");
    lcd.print((int) scale.get_units(5));
    if(((int) scale.get_units(5)) > (weight + vorher)){
      scale.power_down(); 
      digitalWrite(valve, HIGH);
      break;
    }
  }
}
