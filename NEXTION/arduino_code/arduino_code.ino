/* Upload this project to GeekWorm ESP32-C1 using :
 *  -selected board: >>SparkFun ESP32 Thing<< or >> Wemos WiFi& Bluetooth battery<<
 *  - set it to 40MHz (enough for this project)
 *  -upload speed: 115200 : IMPORTANT!
 *  -select available COM port
 *  Connect USB direct to computer (200 mA required +50 mA for LCD) or external power (5V-350mA min/500 mA better)
 *  If not enough current available: Brownout error!
 *  Pins: Geekworm ESP32 <--> Nextion
 *                IO16     -    TX      
 *                IO17     -    RX
 *                GND      -    GND
 *                3.3V     -   +5v  (Nextion works also with 3.3V)
 *  Use Library: "nextion" with a modified file Nextion.h:        
 *  #define nextion Serial2       // This line is for ESP32 with hardware Serial2
 *  //#define USE_SOFTWARE_SERIAL //Comment this line if you use HardwareSerial
 *  The Nextion file is genrated using Nextion Editor: digital_clock-weather.HMI
 *  Once compiled, the file digital_clock-weather.tft can be transfered from File/Open Build folder to the transfer SD card.
 *  With the SD card inserted, Nextion updates the screen. Remove the SD card and connect the pins to ESP32.
 *      Project size: 512910/1310720 bytes (321152 compressed) with variables 47956/294912 bytes 
 *  Receives and displays the weather forecast from the Weather Underground and then displays data using a 
 * JSON decoder wx data to a NEXTION display. 
 * Weather data received via WiFi connection from Weather Underground Servers and using their 'Forecast' API and data
 * is decoded using Copyright Benoit Blanchon's (c) 2014-2017 excellent JSON library.
 * 
 * This MIT License (MIT) is copyright (c) 2017 by David Bird and permission is hereby granted, free of charge, to
 * any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, but not to sub-license and/or 
 * to sell copies of the Software or to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 *   See more at http://dsbird.org.uk
 */

HardwareSerial Serial2(2);        // Activate Serial communication 2 on ESP32 (RX=IO16 and TX=IO17) 
#include <TimeLib.h>              // Arduino Time library for 32 bit 
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Nextion.h>              // Was modified to accept Serial1 hardware on ESP32
#include <ArduinoJson.h>
Nextion myNextion(nextion, 9600);             //create a Nextion object named myNextion using the nextion serial port

char ssid[] = "aaaaaaaaaaaaaa";  //  your network SSID (name)  DEFINED in callserver();
char pass[] = "bbbbbbbbbbbbbb";        // your network password


const int ledPin =0,  timeZone = 2;     // East European Time (winter time)
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String RomanMonths[12] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"}; // January is month 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0
int days,DST=0;

unsigned long t0t=0, tsec=3600, t0w=0, wsec=3600; // Update weather after wsec [s] (30 min...2h is ok) and time after tsec (2..4h) {unsigned long < 4.29e9}


//------ WEATHER NETWORK VARIABLES---------
// Use your own API key by signing up for a free developer account at http://www.wunderground.com/weather/api/
//Selected Plan: Stratus Developer: Calls Per Day= 500;  Calls Per Minute : 10


String API_key       = "xxxxxxxxxxxxxxxxx";            // See: http://www.wunderground.com/weather/api/d/docs (change here with your KEY)
String City          = "yyyyyyyyyyyyy";                   // Your home city : Weather Station ID: IBUCURET77
String pws           = "zzzzzzzzzzzz";                  //  Dinicu Golescu 35 https://www.wunderground.com/personal-weather-station/dashboard?ID=IMUNICIP22&cm_ven=localwx_pwsdash
String Country       = "CC";                          // Your country   
String Conditions    = "conditions";                  // See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
char   wxserver[]    = "api.wunderground.com";        // Address for WeatherUnderGround
// unsigned long        lastConnectionTime = 0;       // Last time you connected to the server, in milliseconds
String SITE_WIDTH =  "900";
String icon_set   =  "k";                             // 
String Units      =  "M";                             // Default use either M for Metric, X for Mixed and I for Imperial
char* conds[]={"\"temp_c\":"};
// These are all (?) weather short definitions:
String rain="Rain, Hail, Rain Showers, Ice Pellet Showers, Hail Showers, Small Hail Showers, Freezing Rain, Small Hail, rain";
String thunde="Thunderstorm, Thunderstorms and Rain, Thunderstorms and Snow, Thunderstorms and Ice Pellets, Thunderstorms with Hail, Thunderstorms with Small Hail, chancetstorms, tstorms";
String sunny="Clear, Unknown Precipitation, Unknown, sunny, unknown, clear";
String cloudy="Overcast, cloudy";
String chanceflurries="chanceflurries, flurries";
String chancerain="Drizzle, Rain Mist, chancerain, chancesleet, sleet";
String chancesnow="Snow Grains, Ice Crystals, Ice Pellets, Low Drifting Snow, Snow Blowing Snow Mist, Freezing Drizzle, chancesnow";
String fog="Mist, Fog, FogPatches, Smoke, VolcanicAsh, WidespreadDust, Sand, Haze, Spray, DustWhirls, Sandstorm, LowDrifting WidespreadDust, LowDriftingSand, BlowingWidespreadDust, BlowingSand, FreezingFog, PatchesofFog, ShallowFog, fog, hazy";
String mostlycloudy="MostlyCloudy, Squalls, FunnelCloud, mostlycloudy, partlysunny";
String partlycloudy="PartlyCloudy, ScatteredClouds, mostlysunny, partlycloudy";
String snow="Snow, Blowing Snow, Snow Showers, snow";
float temp_c=0;
int temp_e, nr=0;
String wind_e, h_up, m_up, h_set, m_set;             // Data strings 
boolean timeok=false, foreok=false, condok=false, zzz=false;  // Update flags
//-------------------------------------------------------------------------------------------
//################ PROGRAM VARIABLES and OBJECTS ################
// Conditions
String webpage, city, country, date_time, observationtime,
       DWDay0, DMon0, DDateDa0, DDateMo0, DDateYr0, Dicon0, Dicon_url0, DHtemp0, DLtemp0, DHumi0, Dpop0, DRain0, DW_mph0, DW_dir0, DW_dir_deg0, DWeather0, Dconditions0, DcurrentTemp, Txw1,
       DWDay1, DMon1, DDateDa1, DDateMo1, DDateYr1, Dicon1, Dicon_url1, DHtemp1, DLtemp1, DHumi1, DPop1, DRain1, DW_mph1, DW_dir1, DW_dir_deg1, DWeather1, Dconditions1,
       DWDay2, DMon2, DDateDa2, DDateMo2, DDateYr2, Dicon2, Dicon_url2, DHtemp2, DLtemp2, DHumi2, DPop2, DRain2, DW_mph2, DW_dir2, DW_dir_deg2, DWeather2, Dconditions2,
       DWDay3, DMon3, DDateDa3, DDateMo3, DDateYr3, Dicon3, Dicon_url3, DHtemp3, DLtemp3, DHumi3, DPop3, DRain3, DW_mph3, DW_dir3, DW_dir_deg3, DWeather3, Dconditions3,
       DWDay4, DMon4, DDateDa4, DDateMo4, DDateYr4, Dicon4, Dicon_url4, DHtemp4, DLtemp4, DHumi4, DPop4, DRain4, DW_mph4, DW_dir4, DW_dir_deg4, DWeather4, Dconditions4;
char buffer[100] = {0};
char buffer_hour[10] = {0};
char Txtweather[200];
//-------------------------------------------------------------------------------------------

// NTP Servers:
//static const char ntpServerName[] = "ntps1-0.cs.tu-berlin.de";    // Chosen Atomic Time server
static const char ntpServerName[] = "0.pool.ntp.org";            // Recommended server 
const int NTP_PACKET_SIZE = 48;     // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];    //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;                        // Initialize User Datagram Protocol (UDP)
unsigned int localPort = 8888;      // local port to listen for UDP time packets

//-------------------------------------------------------------------------------------------
// List of the functions defined below:
time_t getNtpTime();                // Convert received seconds into HH:MM:SS, YY-MO-DD     
void sendNTPpacket(IPAddress &address);  // Require site data 
void digitalClockDisplay();         // See below function to print time on Nextion
void weatherDisplay();              // See below function to print weather on Nextion
void obtain_forecast (String forecast_type); // See below function to get weather data
#define min(a,b) ((a)<(b)?(a):(b)); // Define the min(a,b) function!
// =======================================================================


void callserver(){
// Start by connecting to a local WiFi router
// Since WiFi.disconnect(true)erases ssid&pass, define these here each time.
// =========================!!!!!===================================
  char ssid[] = "RomTelecom-WEP-B086";  //  your network SSID (name)
  char pass[] = "AYPDG7J5TFKGF";        // your network password
// =========================!!!!!===================================
    Serial.println(F("============================================"));
    Serial.print(F("Connecting to "));  Serial.print(ssid); Serial.print(F(" & ")); Serial.println(pass);
    delay(200);
  WiFi.begin(ssid, pass);
    Serial.println(F(">>>>>>>")); Serial.print("Connecting...");
  myNextion.setComponentText("g0","Connecting..."); // Scroll weather description text !
  delay(300);
  int iter=0;           // Try several times to connect to local router
  while (WiFi.status() != WL_CONNECTED  && iter<50) {
    // LED blink as long as the connection is not establihed
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
    digitalWrite(ledPin, HIGH);
    Serial.print(".");
    iter++;
  }
  if (iter<50){
      digitalWrite(ledPin, HIGH);               // Led OFF
      Serial.print("WiFi connected to local"); Serial.print("IP : ");
      Serial.println(WiFi.localIP());   Serial.println("********************************");
      myNextion.setComponentText("g0","Connected. Updating soon!"); // Scroll weather description text !
      delay(300);
      // At this stage the ESP32 is connected to the local network
  }
  else{
      digitalWrite(ledPin, LOW);              // Led ON
      Serial.println(F("!! WiFi connection failed !!!"));
      Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!")); 
      WiFi.disconnect(true); 
      delay(2000);
  }
}


void setup()
{
  Serial.begin(115200);            // USB communication with Serial Monitor
  Serial2.begin(9600);             // Initiate 2-nd UART towards Nextion
  pinMode(ledPin, OUTPUT);         // Set Led pin as output
  myNextion.sendCommand("rest");   // reset Nextion
  myNextion.sendCommand("dim=40");       // Dim to 40% the display

  while(WiFi.status() != WL_CONNECTED){
    callserver();                  // Try to connect to local server
  }
        
  Serial.print("Synchronizing time: ");
  Serial.println(ntpServerName);
  setSyncProvider(getNtpTime);        // Set the MCU time from Internet: the Atomic Clock Time
  setSyncInterval(86400);              // Requiring sync. after ... seconds
                                      // but will fail if WiFi is turned off.
  Serial.print("Next sync. in ");Serial.print(tsec);Serial.println(" s");
  Udp.stop();   // STOP UDP
  delay(500);

  obtain_forecast("forecast");     // Get weather forecast
  obtain_forecast("conditions");   // Get local conditions


  digitalWrite(ledPin, HIGH);     // LED OFF if OK 

  Serial.println("==============================================");
  Serial.print("Weather for current time:");
  Serial.print(hour());Serial.print(":");Serial.print(minute());Serial.print(":");Serial.println(second());

  Serial.print("Time status:");Serial.println(timeok);
  Serial.print("Forecast status:");Serial.println(foreok);
  Serial.print("Conditions status:");Serial.println(condok);
  
  Serial.println(DWDay1);  Serial.println(Dicon1);  Serial.println(DHtemp1);
  Serial.println(DLtemp1);  Serial.println(DHumi1);  Serial.println(DW_mph1);

  Serial.println(DWDay2);  Serial.println(Dicon2);  Serial.println(DHtemp2);
  Serial.println(DLtemp2);  Serial.println(DHumi2);  Serial.println(DW_mph2);

  Serial.println(DWDay3);  Serial.println(Dicon3);  Serial.println(DHtemp3);
  Serial.println(DLtemp3);  Serial.println(DHumi3);  Serial.println(DW_mph3);
  Serial.println("--Local weather, now: --");
  Serial.println(Txtweather);  Serial.println(temp_e);  Serial.println(wind_e);
  Serial.print(h_up);Serial.print(":"); Serial.println(m_up);
  Serial.print(h_set);Serial.print(":");Serial.println(m_set);
   vTaskDelay(100/portTICK_RATE_MS); 
  Serial.println("==============================================");
  digitalClockDisplay();    // Send clock data to Nextion
  vTaskDelay(100/portTICK_RATE_MS);
  weatherDisplay();         // Send weather data to Nextion
  nr=min(tsec, wsec);
  Serial.print("WiFi off for "); Serial.print(nr); Serial.println(" s : after Setup");
  WiFi.disconnect(true);   // STOP WIFI!!
  vTaskDelay(100/portTICK_RATE_MS); 
  nr=0;
 }


time_t prevDisplay = 0; // moment when the clock was displayed

  
void loop()
{

// Every second: update time on Nextion
  if (now() != prevDisplay) {       // ..update the display only if time has changed [every sec.]
     prevDisplay = now();
    //----------------------------------------
    // Compute DST hour if server is not a local one:
    DST=0;
    if (month()>3 && month()<10)            
     DST=1;
     days=31-((5*year())/4+4)%7;
    if (month()==3 && day()>=days)
     DST=1;
     days=31-((5*year())/4+1)%7;
    if (month()==10 && day()<days)
        DST=1;
    //----------------------------------------
     digitalClockDisplay();        // Send clock data to Nextion every second
  } 

  
  if(millis() - t0t > tsec*1e3) {   // after every time interval "tsec"
    if (WiFi.status() != WL_DISCONNECTED)
       WiFi.disconnect(true); 
     // if (timeStatus() == timeNeedsSync){
     // Firstly: connect to local router (WiFi might be disconnected)
    vTaskDelay(100/portTICK_RATE_MS);
    while(WiFi.status() != WL_CONNECTED){ 
      callserver();                  // Try to connect to local server
      if(WiFi.status() != WL_CONNECTED){
         delay(5000);
         Serial.println("RESET ESP32 in loop() for time update !!!!");
         ESP.restart();             // Restart ESP32! So, the code is reinitialized!!              
      }
      // Should be connected at this point, or MCU restarted ???
    }

    if(WiFi.status() == WL_CONNECTED){ 
     Serial.print("Synchronizing time: ");  Serial.println(ntpServerName);
     // The "Time" library is supposed to connect to time server now
     
     setSyncProvider(getNtpTime);        // Set the MCU time from Internet: the Atomic Clock Time
     setSyncInterval(86400);              // Requiring sync. after ... seconds
                                      // but will fail if WiFi is turned off.
     Serial.print("Next sync. in ");Serial.print(tsec);Serial.println(" s");
     Udp.stop();   // STOP UDP
     
     delay(500);
     obtain_forecast("forecast");     // Get weather forecast
     obtain_forecast("conditions");
     
     if (timeok==true){
      t0t=millis();
      myNextion.setComponentValue("bt3",0);  // Button3 [:] has blue bkgd. if time is updated ...
      myNextion.setComponentValue("bt4",0);  // Button4 [:] has blue bkgd. if time is updated ... 
      WiFi.disconnect(true); 
      Serial.print(F("WiFi off for ")); Serial.print(tsec); Serial.println(F(" s : after Time & Weather sync.!")); 
     }
     else{
      myNextion.setComponentValue("bt3",1);  // Button3 [:] has orange bkgd. if time is not updated ...
      myNextion.setComponentValue("bt4",1);  // Button4 [:] has orange bkgd. if time is not updated ...
      delay(500);
      WiFi.disconnect(true); 
      Serial.print(F("WiFi off for ")); Serial.print(tsec); Serial.println(F(" s : Failed sync.!"));
     }
    }
    else{
      t0t=tsec*1e3-60000;         // Repeat updating in 1 minute
      WiFi.disconnect(true); 
      Serial.println(F("!! WiFi off after failing to connect!"));
    }

   // Just for debugging, print results on Serial Monitor attached via USB
       Serial.println(F("=============================================="));   
       Serial.print(F("Weather for current time:"));
       Serial.print(hour());Serial.print(":");Serial.print(minute());Serial.print(":");Serial.println(second());
       nr++;
       Serial.print("Update no:");Serial.println(nr);
       Serial.print("Time status:");Serial.println(timeok);
       Serial.print("Forecast status:");Serial.println(foreok);
       Serial.print("Conditions status:");Serial.println(condok);
       Serial.println(F("----------------------------------------------")); 
       Serial.println(DWDay1);   Serial.println(Dicon1);   Serial.println(DHtemp1);
       Serial.println(DLtemp1);   Serial.println(DHumi1);   Serial.println(DW_mph1);
       Serial.println(DWDay2);   Serial.println(Dicon2);   Serial.println(DHtemp2);
       Serial.println(DLtemp2);   Serial.println(DHumi2);   Serial.println(DW_mph2);
       Serial.println(DWDay3);   Serial.println(Dicon3);   Serial.println(DHtemp3);
       Serial.println(DLtemp3);   Serial.println(DHumi3);   Serial.println(DW_mph1);
       Serial.println(Txtweather);   Serial.println(temp_e);   Serial.println(wind_e);
       Serial.print(h_up);Serial.print(":"); Serial.println(m_up);
       Serial.print(h_set);Serial.print(":");Serial.println(m_set);

       // In any case write weather data to Nextion
        weatherDisplay();                  // Send weather data to Nextion
   
 }
  

  //==============================================================================
  // During night hours 0-6, turn off display, but reactivate if touched  
    if (hour()<6 && zzz==false){
        
        myNextion.sendCommand("dim=0");       // Dim to 0 the display
        // myNextion.sendCommand("thup=1");      // Remain responsive to touch
        delay(1000);
        zzz=true;                             // Sleeping flag ON, to do this once/day.
        Serial.println(F("Display sleeping!"));
        //  myNextion.sendCommand("sleep=1");   // Go to sleep Nextion!!
    }
    else if (hour()>=6 && zzz==true){
        myNextion.sendCommand("dim=40");       // Dim to 40% the display
        //myNextion.sendCommand("wup");         //   wake-up Nextion
        delay(1000);
        zzz=false;                             // Not-sleelping flag
        Serial.println(F("Display activated!"));  
    }  
  //==============================================================================
    if (millis() < t0t){
      t0t=millis();                    // After millis reached the limit, reset counters
      Serial.print("Time counter t0t reset!");
    }
      
    if (millis() < t0w){
      t0w=millis();                    // After millis reached the limit, reset counters
      Serial.print("Weather counter t0w reset!");  
    }
      
}




void digitalClockDisplay()
{// Write Clock data on Nextion
  // Display the time. Digital clock.
  // Send a number to a number cell of Nextion
   if (DST==1)
    myNextion.setComponentValue("n0",hour()+1);
  else
    myNextion.setComponentValue("n0",hour());    
  myNextion.setComponentValue("n1",minute());
  myNextion.setComponentValue("n2",second());
  myNextion.setComponentText("t3",daysOfTheWeek[weekday()-1]);
  // Send strings to string cells of Nextion
  myNextion.setComponentText("t4", String(day())); 
  //myNextion.setComponentText("t5", String(month()));
  myNextion.setComponentText("t5", month_of_year[month()-1]);
  myNextion.setComponentText("t6", String(year()));

}

void weatherDisplay()        
{// Write weather data on Nextion
  myNextion.sendCommand("page 0");
  myNextion.setComponentValue("n3",temp_e);    // to numeric obj. n3 Temp. outside now
  myNextion.setComponentText("t8",wind_e);     // to text obj. 8 Wind speed outside now
  myNextion.setComponentText("t10",DHtemp1);   // to text obj. 10  Max Temp. of the day
  myNextion.setComponentText("t11",DLtemp1);   // to text obj. 11  Min Temp. of the day
  myNextion.setComponentText("t12",DHumi1);    // to text obj. 12  Humidity  of the day
  myNextion.setComponentText("g0",Txtweather); // Scroll weather description text + minutes after update!
  myNextion.setComponentText("t33",h_up+":"+m_up);    // to text obj. 33  Sun rise time  
  myNextion.setComponentText("t34",h_set+":"+m_set);    // to text obj. 33  Sun set time  
  
  myNextion.sendCommand("page 1");
  myNextion.setComponentText("t14",daysOfTheWeek[(weekday())%7]);  // Swow tomorow
  myNextion.setComponentText("t20",DHtemp2);   //to text obj. 20 Estimated High Temp. 
  myNextion.setComponentText("t21",DLtemp2);  // Estimated Low Temp.
  myNextion.setComponentText("t22",DHumi2);   // Estimated humidity
  myNextion.setComponentText("t31",DW_mph2);  // Estimated wind speed

  myNextion.setComponentText("t15",daysOfTheWeek[(weekday()+1)%7]); // Swow after tomorow
  myNextion.setComponentText("t26",DHtemp3);  //to text obj. 26 Estimated High Temp.
  myNextion.setComponentText("t27",DLtemp3);  // Estimated Low Temp.
  myNextion.setComponentText("t28",DHumi3);   // Estimated humidity
  myNextion.setComponentText("t32",DW_mph3);  // Estimated wind speed

  myNextion.sendCommand("page 0");
    // Now change the pictures accordingly:
  if(Dicon1=="clear")
    myNextion.sendCommand("p0.pic=0");
  if(Dicon1=="partlycloudy")
    myNextion.sendCommand("p0.pic=1");
  if(Dicon1=="cloudy"||Dicon1=="mostlycloudy")
    myNextion.sendCommand("p0.pic=2");
  if(Dicon1=="rain" || Dicon1=="chancerain")
    myNextion.sendCommand("p0.pic=3");
  if(Dicon1=="snow")
    myNextion.sendCommand("p0.pic=4");
  if(Dicon1=="fog" || Dicon1=="hazy" || Dicon1=="mist")
    myNextion.sendCommand("p0.pic=5");
  if(Dicon1=="thunderstorm" || Dicon1=="chancetstorms")
    myNextion.sendCommand("p0.pic=6");
  if(Dicon1=="sleet" || Dicon1=="chancetstorms")
    myNextion.sendCommand("p0.pic=6");

 
  myNextion.sendCommand("page 1");        // Go to page 1
  if(Dicon2=="clear")
    myNextion.sendCommand("p1.pic=0");
  if(Dicon2=="partlycloudy")
    myNextion.sendCommand("p1.pic=1");
  if(Dicon2=="cloudy"||Dicon1=="mostlycloudy")
    myNextion.sendCommand("p1.pic=2");
  if(Dicon2=="rain" || Dicon2=="chancerain")
    myNextion.sendCommand("p1.pic=3");
  if(Dicon2=="snow")
    myNextion.sendCommand("p1.pic=4");
  if(Dicon2=="fog" || Dicon2=="hazy" || Dicon2=="mist")
    myNextion.sendCommand("p1.pic=5");
  if(Dicon2=="thunderstorm" || Dicon2=="chancetstorms")
    myNextion.sendCommand("p1.pic=6");

  if(Dicon3=="clear")
   myNextion.sendCommand("p2.pic=0");
  if(Dicon3=="partlycloudy")
   myNextion.sendCommand("p2.pic=1");
  if(Dicon3=="cloudy"||Dicon1=="mostlycloudy")
   myNextion.sendCommand("p2.pic=2");
  if(Dicon3=="rain" || Dicon3=="chancerain")
   myNextion.sendCommand("p2.pic=3");
  if(Dicon3=="snow")
   myNextion.sendCommand("p2.pic=4");
  if(Dicon3=="fog" || Dicon3=="hazy" || Dicon3=="mist")
    myNextion.sendCommand("p2.pic=5");
  if(Dicon3=="thunderstorm" || Dicon3=="chancetstorms")
    myNextion.sendCommand("p2.pic=6");
 
  myNextion.sendCommand("page 0");        // Come back to page 0
  if (condok==true)
    myNextion.setComponentValue("bt1",1);  // Button1 is green if local conditions are updated ...
  else
    myNextion.setComponentValue("bt1",0);  // else it is red.
    
  if (foreok==true)
    myNextion.setComponentValue("bt2",1);  // Button2 is green if forecast is updated ...
  else
    myNextion.setComponentValue("bt2",0);  // else it is red.  
}



/*-------- NTP code ----------*/
time_t getNtpTime()
{ //PaulStoffregen/Time
   while(WiFi.status() != WL_CONNECTED){
      callserver();                  // Try (50 times) to connect to the local server
      if(WiFi.status() != WL_CONNECTED){
        delay(3000);
        ESP.restart();               // Restart ESP32! So, the code is reinitialized!!   
      }
      // Should be connected at this point, or MCU restarted 
   }
  Udp.begin(localPort);
  IPAddress ntpServerIP; // NTP server's ip address
  
  // while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("Transmit NTP Request to:");
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);  Serial.print("= ");  Serial.println(ntpServerIP);
  uint32_t beginWait = millis();
  sendNTPpacket(ntpServerIP);
  
  while (millis() - beginWait < 5000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      Serial.println(F("Received NTP Response. Successfully updated Time..."));
      Serial.println(F("====================================="));
      timeok=true;
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;  // maybe add +1 for update delays?
      // Exit function here.
    }
    else
      timeok=false;
  }
  Udp.endPacket();
  Serial.println("No NTP Response :-(");
  Serial.println("Time is not synchronized");
  Serial.println("RESET ESP32 in getNtpTime() !!!!");
  ESP.restart();               // Restart ESP32! So, the code is reinitialized!!   
  return 0; // return 0 if unable to get the time
}



void sendNTPpacket(IPAddress& address)
// send an NTP request to the time server at the given address
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}



void obtain_forecast (String forecast_type) {
  static char RxBuf[8704];
  String request;
  if (forecast_type=="forecast")
    request  = "GET /api/" + API_key + "/"+ forecast_type + "/q/" + Country + "/" + City + ".json HTTP/1.1\r\n"; // Request Country/City
  else
    request  = "GET /api/" + API_key + "/"+ forecast_type + "/astronomy/q/geolookup/pws:" + pws + ".json HTTP/1.1\r\n";  // Request data by pws station
  // http://api.wunderground.com/api/7a4e220afaaf6547/conditions/q/geolookup/pws:IBUCURET77.json

  request += F("User-Agent: Weather Webserver v");
  //request += version;
  request += F("\r\n");
  request += F("Accept: */*\r\n");
  request += "Host: " + String(wxserver) + "\r\n";
  request += F("Connection: close\r\n");
  request += F("\r\n");
  Serial.println(F("------------------------------------------"));
  Serial.print(F("Connecting to ")); Serial.println(wxserver);
  WiFiClient httpclient;
  if (!httpclient.connect(wxserver, 80)) {
    Serial.println(F("connection failed"));
    httpclient.flush();  //Waits until all outgoing characters in buffer have been sent (forever if lost connection!)
    httpclient.stop();
    WiFi.disconnect(true);
    vTaskDelay(1000/portTICK_RATE_MS);
              ESP.restart();             // Restart ESP32! So, the code is reinitialized??? 
    return;
  }
  vTaskDelay(100/portTICK_RATE_MS);
  httpclient.print(request); //send the request to the server
  httpclient.flush();  //Waits until all outgoing characters in buffer have been sent (forever if connection lost)
  Serial.print(forecast_type); Serial.print(F(": "));
  Serial.println("The request was sent to Weather server. Waiting for the response...");
  vTaskDelay(100/portTICK_RATE_MS);
  
  // Collect http response headers and content from Weather Underground, discarding HTTP headers, 
  //   the content is JSON formatted and will be returned in RxBuf.
  int    respLen = 0;
  bool   skip_headers = true;
  String rx_line;
  unsigned long beginWait = millis();
  while ((httpclient.connected() || httpclient.available()) && (millis() - beginWait < 10000)) {
    //vTaskDelay(10/portTICK_RATE_MS);//works for keeping the watchdog fed when there is no other activity
    if (skip_headers) {
      rx_line = httpclient.readStringUntil('\n');
      if (rx_line.length() <= 1) { // a blank line denotes end of headers
        skip_headers = false;
      }
    }
    else {
      int bytesIn;
      bytesIn = httpclient.read((uint8_t *)&RxBuf[respLen], sizeof(RxBuf) - respLen);
      Serial.print(F("bytesIn: ")); Serial.println(bytesIn);
      if (bytesIn > 0) {
        respLen += bytesIn;           // Gather bytes in RxBuf
        if (respLen > sizeof(RxBuf)) respLen = sizeof(RxBuf);
        //vTaskDelay(10/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        
      }
      else if (bytesIn < 0) {
        vTaskDelay(200/portTICK_RATE_MS);
        Serial.print("?");  // Appears very often, waiting for an answer!
        vTaskDelay(200/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        
      }
    }
   vTaskDelay(100/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        
  }
  httpclient.stop();
  vTaskDelay(100/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        

   
  RxBuf[respLen++] = '\0'; // Terminate the C string
  // RxBuf contains now the server response, of length=respLen.
  if (respLen<=1 && forecast_type == "forecast"){
        foreok=false;     // If no data available return without changing forecast
        return;
  }
  
  if (respLen<=1 && forecast_type == "conditions"){
        condok=false;     // If no data available return without changing conditions
        return;
  }  
  
  if (forecast_type == "forecast"){
     if (showWeather_forecast(RxBuf)){
        foreok=true; 
        vTaskDelay(50/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        
     }
  }
  if (forecast_type == "conditions"){
     if (showWeather_conditions(RxBuf)){
         condok=true;
         vTaskDelay(50/portTICK_RATE_MS); //works for keeping the watchdog fed when there is no other activity        
     }
  }  
}


bool showWeather_forecast(char *json) 
{// Convert forecast received data into Strings and values 
  DynamicJsonBuffer jsonBuffer(8704);
  char *jsonstart = strchr(json, '{');    // Look for first "{"
// Serial.println(F("jsonstart ")); Serial.println(jsonstart); // This is input data
  if (jsonstart == NULL) {
    Serial.println(F("JSON data missing"));
    return false;
  }
  json = jsonstart;
  // Parse JSON
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
    return false;
  }
  JsonObject& forecast = root["forecast"]["simpleforecast"];
  String WDay1      = forecast["forecastday"][0]["date"]["weekday"];         DWDay1      = WDay1;
  int DateDa1       = forecast["forecastday"][0]["date"]["day"];             DDateDa1    = DateDa1<10?"0"+String(DateDa1):String(DateDa1);
  String Temp_mon   = forecast["forecastday"][0]["date"]["monthname"];
  String Mon1       = forecast["forecastday"][0]["date"]["monthname_short"]; DMon1       = Mon1;
  int DateYr1       = forecast["forecastday"][0]["date"]["year"];            DDateYr1    = String(DateYr1).substring(2);
  observationtime   = "from " + String(DDateDa1) + " " + Temp_mon + ", " + DateYr1;
  if (Units == "M" || Units == "X") {
    String icon1         = forecast["forecastday"][0]["icon"];               Dicon1      = String(icon1);
    String conditions1    = forecast["forecastday"][0]["conditions"];        Dconditions1   = String(conditions1);
    int Htemp1        = forecast["forecastday"][0]["high"]["celsius"];       DHtemp1     = String(Htemp1);
    int Ltemp1        = forecast["forecastday"][0]["low"]["celsius"];        DLtemp1     = String(Ltemp1);
    int rain1         = forecast["forecastday"][0]["qpf_allday"]["mm"];      DRain1      = String(rain1)+"mm";
    if (Units == "M") {int w_mph1 = forecast["forecastday"][0]["avewind"]["kph"];  DW_mph1 = String(w_mph1)+"km/h";}
    else {int w_mph1   = forecast["forecastday"][0]["avewind"]["mph"];  DW_mph1 = String(w_mph1)+"mph";}
  }
  String icon_url1  = forecast["forecastday"][0]["icon_url"];           
  Dicon_url1        = icon_url1.substring(0,icon_url1.indexOf("/i/c/")+5) + icon_set + icon_url1.substring(icon_url1.indexOf("/i/c/")+6);
  String pop1       = forecast["forecastday"][0]["pop"];                     DPop1       = String(pop1);
  String w_dir1     = forecast["forecastday"][0]["avewind"]["dir"];          DW_dir1     = String(w_dir1);
  String w_dir_deg1 = forecast["forecastday"][0]["avewind"]["degrees"];      DW_dir_deg1 = String(w_dir_deg1);
  int humi1         = forecast["forecastday"][0]["avehumidity"];             DHumi1      = String(humi1);
  
  String WDay2      = forecast["forecastday"][1]["date"]["weekday"];         DWDay2      = WDay2;
  int DateDa2       = forecast["forecastday"][1]["date"]["day"];             DDateDa2    = DateDa2<10?"0"+String(DateDa2):String(DateDa2);
  String Mon2       = forecast["forecastday"][1]["date"]["monthname_short"]; DMon2       = Mon2;
  int DateYr2       = forecast["forecastday"][1]["date"]["year"];            DDateYr2    = String(DateYr2).substring(2);
  if (Units == "M" || Units == "X") {
    String icon2         = forecast["forecastday"][1]["icon"];              Dicon2      = String(icon2);
    String conditions2    = forecast["forecastday"][1]["conditions"];       Dconditions2   = String(conditions2);
    int Htemp2        = forecast["forecastday"][1]["high"]["celsius"];       DHtemp2     = String(Htemp2);
    int Ltemp2        = forecast["forecastday"][1]["low"]["celsius"];        DLtemp2     = String(Ltemp2);
    int rain2         = forecast["forecastday"][1]["qpf_allday"]["mm"];      DRain2      = String(rain2)+"mm";
    if (Units == "M"){int w_mph2 = forecast["forecastday"][1]["avewind"]["kph"]; DW_mph2 = String(w_mph2)+"km/h";}
    else {int w_mph2  = forecast["forecastday"][1]["avewind"]["mph"];        DW_mph2     = String(w_mph2)+"mph";
    }
  }

  String icon_url2  = forecast["forecastday"][1]["icon_url"];           
  Dicon_url2        = icon_url2.substring(0,icon_url2.indexOf("/i/c/")+5) + icon_set + icon_url2.substring(icon_url2.indexOf("/i/c/")+6);
  String pop2       = forecast["forecastday"][1]["pop"];                     DPop2       = String(pop2);
  String w_dir2     = forecast["forecastday"][1]["avewind"]["dir"];          DW_dir2     = String(w_dir2);
  String w_dir_deg2 = forecast["forecastday"][1]["avewind"]["degrees"];      DW_dir_deg2 = String(w_dir_deg2);
  int humi2         = forecast["forecastday"][1]["avehumidity"];             DHumi2      = String(humi2);

  String WDay3      = forecast["forecastday"][2]["date"]["weekday"];         DWDay3      = WDay3;
  int DateDa3       = forecast["forecastday"][2]["date"]["day"];             DDateDa3    = DateDa3<10?"0"+String(DateDa3):String(DateDa3);
  String Mon3       = forecast["forecastday"][2]["date"]["monthname_short"]; DMon3       = Mon3;
  int DateYr3       = forecast["forecastday"][2]["date"]["year"];            DDateYr3    = String(DateYr3).substring(2);
  if (Units == "M" || Units == "X") {
    String icon3         = forecast["forecastday"][2]["icon"];               Dicon3      = String(icon3);
    String conditions3    = forecast["forecastday"][2]["conditions"];        Dconditions3   = String(conditions3);
    int Htemp3        = forecast["forecastday"][2]["high"]["celsius"];       DHtemp3     = String(Htemp3);
    int Ltemp3        = forecast["forecastday"][2]["low"]["celsius"];        DLtemp3     = String(Ltemp3);
    int rain3         = forecast["forecastday"][2]["qpf_allday"]["mm"];      DRain3      = String(rain3)+"mm";
    if (Units == "M") {int w_mph3 = forecast["forecastday"][2]["avewind"]["kph"]; DW_mph3 = String(w_mph3)+"km/h"; }
    else {int w_mph3  = forecast["forecastday"][2]["avewind"]["mph"];        DW_mph3     = String(w_mph3)+"mph"; }
  }
 
  String icon_url3  = forecast["forecastday"][2]["icon_url"];           
  Dicon_url3        = icon_url3.substring(0,icon_url3.indexOf("/i/c/")+5) + icon_set + icon_url3.substring(icon_url3.indexOf("/i/c/")+6);
  String pop3       = forecast["forecastday"][2]["pop"];                     DPop3       = String(pop3);
  String w_dir3     = forecast["forecastday"][2]["avewind"]["dir"];          DW_dir3     = String(w_dir3);
  String w_dir_deg3 = forecast["forecastday"][2]["avewind"]["degrees"];      DW_dir_deg3 = String(w_dir_deg3);
  int humi3         = forecast["forecastday"][2]["avehumidity"];             DHumi3      = String(humi3);

  String WDay4      = forecast["forecastday"][3]["date"]["weekday"];         DWDay4      = WDay4;
  int DateDa4       = forecast["forecastday"][3]["date"]["day"];             DDateDa4    = DateDa4<10?"0"+String(DateDa4):String(DateDa4);
  String Mon4       = forecast["forecastday"][3]["date"]["monthname_short"]; DMon4       = Mon4;
  int DateYr4       = forecast["forecastday"][3]["date"]["year"];            DDateYr4    = String(DateYr4).substring(2);
  if (Units == "M" || Units == "X") {
    String icon4         = forecast["forecastday"][3]["icon"];               Dicon4      = String(icon4);
    String conditions4    = forecast["forecastday"][3]["conditions"];        Dconditions4   = String(conditions4);
    int Htemp4        = forecast["forecastday"][3]["high"]["celsius"];       DHtemp4     = String(Htemp4);
    int Ltemp4        = forecast["forecastday"][3]["low"]["celsius"];        DLtemp4     = String(Ltemp4);
    int rain4         = forecast["forecastday"][3]["qpf_allday"]["mm"];      DRain4      = String(rain4)+"mm";
    if (Units == "M") {int w_mph4 = forecast["forecastday"][3]["avewind"]["kph"]; DW_mph4 = String(w_mph4)+"km/h";}
    else {int w_mph4  = forecast["forecastday"][3]["avewind"]["mph"];        DW_mph4     = String(w_mph4)+"mph";}
  }

  String icon_url4  = forecast["forecastday"][3]["icon_url"];           
  Dicon_url4        = icon_url4.substring(0,icon_url4.indexOf("/i/c/")+5) + icon_set + icon_url4.substring(icon_url4.indexOf("/i/c/")+6);
  String pop4       = forecast["forecastday"][3]["pop"];                     DPop4       = String(pop4);
  String w_dir4     = forecast["forecastday"][3]["avewind"]["dir"];          DW_dir4     = String(w_dir4);
  String w_dir_deg4 = forecast["forecastday"][3]["avewind"]["degrees"];      DW_dir_deg4 = String(w_dir_deg4);
  int humi4         = forecast["forecastday"][3]["avehumidity"];             DHumi4      = String(humi4);

  JsonObject& current = root["forecast"]["txt_forecast"];       
  String Txw1  = current ["forecastday"][0]["fcttext_metric"];  // Read the text describing the forecast.
  Txw1.toCharArray(Txtweather, 200);                            // Convert String to CharArray for Nextion
 return true;
}


bool showWeather_conditions(char *json)
{// Convert forecast received data into Strings and values 
  DynamicJsonBuffer jsonBuffer(8704);
  char *jsonstart = strchr(json, '{');
//Serial.println(F("jsonstart ")); Serial.println(jsonstart); // This is input data
  if (jsonstart == NULL) {
    Serial.println(F("JSON data missing"));
    return false; }
  json = jsonstart;
  // Parse JSON
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
    return false; }
  JsonObject& current = root["current_observation"];
  const float temp_c = current["temp_c"];         // Extract local temp. now
  const float wind_c = current["wind_kph"];       // Extract local wind speed now
  const char  h_c = current["relative_humidity"]; // Extract local humidity now
  temp_e=temp_c+0.5;                              // Round the decimal value                   
  wind_e=String(wind_c,0)+"km/h";                 // Convert wind speed to string

  JsonObject& currentup = root["sun_phase"]["sunrise"];
  String h_u = currentup["hour"];                  // Extract local Sunrise hour
  h_up=String(h_u);
  String m_u = currentup["minute"];                // Extract local Sunrise minute
  m_up=String(m_u);
  JsonObject& currentset = root["sun_phase"]["sunset"];
  String h_s = currentset["hour"];                  // Extract local Sunrise hour
  h_set=String(h_s);
  String m_s = currentset["minute"];                // Extract local Sunrise minute
  m_set=String(m_s);
  return true;                                      // All conversions worked, return True
}
