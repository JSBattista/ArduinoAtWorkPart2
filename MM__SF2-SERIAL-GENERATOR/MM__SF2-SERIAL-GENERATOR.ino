/*
  The sole purpose is to provide an incremental count to 2 machines. The count is used as part os Serial number generation
  whereby it's tacked onto the Julian date. For example:
  213100001 where 21 is year, 310 is Nth date of year, and 0001 would be the count of units produced. The entire
  Serial Number would look like XR700ERCD213100001X

  EEPROM is used to store the count for the day, where there are 31 different addresses for each day, not all of which 
  need to be used in a given month. When the clock rolls over into a new day, the inactive value on the structure
  representing the day, being false, will trigger the counter to reset back to 1, and also set all other days to inactive.
  
  Each "day" represented by a memory position has a field that holds the counter value. The counter value may be updated 
  hundreds of times in a day, so the day's counter address value is randomly generated and can "land" in an allowable area 
  for counter values. It's intended to be different each day so as not to create ruts in the EEPROM. 

  https://playground.arduino.cc/Main/DS1302RTC/
  https://www.allelectronics.com/mas_assets/media/allelectronics2018/spec/ME-48.pdf
  https://www.arduino.cc/en/Tutorial/BuiltInExamples/SerialEvent
  https://forum.arduino.cc/t/serial-input-basics/278284/2
  https://randomnerdtutorials.com/arduino-eeprom-explained-remember-last-led-state/
  https://www.geeksforgeeks.org/find-the-day-number-in-the-current-year-for-the-given-date/


  Notes:
  Due to possible needs of the systems using this device, serial handling is all contained in respectice serial event
  handlers so each has their own code. 

  
*/
#include <Wire.h>
#include <ds3231.h>
#include <EEPROM.h>
#include <Adafruit_SSD1306.h>
struct CAL_NDAY {
  bool active;
  unsigned int counterADDR;       
  byte stg[8];  // Storage in case it's needed later
};
struct ts t;              // Used for RTC module
int maxADDR = 4096;       // !!! This is based on the hardware memory size !!!!
int counterADDR_MIN = 400;  // The random locations of counters will start at this address value. DO NOT start where the day structures are recorded.
int dstADDR = 360;  // This is where the boolean value for Daylight Savings Time on or off is kept. Keep it away from other areas.
int GENLED = 4;     // General or red LED for warnings, indication, etc. 
int DSTLED = 5;     // This LED is ON when Daylight Savings is on.
int ser1LED = 6;    // Lights when Serial 1 is being served
int ser2LED = 7;    // LED for Serial 2
int ser3LED = 8;    // LED for Serial 2
bool lockout;       // Prevents possible race condition. Serials will wait for this to be false.
bool DST;           // Daylight Savings time on or off. Less risky than trying to reset time in the RTC module without supervision
                    // DST starts second Sunday in March and ends first Sunday in November.
int currentglobalcount = 0;   // The count for the 00-24 span day
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
void showMessage(String s, int fs) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0,0);
            display.println("   STARFISH II");  
            display.setCursor(0,8);
            display.setTextSize(fs);
            display.setTextColor(WHITE);
            display.println(s);
            display.display();
}
void showTimeString(String s, int c){
            display.clearDisplay();
            display.setCursor(0,0);
            display.println("   STARFISH II");  
            display.setCursor(0,8);
            display.println("SERIAL GENERATOR");
            display.setCursor(0,16);
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.println(s);
            display.setCursor(0,24);
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.print("CURRENT COUNT:");
            display.println(c);
            display.display();
}
// This will set all non-active fields of the days to false, except the one that is actually the current day. 
// intended to happen every time there is a new day.
// The trigger for this is when a day is sought but found to be false, meaning the RTC has progressed and
// it's an actual new day. 
// All that matters is active or not, as the latter routines for aligning to the day set to active and the proper address
void deactNONActive(int current)
{
  for (int incr = 0; incr < (31 * sizeof(struct CAL_NDAY)); incr += sizeof(struct CAL_NDAY)){
    if (incr != current) {
      CAL_NDAY dmk;
      EEPROM.get(incr, dmk);
      //int ctra = dmk.counterADDR;
      //EEPROM.write(ctra, 1);
      dmk.active = false;
      //randomSeed(analogRead(0));   // new address for counter will be random in the counter space.
      //int targetADDR =  random(counterADDR_MIN, maxADDR);  // new address to hold the count value - apt to be written to a lot.
      //dmk.counterADDR = targetADDR; // set this up to the end that is never written to.
      EEPROM.put(incr, dmk);
    }// if
  }  // for
}
// This function will check if the day is active. If the day is not active, then that means it's a new day. So that
// day is made active, and every other day of the month - all 31 - are set to inactive with the deactNONActive function call.
int getCount() {
      int count = 0;
      unsigned int findADDR = ((t.mday - 1) * sizeof(struct CAL_NDAY));  // Address of the day
      CAL_NDAY dmr;
      int targetADDR;                 // Actual target address where the count value is, not to confuse with the address
      EEPROM.get(findADDR, dmr);      // This is our "find" 
      if (dmr.active == true) {   
         unsigned short  cc;          // the actual count will be read into here.
         targetADDR = dmr.counterADDR;// Counter itself is not in the CAL_NDAY structure, but referenced from the address stored there.
         EEPROM.get(targetADDR, cc);  // getting the actual count.
         cc++;                        // increment
         EEPROM.put(targetADDR, cc);  // Write back into that slot. This can happen a lot in a day.
         count =  cc;
      }  else {                       // The current day found is NOT active! Means this is a new day.
         dmr.active = true;           // New day, set this day slot to active
         randomSeed(analogRead(0));   // new address for counter will be random in the counter space.
         targetADDR =  random(counterADDR_MIN, maxADDR-4);  // new address to hold the count value - apt to be written to a lot.
         EEPROM.put(targetADDR, 1);   // the counter address now holds the value of 1, the first count
         dmr.counterADDR = targetADDR;
         EEPROM.put(findADDR, dmr);   // Here the original is overwritten
         deactNONActive(findADDR);    // Set all the other days to non active
         count = 1;
      }//if
      currentglobalcount = count;     // For local display.
      return count;                   // Send back result to serial requestor
}
// Somewhat dangerous function. This will manually increment the counter value. The intent of this function
// is to make it possible to deploy this counter device during the day when units have already been produced so 
// that the proper number can be picked up from that point, or similar situations like recovery from a crash. 
// Use wisely. 
void setCount(int mod) {
      unsigned int findADDR = ((t.mday - 1) * sizeof(struct CAL_NDAY));  
      CAL_NDAY dmr;
      int targetADDR;                 
      unsigned short  cc = 0;
      EEPROM.get(findADDR, dmr);      
      if (dmr.active == true) {   
         targetADDR = dmr.counterADDR;
         EEPROM.get(targetADDR, cc);  
         cc += mod;                   // Here the mod is going to be a minus or plus 1      
         if (cc < 0) cc = 0;          // Let's not go into negatives...                   
         EEPROM.put(targetADDR, cc);  
      }  else {                       
         dmr.active = true;     
         cc += mod;         
         if (cc < 0) cc = 0;      
         randomSeed(analogRead(0));   
         targetADDR =  random(counterADDR_MIN, maxADDR-4);  
         EEPROM.put(targetADDR, cc);  // As before, counter is set to 1 initially, repeated calls can increment
         dmr.counterADDR = targetADDR;
         EEPROM.put(findADDR, dmr);   
         deactNONActive(findADDR);    
      } //if
      currentglobalcount = cc;     // Global counter for display updated.
}
// Update the global for count display. 
void updateGlobal(){
      unsigned int findADDR = ((t.mday - 1) * sizeof(struct CAL_NDAY));  
      CAL_NDAY dmr;
      int targetADDR;                 
      unsigned short  cc = 0;
      EEPROM.get(findADDR, dmr);      
      if (dmr.active == true) {   
         targetADDR = dmr.counterADDR;
         EEPROM.get(targetADDR, cc);    
      }  // if
      currentglobalcount = cc;     // Global counter for display updated.
}
// Set display count. Used mainly to set the variable for display if this device is rebooted during the workday. 
void setGlobalCount() {
  unsigned int findADDR = ((t.mday - 1) * sizeof(struct CAL_NDAY));  
  CAL_NDAY dmr;
  int targetADDR;                 
  EEPROM.get(findADDR, dmr);  
  unsigned short  cc = 0;
  if (dmr.active == true) { // we have this current day already in use for counts so use that count. Otherwise 0 is sent back.   
    targetADDR = dmr.counterADDR;
    EEPROM.get(targetADDR, cc);  
  }  // if
  currentglobalcount = cc;
}
void setup() {
  pinMode(GENLED, OUTPUT);                     // RED Led for basic indications       
  pinMode(DSTLED, OUTPUT);                     // Blue LED for DST indication.
  pinMode(ser1LED, OUTPUT);                     // Blue LED for DST indication.
  pinMode(ser2LED, OUTPUT);                     // Blue LED for DST indication.  
  pinMode(ser3LED, OUTPUT);                     // Blue LED for DST indication.  
  digitalWrite(DSTLED, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);     
  EEPROM.get(dstADDR, DST);
  if (DST == false){
    digitalWrite(DSTLED, LOW);    // turn the LED off by making the voltage LOW
    delay(500); 
  } // if
  pinMode (12, INPUT_PULLUP);             // !!!! When Pin 12 is grounded, the EEPROM will be wiped clean.
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32        
      display.clearDisplay();
      display.display();     
  }  // if
  digitalWrite(GENLED, HIGH);   // turn the LED on (HIGH is the voltage level)
  if(digitalRead(12) == LOW) { // D12 grounded?
    showMessage("!!!!SYSTEM EEPROM RESET!!!",1);
    delay(5000);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    for (int incr = 0; incr < maxADDR; incr++)EEPROM.put(incr, 0);  // Always know the amount available. Write every address to 0
    int writeADDR = 0;    
    for (int incr = 0; incr < (31 * sizeof(struct CAL_NDAY)); incr += sizeof(struct CAL_NDAY))
    { 
      randomSeed(analogRead(0));
      CAL_NDAY dm = {false, (unsigned int) random(400, 4090), {0,0,0,0,0,0,0,0}}; 
      EEPROM.put(writeADDR, dm);
      EEPROM.put(dm.counterADDR, 0);
      writeADDR += sizeof(dm);  // Next write address is skipping over the data just written
   }   //for
  }  // if
  delay(500);                       // wait for a second
  digitalWrite(GENLED, LOW);    // turn the LED off by making the voltage LOW
  delay(500); 
  Wire.begin();
  delay(500); 
  DS3231_init(DS3231_CONTROL_INTCN);  // RTC setup
  // initialize serial:
  Serial.begin(9600);
  delay(500); 
  Serial1.begin(9600);
  delay(500); 
  Serial2.begin(9600);
  delay(500); 
  Serial3.begin(9600);
  Serial.println("READY local");
  delay(500); 
  Serial1.println("READY ser 1");
  delay(500); 
  Serial2.println("READY ser 2");
  delay(500); 
  Serial3.println("READY ser 3");
  DS3231_get(&t);
  setGlobalCount(); 
  lockout = false;
}
void loop() {
  DS3231_get(&t);
  String s = String(t.mon) + "/" +String(t.mday)+ "/" + String(t.year) + " " + String(t.hour) + ":" + String(t.min) + ":" + String(t.sec);
  showTimeString(s, currentglobalcount);
  delay(1000);  
}
void serialEvent1() {
  digitalWrite(ser1LED, HIGH); 
  while (lockout == true){delay(100);}    // The other Serial or process is getting a count, so wait a moment    
  int directive = Serial1.parseInt();
  Serial1.flush();    
  if (directive == 777) {  // Incoming 777 code for a increment of number. Use of code incoming may allow for further directives in the future possibly
    lockout = true;
    int result = getCount();
    Serial1.print(result);
    Serial1.print("\r\n");
    delay(10);
    lockout = false;
  } else if (directive == 44){
    int currj = ((t.year-2000) * 1000) + dayOfYear(t.year, t.mon, t.mday);          // Current Julian Date 
    Serial1.println(currj);
  }  // if
  Serial1.flush();
  digitalWrite(ser1LED, LOW); 
}
void serialEvent2() {
  digitalWrite(ser2LED, HIGH); 
  while (lockout == true){delay(100);}  // The other Serial or process is getting a count, so wait a moment
  int directive = Serial2.parseInt();
  Serial2.flush();  
  if (directive == 777) {  
    lockout = true;
    int result = getCount();
    Serial2.print(result);
    Serial2.print("\r\n");
    delay(10);
    lockout = false;
  }  else if (directive == 44){
    int currj = ((t.year-2000) * 1000) + dayOfYear(t.year, t.mon, t.mday);          // Current Julian Date 
    Serial2.println(currj);
  }//  if
  Serial2.flush(); 
  digitalWrite(ser2LED, LOW); 
}

void serialEvent3() {
  digitalWrite(ser3LED, HIGH); 
  while (lockout == true){delay(100);}  // The other Serial or process is getting a count, so wait a moment
  int directive = Serial3.parseInt();
  Serial3.flush();  
  if (directive == 777) {  
    lockout = true;
    int result = getCount();
    Serial3.print(result);
    Serial3.print("\r\n");
    delay(10);
    lockout = false;
  }  else if (directive == 44){
    int currj = ((t.year-2000) * 1000) + dayOfYear(t.year, t.mon, t.mday);          // Current Julian Date 
    Serial3.println(currj);
  }//  if
  Serial3.flush();
  digitalWrite(ser3LED, LOW); 
}
void serialEvent() {    // for serial events through the CH340 USB port
  while (lockout == true){delay(500);}  // The other Serials are busy providing a count, so wait a moment
  int directive = Serial.parseInt();
  Serial.flush();  
  digitalWrite(GENLED, HIGH);   // turn the LED on (HIGH is the voltage level)
  if (directive == 1) {  // dump all of the data in the EEPROM for viewing
    lockout = true;
    int dayctr = 1;
    digitalWrite(GENLED, HIGH);   // turn the LED on (HIGH is the voltage level)
    for (int incr = 0; incr < (31 * sizeof(struct CAL_NDAY)); incr += sizeof(struct CAL_NDAY))
    { 
      Serial.print("ADDR ");
      Serial.print(incr);
      Serial.print("\t\tMDAY ");
      Serial.print(dayctr++);
      CAL_NDAY dmr;
      EEPROM.get(incr, dmr);
      if (dmr.active == true)
        Serial.print("\t\tACTIVE\t");
      else
        Serial.print("\t\tINACTIVE ");
      Serial.print("\tSTOR ");
      for (int j = 0; j < 8; j++)Serial.print(dmr.stg[j]);
      Serial.print("\t\tCTR LOC ");
      Serial.print(dmr.counterADDR);
      Serial.print("\tCURRCT ");
      unsigned short  cc;          // current count
      int targetADDR = dmr.counterADDR;
      EEPROM.get(targetADDR, cc);  
      Serial.print(cc);
      Serial.println();
    }   //for  
  }  else if (directive == 2) {       //   // Toggle DST
    Serial.println("DAYLIGHT SAVINGS TIME TOGGLED");
    DST = !DST;
    EEPROM.put(dstADDR, DST);  // Write back into that EEPROM address. 
    if (DST == true){
      digitalWrite(DSTLED, HIGH);    // turn the LED off by making the voltage LOW
      Serial.println("DST IS ON, LED SHOULD BE ON");
      t.hour++;
    } else {
      digitalWrite(DSTLED, LOW);    // turn the LED off by making the voltage LOW
      Serial.println("DST IS OFF, LED SHOULD BE OFF");
      t.hour--;
    } // if
    DS3231_set(t);  
  } else if (directive == 411) {        // Information on what to send to this unit
    Serial.print("SF2 SERIAL GENERATOR. \n"
"1 Read all data from the EEPROM \n"
"2 Toggle DST on or off, which will also increase or decrease the time by 1 hour and toggle the LED \n"
"44 Get current Julian Date to use for date confirmation\n"
"777 Get current Count. 777 to this port does NOT increment the counter.\n"
"411 Display this information \n"
"555 Summon the Hoff (LED check)\n"
"+/- 666 will manually increment or decrement the current counter of the day. Devilish stuff.\n"
"+/- 12 increases or decreases the month of the year by one \n"
"+/- 31 increases or decreases the day of the month  by one\n"
"+/- 365 increases or decreases the year by one\n"
"+/- 24 increases or decreases the hour  by one\n"
"+/- 60 increases or decreases the minute by one\n");
  } else if (directive == 555) {
    larsen();
  }else if (directive == 44) {
    int currj = ((t.year-2000) * 1000) + dayOfYear(t.year, t.mon, t.mday);          // Current Julian Date 
    Serial.println(currj);
  } else if (directive == 777) {
    updateGlobal();   // Call to update the global display/return counter variable.
    Serial.println(currentglobalcount);
  } else if (abs(directive) == 666){    // increment or decrement the day's current count.
    setCount((directive < 0)?  -1:  1);
  } else {   // time modification
    // Here would be handled special routines for emergency time adjustment in case the RTC battery fails or some other unforeseen circumstance.
    // Time can be adjusted by sending positive or negative numbers representing the division of the date/time factor. 
    int modif = 0;
    bool modded = true;
    (directive < 0)? modif = -1: modif = 1;
    switch(abs(directive)){
      case 12: t.mon += modif; //Month
               break;
      case 31: t.mday += modif;  // Day
               break;
      case 365:t.year += modif;   // Year
               break;
      case 24: t.hour += modif;   // Hour
               break;
      case 60: t.min += modif;  // Minute
               break;
      default: modded = false; break;
    }  // switch
    if (modded == true)DS3231_set(t);
  }// if
  digitalWrite(GENLED, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(10);
  lockout = false;
  digitalWrite(GENLED, LOW);   // turn the LED on (HIGH is the voltage level)
}
void larsen()   // Summon the Hoff
{
  for (int incr = 4; incr <= 8; incr++)
  {
    digitalWrite(incr, HIGH); 
    delay(200);
    digitalWrite(incr, LOW); 
  }  // for
  for (int incr = 8; incr >= 3; incr--)
  {
    digitalWrite(incr, HIGH); 
    delay(200);
    digitalWrite(incr, LOW); 
  }  // for
}
int dayOfYear(int year, int month, int day)  // Julian Date return for on the spot checking.
{
    int days[] = { 31, 28, 31, 30, 31, 30,
               31, 31, 30, 31, 30, 31 };
    // If current year is a leap year and the date
    // given is after the 28th of February then
    // it must include the 29th February
    if (month > 2 && year % 4 == 0
        && (year % 100 != 0 || year % 400 == 0)) {
        ++day;
    }  // if
    // Add the days in the previous months
    //while (month-- > 0) {
    for (int incr = 0; incr < month-1; incr++){
        day = day + days[incr];
    }  // for
    return day;
}
