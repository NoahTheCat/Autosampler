// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_MCP23017.h>

// This #include statement was automatically added by the Particle IDE.
#include <AdafruitDataLoggerRK.h>

// This #include statement was automatically added by the Particle IDE.
#include <oled-wing-adafruit.h>


#include <FreeSansOblique9pt7b.h>
#include <FreeSansBoldOblique18pt7b.h>

//Threading needed for RTC operation
SYSTEM_THREAD(ENABLED);

// Put Particle into semi-automatic mode - i.e. it wont try to connect to Wifi
// until you tell it to - easier to use if no local WiFi
SYSTEM_MODE(SEMI_AUTOMATIC);
boolean connectToCloud = false;     // flag to say the "connect" switch has been pressed



// Import the display code and instantiate with the keyword display
OledWingAdafruit display;

// Instantiate the Log handler and RTC for the Adalogger and RTC
SerialLogHandler logHandler;
RTCSynchronizer rtcSync;

// Initialise the SD card logging
#include "Particle.h"
#include "SdCardLogHandlerRK.h"
// The SD card CS pin on the Adafruit AdaLogger FeatherWing is D5.
const int SD_CHIP_SELECT = D5;
SdFat sd; //initialise the SdFat library

SdCardPrintHandler printToCard(sd, SD_CHIP_SELECT, SPI_FULL_SPEED);

size_t counter = 0;




//------------------------------------------------------
// Variables
//------------------------------------------------------

//Define the states of the Main machine
#define nullState 0
#define Repeat 1
#define Begin 2
#define Sampling 3
int fsm_state = nullState;  // initialise in null
String fsm_state_name;

//define the states of the Pump filling machine
#define nullState 0
#define startFlush 1
#define flushing 2
#define fillingBag 3
int pump_state = nullState;

//------------------------------------------------------
// Cloud Functions
int enterSamplingMode(String command);
int fillNextBag(String command);
bool singleRemoteFill = FALSE;
int webUpdateRepeatBegin(String command);





unsigned long startSampleTimer = millis();
//unsigned long endSampleTimer = millis();
unsigned long enterSampleTimeSet = 1500;
bool startSampling = FALSE;
int numberCPress = 0;

bool updateDisplay = TRUE;
bool enteringCase = TRUE;
int test = 0;

int unitSelectR = 3;
int unitSelectB = 3;

//Repeat times
int rDay = 0;
int rHour = 0;
int rMin = 2;

//begin in times
int bDay = 0;
int bHour = 0;
int bMin = 2;

//Bag filling variables
int bagToFill = 1;
int numOfBags = 12;

//Countdown Timer timing variables
int previousMillis = 0;

uint32_t now = 0;

uint32_t nextFillTime = 0;
String fillTimeString() {
    return  Time.timeStr(Time.now() + 86400*bDay + 3600*bHour + 60*bMin);
}

int nextFillDay = 0;
int nextFillHour = 0;
int nextFillMinute = 0;
int nextFillSecond = 0;

uint32_t untilNextFillTime = 9999; // there's no sampling queued up so initialise with a large number, i.e. 9999s seconds until next fill.
int untilNextFillDay =0;
int untilNextFillHour = 0;
int untilNextFillMinute = 0;
int untilNextFillSecond = 0;

//pump timing variables
int pumpTimer = 0;
bool flushBool = FALSE;
bool startFillingBag = FALSE;
int flushTime = 10000;          // time to flush the lines in ms, i.e. 1000 = 10s
int fillTime = 60000;           // time to fill the bag in ms, i.e. 60000 = 60s

//------------------------------------------------------
// Relay setup and variables
//------------------------------------------------------

// Instantiate the Relay board as mcp
Adafruit_MCP23017 mcp;

//#define  ON 1
//#define OFF 0
//#define address 0x20            // i2c slave address from 0x20 to 0x27

//unsigned char i;
//unsigned char variable_LOW;
//unsigned char variable_HIGH;
//unsigned int  mode_byte;        // 16bit unsigned variable

//------------------------------------------------------



void setup() {
    
    //Set the input pins for the switches
    pinMode(D7, INPUT_PULLDOWN);    // sets pin D7 as input with a default pulldown to GND
    attachInterrupt(D7, connect, RISING);  // Connect to WiFi+Cloud inturrupt - Pulse D7 to GND to trigger the connect function.
    
    
    //Particle.variable("Fill every_X_days", rDay);
    //Particle.variable("Fill every_X_hours", rHour);
    //Particle.variable("Fill every_X_minutes", rMin);
    
    //Particle.variable("Begin_in_X_days", rDay);
    //Particle.variable("Begin_in_X_hours", rHour);
    //Particle.variable("Begin_in_X_minutes", rMin);
    
    //Particle.variable("Start_Filling_Bag", startFillingBag);
    Particle.variable("Current_mode", fsm_state_name);
    Particle.variable("If_sampling_time_of_next_sample", fillTimeString);
    Particle.variable("Next_bag_to_fill", bagToFill);
    
    // register the cloud function
    Particle.function("Sampling_Mode", enterSamplingMode);
    Particle.function("Fill_Bag_Now", fillNextBag);
    Particle.function("New_Repeat_or_Begin_Period", webUpdateRepeatBegin);
    
    
    Serial.begin(9600); // setup USB serial to computer
	
	display.setup();    //Sets up the OLED - e.g. I2C address, button DIOs etc.
    rtcSync.setup();    //sets up the RTC - e.g. I2C address, etc. 
    
    splashScreen();
    
    // Set the initial display 
    display.setTextSize(1);
    display.setTextColor(WHITE);
    //nullDisplay();
    //display.display();
    
    //------------------------------------------------------
    // Initialise the Relay board
    //------------------------------------------------------
/*
    // Initialize the I2C bus if not already enabled
    if (!Wire.isEnabled()) {
        Wire.begin();
    }
    Wire.beginTransmission(0x20);
    Wire.write(0x00);             // A register
    Wire.write(0x00);             // set all of port A to outputs
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write((byte)0x01);       // B register
    Wire.write((byte)0x00);       // set all of port B to outputs
    Wire.endTransmission(); 
*/
    mcp.begin(0x20);
    init_relay();
    
    //turn on pump and flush valve for 100ms to test the relays work
    mcp.digitalWrite(5, HIGH);
    mcp.digitalWrite(7, HIGH);
    delay(100);
    relay_all_LOW();
    
  
}

void loop() {
	display.loop();     //button debouncer
	rtcSync.loop();     //sets the RTC from the cloud if the time is missing.

switch (fsm_state) {
  
    case nullState: // statements to run in nullState mode
        
        connectToCloudCheck(); //check if we should connect to Cloud
        
        if (enteringCase == TRUE){  //check if anything has changed and we need to update the display 
            nullDisplay();
            display.display();
            //Aargh();
            //display.clearDisplay();
            //display.setCursor(48,0);
            //display.print(test);
            //display.display();
            //test++;
            //delay(500);
            fsm_state_name = "Waiting...";
            enteringCase = FALSE;  //reset the flag to say "no need to update"
        }
        
        if (display.pressedA()){
            fsm_state = Repeat; //change state if you press A
            unitSelectR = 3;      //reset the unit selection so it always starts from Day
            enteringCase  = TRUE;
        } 
        if (display.pressedB()){
            fsm_state = Begin;  //change state if you press B 
            unitSelectB = 3;      //reset the unit selection so it always starts from Day
            enteringCase  = TRUE;
        } 
        
        if (display.pressedC()) {
            
            if (numberCPress == 0 ) {
                startSampleTimer = millis();
                numberCPress++;
            }
            else if (numberCPress < 4 ){
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    numberCPress++;
                }
                else {
                    numberCPress=1;
                    startSampleTimer = millis();
                }
            }
            else {
                numberCPress = 0;
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    fsm_state = Sampling;
                    pump_state = nullState;         //cancel the fill if you presss C 5 times
                    enteringCase = TRUE;
                }
            }
            
        };   
        
        if (singleRemoteFill == TRUE) fillBagIfConditionsMet();
        
        break;
  
    case Repeat:                                  //statements to run every time you're in Repeat mode
        
        connectToCloudCheck(); //check if we should connect to Cloud
        
        if (enteringCase == TRUE){  //check if anything has changed and we need to update the display 
            nullDisplay();
            displayRepeatHighlight();                   //make REPEAT all caps
            display.display();
            fsm_state_name = "Waiting...";
            enteringCase = FALSE;
        }
        
        if (display.pressedB()) {
            fsm_state = Begin;  //change state if you press B
            unitSelectR = 3;     //reset unit select so case starts at Day
            enteringCase  = TRUE;
        }
    
        if (display.pressedC()) {
            
            if (numberCPress == 0 ) {
                startSampleTimer = millis();
                numberCPress++;
            }
            else if (numberCPress < 4 ){
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    numberCPress++;
                }
                else {
                    numberCPress=1;
                    startSampleTimer = millis();
                }
            }
            else {
                numberCPress = 0;
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    fsm_state = Sampling;
                    pump_state = nullState;         //cancel the fill if you presss C 5 times
                    enteringCase = TRUE;
                }
            }
            
            unitSelectR++;  if(unitSelectR >3) unitSelectR=0;           //Loop through the units
            highlightUnit(unitSelectR);                //highlights the unit that will change
            display.display();
        };      
        //Serial.print(unitSelect);
        
        if (display.pressedA()) {
            updateRepeatTime();
            displayRepeatTime();
            display.display();
        }
        
        //if (display.pressedC() & display.pressedA()) {
        //    fsm_state = Sampling;
        //    unitSelectR = 3;         //reset unit select so case starts at Day
        //}
        if (singleRemoteFill == TRUE) fillBagIfConditionsMet();
        
    break;
    
    case Begin:
    
        connectToCloudCheck(); //check if we should connect to Cloud
        
        if (enteringCase == TRUE){  //check if anything has changed and we need to update the display 
            nullDisplay();
            displayBeginHighlight();                   //make BEGIN all caps
            display.display();
            //nextFillTime = Time.now() + 86400*bDay + 3600*bHour + 60*bMin;
            //fillTimeString = Time.timeStr(nextFillTime);
            fsm_state_name = "Waiting...";
            enteringCase = FALSE;
        }        
        
        //displayBeginHighlight();
        //display.display();
        
        if(display.pressedA()) {
            fsm_state = Repeat;
            unitSelectB = 3;         //reset unit select so case starts at Day
            enteringCase  = TRUE;
        }
        
        if (display.pressedC()) {
            
            if (numberCPress == 0 ) {
                startSampleTimer = millis();
                numberCPress++;
            }
            else if (numberCPress < 4 ){
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    numberCPress++;
                }
                else {
                    numberCPress=1;
                    startSampleTimer = millis();
                }
            }
            else {
                numberCPress = 0;
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    fsm_state = Sampling;
                    pump_state = nullState;         //cancel the fill if you presss C 5 times
                    enteringCase = TRUE;
                }
            }
            
            unitSelectB++;  if(unitSelectB >3) unitSelectB=0;           //Loop through the units
            highlightUnit(unitSelectB);                //highlights the unit that will change
            display.display();
        };  
        
        if (display.pressedB()) {
            updateBeginTime();
            //nextFillTime = Time.now() + 86400*bDay + 3600*bHour + 60*bMin;
            //fillTimeString = Time.timeStr(nextFillTime);
            //fillTimeString = String::format("frobnicator started at %s", nextFillTime.timeStr());
            displayBeginTime();
            display.display();
        }
        
        //if (display.pressedC() & display.pressedA()) {
        //    fsm_state = Sampling;
        //    unitSelectB = 3;         //reset unit select so case starts at Day
        //}
        
        if (singleRemoteFill == TRUE) fillBagIfConditionsMet();
        
    break;
    
    case Sampling:
 
        
        if (enteringCase == TRUE){  //check if anything has changed and we need to update the display 
            display.clearDisplay();
            
            display.setCursor(0,0);
            display.print("Rep:  d   h   m ");
            display.setCursor(24,0);
            display.print(rDay);              //write the new day
            display.setCursor(48,0);
            display.print(rHour);              //write the new hour
            display.setCursor(72,0);
            display.print(rMin);              //write the new minute

            display.setCursor(96,0);
            display.print(bagToFill);
            display.setCursor(108,0);
            display.print("/");
            display.print(numOfBags);
            
            
            //------------------------------------------------------
            // Calculate & Display the Timing Variables
            //------------------------------------------------------
            now = Time.now();
            
            //nextFillDay    = Time.day(now)    + bDay;
            //nextFillHour   = Time.hour(now)   + bHour;
            //nextFillMinute = Time.minute(now) + bMin; 
            //nextFillSecond = Time.second(now);
            nextFillTime = now + 86400*bDay + 3600*bHour + 60*bMin;
            
            //untilNextFillDay    = nextFillDay    - Time.day(now);
            //untilNextFillHour   = nextFillHour   - Time.hour(now);
            //untilNextFillMinute = nextFillMinute - Time.minute(now);
            //untilNextFillSecond = nextFillSecond - Time.second(now);
            untilNextFillTime = nextFillTime - Time.now();
            untilNextFillSecond = (untilNextFillTime % 60);
            untilNextFillMinute = (untilNextFillTime % 3600) /60;
            untilNextFillHour   = (untilNextFillTime % 86400) /3600;
            untilNextFillDay    = untilNextFillTime / 86400;
            displayUntilNextFillTimeLong();
            //------------------------------------------------------
            
            display.setCursor(0,22);
            display.print("Cancel = x5 fast"); 
            display.display();      //update the display 
            fsm_state_name = "Sampling";
            enteringCase = FALSE;  //reset the flag to say "no need to update"
        }
        
        
        //------------------------------------------------------
        // Countdown Timers to display the time til next fill
        //------------------------------------------------------
        
        unsigned long currentMillis = millis();
            
        // update the countdown every second if close to next fill and 10 sec when a long time from next fill
        //if(untilNextFillDay==0 & untilNextFillHour==0 & untilNextFillMinute<10) {
        if(untilNextFillTime<600) {  // if less than 10 minutes 10*60sec=600sec
            
            connectToCloudCheck(); //check if we should connect to Cloud - this is here so you have time to resest the device if something goes wrong
            
            if(currentMillis - previousMillis > 1000) {     //once a second
                previousMillis = currentMillis; //reset the timer
                
                untilNextFillTime = nextFillTime - Time.now();      //calculate the time in sec unitl the next bag fill
                
                if(untilNextFillTime <=0){    //if the bag fill is due
                    startFillingBag = TRUE;
                    nextFillTime = Time.now() + 86400*bDay + 3600*bHour + 60*bMin;
                    
                }
                else{
                    untilNextFillSecond = (untilNextFillTime % 60);
                    untilNextFillMinute = (untilNextFillTime % 3600) /60;
                    displayUntilNextFillTimeShort();
                }
                
                display.display();
                
               
            }
        }
        else{    
            if(currentMillis - previousMillis > 10000) {     //once every 10 seconds
                previousMillis = currentMillis; //reset the timer
                
                untilNextFillTime = nextFillTime - Time.now();
                untilNextFillSecond = (untilNextFillTime % 60);
                untilNextFillMinute = (untilNextFillTime % 3600) /60;
                untilNextFillHour   = (untilNextFillTime % 86400) /3600;
                untilNextFillDay    = untilNextFillTime / 86400;
                
                displayUntilNextFillTimeLong();
                display.display();
            }
        }
        
        //------------------------------------------------------
        // Button C handler - i.e. cancel if pressed repeadedly
        //------------------------------------------------------
        
        if (display.pressedC()) {
            
            
            if (numberCPress == 0 ) {
                startSampleTimer = millis();
                numberCPress++;
            }
            else if (numberCPress < 4 ){
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    numberCPress++;
                }
                else {
                    numberCPress=1;
                    startSampleTimer = millis();
                }
            }
            else {
                numberCPress = 0;
                if ((millis()-startSampleTimer) < enterSampleTimeSet) {
                    relay_all_LOW();
                    fsm_state = nullState;
                    pump_state = nullState;                 //cancel the fill if you presss C 5 times
                    enteringCase = TRUE;
                    untilNextFillTime = 9999; //set the time of the next fill to a big number so that the one-off bag filling conditions are met
                }
            }
            
        };
        
        //------------------------------------------------------
        // Bag Filling timers, turn on pumps, flush, fill, exit.
        //------------------------------------------------------
        
        fillBagIfConditionsMet();
        /*
        //if we have the command to fill a bag, AND we have time, AND we have empty bags. THEN start.
        if(startFillingBag == TRUE && untilNextFillTime >100 && (bagToFill <= numOfBags) ) pump_state = startFlush;
        
        switch (pump_state) {
        
            case nullState:
            break;
            
            case startFlush:
                
                display.fillRect(0,0, 128,8, 0);//erase the top line.
                display.setCursor(0,0);
                display.print("Flushing");
                display.display();
                
                // I2C writes to turn on pump and open flush valve
                //turn on Pump
                relay_control(16, HIGH);
                //open exhaust valve
                relay_control(14, HIGH);
                
                startFillingBag = FALSE;        //clear the flag to start the pump sequence
                pumpTimer = millis();           //start the pump's timer
                pump_state = flushing;          //go to the flushing case 
                
            break;
            
            case flushing:
                if((millis() - pumpTimer) >= 10000) {    //after 10 seconds of flushing
                    
                    display.fillRect(0,0, 128,8, 0);//erase the top line.
                    display.setCursor(0,0);
                    display.print("Filling Bag: ");
                    display.print(bagToFill);
                    display.display();
                    
                    //CLOSE exhaust valve
                    relay_control(14, LOW);

                    //OPEN bagToFill valve
                    relay_control(bagToFill, HIGH);
                    
                    // Tell the console we're filling a bog - NO! CAN BE BLOCKING!
                   // Particle.publish("Filling Bag", String::format("%d",bagToFill), PRIVATE);
                    
                    // update the bagToFill to say this bag is full - if the sampling is cancelled this bag is still considered
                    // "full" as it has been opened.
                    //need to use (bagToFill-1) in the next case
                    bagToFill++; 
                    
                    pumpTimer = millis();       //reset timer
                    pump_state = fillingBag;     //move to the next state
                }
            break;
            
            case fillingBag:
                if((millis() - pumpTimer) >= 60000) {    //after 60 seconds of filling
                    
                    display.fillRect(0,0, 128,8, 0);//erase the top line.
                    display.setCursor(0,0);
                    display.setCursor(0,0);
                    display.print("Done");
                    display.display();
                    
                    //CLOSE bagToFill valve
                    relay_control((bagToFill-1), LOW);
                    //turn pump OFF
                    relay_control(16, LOW);
                    //double check we have everything off
                    relay_all_LOW();
                    
                    //save fill to SD card
                    
                    bagToFill++; //increment the bag number
                    pump_state = nullState;     //reset the pump to null.
                }
            break;
        }    
        */
    break;
}

}








//************************************************************************************************************
//------------------------------------------------------------------------------------------------------------
//************************************************************************************************************
//------------------------------------------------------------------------------------------------------------

// FUNCTIONS

//------------------------------------------------------------------------------------------------------------
//************************************************************************************************************
//------------------------------------------------------------------------------------------------------------
//************************************************************************************************************


//------------------------------------------------------
// Debug Function
//------------------------------------------------------
int Aargh(){
    display.clearDisplay();
    display.setCursor(0,11);               //sets the cursor position
    display.print("HOW AM I HERE???");
    display.display();
}


//============================================================================================================================================
//============================================================================================================================================
// Display Functions
//============================================================================================================================================
//============================================================================================================================================



//------------------------------------------------------
// Set the display splashscreen
//------------------------------------------------------

int splashScreen(){
    
    display.clearDisplay();
    display.setFont(&FreeSansBoldOblique18pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,31);
    display.print("Noah");
    display.display();
    display.setFont();
    delay(2000);
}

//------------------------------------------------------
// This function displays the null items
//------------------------------------------------------
int nullDisplay(){
    display.clearDisplay();
    
    display.setCursor(0,0);               //sets the cursor position
    display.print("Repeat:   d   h   m");
    displayRepeatTime();
    
    display.setCursor(0,11);
    display.print("Begin:    d   h   m");
    displayBeginTime();
    
    display.setCursor(0,22);
    display.print("Units / Start=x5 fast");             
    //display.display(); // actually display all of the above
}


//------------------------------------------------------
// This function highlights REPEAT
//------------------------------------------------------
int displayRepeatHighlight(){
    display.setCursor(0,0);                      //sets the cursor position
    display.fillRect(0,0, 35, 10, 0);  //erase "Repeat"
    display.println("REPEAT");       
    display.fillRect(0,11, 35, 11, 0);  //erase "BEGIN"
    display.setCursor(0,11);
    display.println("Begin");    
    //display.display();
}

//------------------------------------------------------
// This function highlights BEGIN
//------------------------------------------------------
int displayBeginHighlight(){
    display.setCursor(0,0);                      //sets the cursor position
    display.fillRect(0,0, 35, 10, 0);  //erase "REPEAT"
    display.println("Repeat");       
    display.fillRect(0,11, 35, 11, 0);  //erase "begin"
    display.setCursor(0,11);
    display.println("BEGIN");    
    //display.display();
}

//------------------------------------------------------
// this function makes the currently selected unit (m/h/d) a capital letter (M/H/D)
//------------------------------------------------------
int highlightUnit(int unitSelect){
    /* this function makes the currently selected unit (m/h/d) a capital letter (M/H/D)*/
  int y_val = 0;                        //chose the correct row to update
  if (fsm_state == Repeat) y_val=0;
  if (fsm_state == Begin) y_val=11;

  resetAllUnits(y_val);                 //reset all units to lower case
  
  if (unitSelect == 0){
    display.fillRect(48,y_val, 78,8, 0);//erase "Preset"
    resetAllUnits(y_val);
    display.fillRect(60,y_val, 5,8, 0);//erase d
    if (fsm_state == Repeat) displayRepeatTime();
    if (fsm_state == Begin) displayBeginTime();
    display.setCursor(60,y_val);
    display.print("D");                 //write D
    //display.display();
  }

  if (unitSelect == 1){
    display.fillRect(84,y_val, 5,8, 0);//erase h
    display.setCursor(84,y_val);
    display.print("H");                 //write H
    //display.display();
  }

  if (unitSelect == 2){
    display.fillRect(108,y_val, 5,8, 0);//erase m
    display.setCursor(108,y_val);
    display.print("M");                 //write D
    //display.display();
  } 

  if (unitSelect == 3){
    display.fillRect(48,y_val, 78,8, 0);//erase all
    display.setCursor(48,y_val);
    display.print("Time Set");            //write Preset
    //display.display();
  }
}

//------------------------------------------------------
// This function resets all timer displays to the defaults
//------------------------------------------------------
int resetAllUnits(int y_val){
    /*This function resets all timer displays to the defaults*/
  if (fsm_state == Repeat) y_val=0;
  if (fsm_state == Begin) y_val=11;

  display.fillRect(60,y_val, 5,5, 0); //erase d/D
  display.setCursor(60,y_val);
  display.print("d");

  display.fillRect(84,y_val, 5,5, 0); //erase h/H
  display.setCursor(84,y_val);
  display.print("h");  

  display.fillRect(108,y_val, 5,5, 0); //erase m/M
  display.setCursor(108,y_val);
  display.print("m");
}

//------------------------------------------------------
// This function displays the current REPEAT time
//------------------------------------------------------
int displayRepeatTime(){
    //Erase the current values
    display.fillRect(48,0, 12,8, 0); //erase d/D value
    display.fillRect(72,0, 12,8, 0); //erase m/M value
    display.fillRect(96,0, 12,8, 0); //erase h/H value
    
    //Write the new values
    display.setCursor(48,0);
    display.print(rDay);              //write the new day
    display.setCursor(72,0);
    display.print(rHour);              //write the new hour
    display.setCursor(96,0);
    display.print(rMin);              //write the new minute
    //display.display();                // force an update of the display
    
}

//------------------------------------------------------
// This function displays the current BEGIN time
//------------------------------------------------------
int displayBeginTime(){
    //Erase the current values
    display.fillRect(48,11, 12,8, 0); //erase d/D value
    display.fillRect(72,11, 12,8, 0); //erase m/M value
    display.fillRect(96,11, 12,8, 0); //erase h/H value

    //Write the new values
    display.setCursor(48,11);
    display.print(bDay);              //write the new day
    display.setCursor(72,11);
    display.print(bHour);              //write the new hour
    display.setCursor(96,11);
    display.print(bMin);              //write the new minute
    //display.display();
    
}

//------------------------------------------------------
// This function updates the current REPEAT time
//------------------------------------------------------
int updateRepeatTime(){
    if (unitSelectR == 0) rDay++;
    if (rDay > 10) rDay=0;

    if (unitSelectR == 1) rHour++;
    if (rHour > 24) rHour=0;  

    if (unitSelectR == 2) rMin=rMin+5;
    if (rMin > 60) rMin=0;
}

//------------------------------------------------------
// This function updates the current BEGIN time
//------------------------------------------------------
int updateBeginTime(){
    if (unitSelectB == 0) bDay++;
    if (bDay > 10) bDay=0;

    if (unitSelectB == 1) bHour++;
    if (bHour > 24) bHour=0;  

    if (unitSelectB == 2) bMin=bMin+5;
    if (bMin > 60) bMin=0;
}

//------------------------------------------------------
//
//------------------------------------------------------
int displayUntilNextFillTimeLong(){
    
    if (pump_state == nullState){
        //display top line - this gets cleared by the bag filling status display
        display.fillRect(0,0, 128,8, 0);//erase the top line.
        display.setCursor(0,0);
        display.print("Rep:  d   h   m ");
        display.setCursor(24,0);
        display.print(rDay);              //write the new day
        display.setCursor(48,0);
        display.print(rHour);              //write the new hour
        display.setCursor(72,0);
        display.print(rMin);              //write the new minute

        display.setCursor(96,0);
        display.print(bagToFill);
        display.setCursor(108,0);
        display.print("/");
        display.print(numOfBags);
    }
    
    //start with the actual update
    display.setCursor(0,11);
    display.print("Nxt Bag:  d   h   m");
    //Erase the current values
    display.fillRect(48,11, 12,8, 0); //erase d/D value
    display.fillRect(72,11, 12,8, 0); //erase m/M value
    display.fillRect(96,11, 12,8, 0); //erase h/H value

    //Write the new values
    display.setCursor(48,11);
    display.print(untilNextFillDay);              //write the new day
    display.setCursor(72,11);
    display.print(untilNextFillHour);              //write the new hour
    display.setCursor(96,11);
    display.print(untilNextFillMinute);              //write the new minute
    //display.display();
    
}

//------------------------------------------------------
//
//------------------------------------------------------
int displayUntilNextFillTimeShort(){
    
    if (pump_state == nullState){
        //display top line - this gets cleared by the bag filling status display
        display.fillRect(0,0, 128,8, 0);//erase the top line.
        display.setCursor(0,0);
        display.print("Rep:  d   h   m ");
        display.setCursor(24,0);
        display.print(rDay);              //write the new day
        display.setCursor(48,0);
        display.print(rHour);              //write the new hour
        display.setCursor(72,0);
        display.print(rMin);              //write the new minute

        display.setCursor(96,0);
        display.print(bagToFill);
        display.setCursor(108,0);
        display.print("/");
        display.print(numOfBags);
    }
    
    //start with the actual update
    
    
    //Erase the current values
    display.fillRect(48,11, 80,8, 0); //erase old time

    //Write the new values
    display.setCursor(50,11);
    display.print(untilNextFillMinute);              //write the new minute
    //display.setCursor(62,11);
    display.print(" m ");
    //display.setCursor(70,11);
    display.print(untilNextFillSecond);              //write the new second
    display.print(" s");
}


//============================================================================================================================================
//============================================================================================================================================
// Relay Functions
//============================================================================================================================================
//============================================================================================================================================

//------------------------------------------------------
//This function initialises all DIO to outputs
//------------------------------------------------------
void init_relay(){
    
    for (int i=0; i<=16; i++){
        mcp.pinMode(i, OUTPUT);
    }
}

//------------------------------------------------------
//This function turns all the relays OFF
//------------------------------------------------------
void relay_all_LOW(){
    
    for (int i=0; i<=16; i++){
        mcp.digitalWrite(i, LOW);
    }
}

//------------------------------------------------------------
// This function is a wrapper to re-number the relays to sensible values 
//------------------------------------------------------------

void relay_control(unsigned int channel, unsigned char mode){   

     switch (channel) { case 16 : channel =  7; break;
                        case 15 : channel =  6; break;
                        case 14 : channel =  5; break;
                        case 13 : channel =  4; break;
                        case 12 : channel =  3; break;
                        case 11 : channel =  2; break;
                        case 10 : channel =  1; break;
                        case  9 : channel =  0; break;
                        case  8 : channel = 15; break;
                        case  7 : channel = 14; break;
                        case  6 : channel = 13; break;
                        case  5 : channel = 12; break;
                        case  4 : channel = 11; break;
                        case  3 : channel = 10; break;
                        case  2 : channel =  9; break;
                        case  1 : channel =  8; break; }

     mcp.digitalWrite(channel, mode);
  
}

//------------------------------------------------------------
//------------------------------------------------------------

void fillBagIfConditionsMet(){
    //if we have the command to fill a bag, AND we have time, AND we have empty bags. THEN start.
        if(startFillingBag == TRUE && untilNextFillTime >100 && (bagToFill <= numOfBags) ) pump_state = startFlush;
        
        switch (pump_state) {
        
            case nullState:
            break;
            
            case startFlush:
                
                display.fillRect(0,0, 128,8, 0);//erase the top line.
                display.setCursor(0,0);
                display.print("Flushing");
                display.display();
                
                // I2C writes to turn on pump and open flush valve
                //turn on Pump
                relay_control(16, HIGH);
                //open exhaust valve
                relay_control(14, HIGH);
                
                startFillingBag = FALSE;        //clear the flag to start the pump sequence
                pumpTimer = millis();           //start the pump's timer
                pump_state = flushing;          //go to the flushing case 
                
            break;
            
            case flushing:
                if((millis() - pumpTimer) >= flushTime) {    //after 10 seconds of flushing
                    
                    display.fillRect(0,0, 128,8, 0);//erase the top line.
                    display.setCursor(0,0);
                    display.print("Filling Bag: ");
                    display.print(bagToFill);
                    display.display();
                    
                    //CLOSE exhaust valve
                    relay_control(14, LOW);

                    //OPEN bagToFill valve
                    relay_control(bagToFill, HIGH);
                    printToCard.print(Time.timeStr());
                    printToCard.printlnf(", %d" ,bagToFill);
                    
                    // Tell the console we're filling a bog - NO! CAN BE BLOCKING!
                   // Particle.publish("Filling Bag", String::format("%d",bagToFill), PRIVATE);
                    
                    // update the bagToFill to say this bag is full - if the sampling is cancelled this bag is still considered
                    // "full" as it has been opened.
                    //need to use (bagToFill-1) in the next case
                    bagToFill++; 
                    
                    pumpTimer = millis();       //reset timer
                    pump_state = fillingBag;     //move to the next state
                }
            break;
            
            case fillingBag:
                if((millis() - pumpTimer) >= fillTime) {    //after 60 seconds of filling
                    
                    display.fillRect(0,0, 128,8, 0);//erase the top line.
                    display.setCursor(0,0);
                    display.setCursor(0,0);
                    display.print("Done");
                    display.display();
                    
                    //CLOSE bagToFill valve
                    relay_control((bagToFill-1), LOW);
                    //turn pump OFF
                    relay_control(16, LOW);
                    //double check we have everything off
                    relay_all_LOW();
                    
                    //save fill to SD card
                    
                    //bagToFill++; //increment the bag number
                    pump_state = nullState;     //reset the pump to null.
                    singleRemoteFill = FALSE;   //reset the flag to start a remote fill
                }
            break;
        }
}



//============================================================================================================================================
//============================================================================================================================================
// Web Functions
//============================================================================================================================================
//============================================================================================================================================


//------------------------------------------------------------
//function called by the "Connect to wifi" inturrupt, sets correct flag
//------------------------------------------------------------
void connect() {
    connectToCloud = true;
}

//------------------------------------------------------------
// This function checks if the Connect to Cloud switch interrupt has been triggered and connects to the cloud if it has 
//------------------------------------------------------------
void connectToCloudCheck(){
    if(connectToCloud == true && Particle.connected() == false) {
         Particle.connect();
         Serial.println("Connected to cloud");
     }
}



//------------------------------------------------------------
// Web function to Start/Stop the sampling mode
// automagically gets called upon a matching POST request 
// e.g. if you press the button on the matching function on the console
//------------------------------------------------------------
int enterSamplingMode(String command)
{
  // look for the matching argument "Start" <-- max of 64 characters long
  if(command == "Start") {
    fsm_state = Sampling;
    enteringCase = TRUE;
    return 1;
    
  }
  else if(command == "Stop") {
      fsm_state = nullState;
      enteringCase = TRUE;
      return 1;
  }
  else return -1;
}

//------------------------------------------------------------
// Web function to take a single sample
// automagically gets called upon a matching POST request 
// e.g. if you press the button on the matching function on the console
//------------------------------------------------------------
int fillNextBag(String command){
    if(command == "Fill_Bag_Now") {
        singleRemoteFill = TRUE;
        startFillingBag = TRUE;
        //fillBagIfConditionsMet();
        return 1;
    }
    else return -1;
}

//------------------------------------------------------------
// Web function to update the BEGIN and REPEAT times
// automagically gets called upon a matching POST request 
// e.g. if you press the button on the matching function on the console
//------------------------------------------------------------
int webUpdateRepeatBegin(String command){
    // these lines find the locations of the semicolons (;) that separate out the day;hour;minutes
    int semicolonIndex = command.indexOf(';');
    int secondSemicolonIndex = command.indexOf(';', semicolonIndex + 1);
    int thirdSemicolonIndex = command.indexOf(';', secondSemicolonIndex + 1);
    
    if(command.substring(0, semicolonIndex) == "Update_Repeat" ){
        // these lines pull the data out into separte strings
        String newDayValue = command.substring(semicolonIndex + 1, secondSemicolonIndex);
        String newHourValue = command.substring(secondSemicolonIndex + 1, thirdSemicolonIndex);
        String newMinuteValue = command.substring(thirdSemicolonIndex + 1); // To the end of the string
        
        rDay = newDayValue.toInt();
        rHour = newHourValue.toInt();
        rMin = newMinuteValue.toInt();
        
        displayRepeatTime();
        display.display();
        return 1;
    }
    else if(command.substring(0, semicolonIndex) == "Update_Begin" ){
        // these lines pull the data out into separte strings
        String newDayValue = command.substring(semicolonIndex + 1, secondSemicolonIndex);
        String newHourValue = command.substring(secondSemicolonIndex + 1, thirdSemicolonIndex);
        String newMinuteValue = command.substring(thirdSemicolonIndex + 1); // To the end of the string
        
        bDay = newDayValue.toInt();
        bHour = newHourValue.toInt();
        bMin = newMinuteValue.toInt();
        
        displayBeginTime();
        display.display();
        return 2;
    }
    else return -1;
    

    
}


