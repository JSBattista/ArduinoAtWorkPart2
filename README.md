# Arduino A tWork Part2
In this second around of Arduino on the job, the situation presented was a little more challenging and but not complex. What we needed was a device that can connect to multiple 
test stations and generate a unique number from 1 to 9999. That number would sit on the tail end of a serial number comprised of some plant/model/make codes and a julian date. 
This number must be unique, but with multiple stations doing the same thing, what's an easy way to keep track? 
The simplest way is to use a standalone device that generates a unique number on request, incremented +1 on each request, and rolling back to 1 when the day advances. 

Particular to this case:
 - EEPROM usage
 - RTC module
 - UART to Serial communication
 
 For the EEPROM I created a data structure consisting of 31 "slots" representing days of the month, it matters not if a month has 31 days or less. It does not even matter what 
 month it is, or what year it is. When the date is called on from the RTC module, that "day of the month" is the number used to access one of the 31 positons. So when the day 
 changes, so does the position used. Think of it as the element of a 31 element array. 
 That element contains a variable telling us if the day is active or not, and an address. That address is where the actual count is stored. So beyond the 31 elements, the rest 
 of the Arduino Mega EEPROM space is used but it's going to be a random address each day. This is to spread out the EEPROM writes. The element itself is not written to a lot.
 All other elements are updated to be inactive. When a new day comes, and it's seen as inactive, that triggers the response to set it active, get a new address for the counter, 
 assign it as 1, and return that one and write the EEPROM data. For the rest of the day, we're just accessing the element, getting the address of the count, incrementing, updating,
 and sending back the new count. 
 The RTC is the sort that recharges the battery so be sure to use the actual rechargeable button battery not a generic 3.3V or there's going to be trouble. 
 The Arduino uses UART RX/TX to some UART to RS-232 boards, and down stream, the PC are using RS-232 to USB adaptors. This Rube Goldberg pipeline turns out to be way more reliable
 than using those chceap blue UART to USB adaptors. 
 If the unit is powered down, it does not miss a beat. But the USB communication to the Arduino Mega is left in place and is also the power source too. So the PC connected to that 
 talking to the CH340 COM port can send commands to toggle DST, make time adjustments, and other things. 
 The PCs connected serially are simply making a request through LabView. The unit also have provisions to lock out the Serial Event from 
 
 For the case, an old Thermaltake external HDD case from the early 2000s worked perfectly. 
 
 
