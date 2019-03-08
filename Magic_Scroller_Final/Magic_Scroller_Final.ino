/* ReadMe
   *  MagicScroller
   *  Program to exercise the MD_MAX72XX library version 2.9
   *  MD_Parola 2.6.2
   *  Uses most of the functions in the library
   *  
   * 20170428: Sometimes ESP losses connection with ADSL router. I need to reset either of ADSL or ESP to get it connected again. 
   * I will now ping gateway. If GW is not reachable and none of my devices are "ON", I will reset ESP.
   * 
   * 20170428: Include the last power-on time in webpage. 
   * 20171028: Save file in spiff and scroll random lines of quotes, facts, taglines
   * 20171030: New NTP, WiFi Manager, Move web page to PROGMEM
   * 
   * 20180210: WiFi Manager does not set dns ip which is required to resolve newNTP NTP server hostname. Following line changed in tzapu library WiFiManager.cpp. I used my router GW as dns server IP.
   * WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn, _sta_static_gw); //set GW as dns server -SH
   * Originaly it was WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
   * 
   * 20180321: One Button added.
   * 
   * ToDo: Calendar function should scroll between event time only.
   * RTC Integration see following to set time in RTC
   * https://forum.arduino.cc/index.php?topic=128928.0
   * http://www.instructables.com/id/Setting-the-DS1307-Real-Time-Clock-using-the-Seria/
   * 
   * RSS FEED LIST - https://www.repeatserver.com/RSSFeedList.aspx; http://zeenews.india.com/rss.html
   * http://feeds.bbci.co.uk/news/world/rss.xml
   * http://www.moneycontrol.com/rss/latestnews.xml
   * http://www.business-standard.com/rss/current-affairs-115.rss
   * 
   * const int sleepTimeS = 10;
   * Serial.println("ESP8266 in sleep mode");
   * ESP.shutDisplay(sleepTimeS * 1000000);
   * 
   */
  

#include <FS.h>                   //(for WiFiManager) this needs to be first, or it all crashes and burns...
#include <DNSServer.h>            //(for WiFiManager)
#include <WiFiManager.h>          //(for WiFiManager)https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //(for WiFiManager)https://github.com/bblanchon/ArduinoJson


#include <NTPClient.h> //Use Library: https://github.com/taranais/NTPClient This has getFormattedDate function.

#include <MD_Parola.h>    //Display Zone management
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "Font_Data.h"
#include <ESP8266WiFi.h>                //https://github.com/esp8266/Arduino
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include "index.h" //Our HTML webpage contents
#include <WiFiUdp.h>

#include <TimeLib.h> //used in function chkEvents

#include "PMButton.h"   //https://github.com/JCWentzel/PolymorphicButtons


/* for normal hardware wire use below */
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
/* for normal hardware wire use above */


//Put day and month strings in PROGMEM to save RAM
//const char monthShortNames_P[] PROGMEM = "ErrJanFebMarAprMayJunJulAugSepOctNovDec";
//const char dayShortNames_P[] PROGMEM = "SunMonTueWedThuFriSatErr";
//static char buffer[4];  // must be big enough for longest string and the terminating null. In our case it is 3+1

String myTime;
String myDate;
String userInput="timeDisplay";
String stringCalendar1;
float temp;
uint8_t scrollPt=0;
uint8_t intensity=8;
boolean breakLoop=false;
uint8_t displayItem=0;
boolean shutDisplay=false;
//const int sleepTimeS = 10;

//include ESP8266 SDK C functions to get heap
extern "C" {                                          // ESP8266 SDK C functions
#include "user_interface.h"
} 

// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define  PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTD(x) Serial.println(x, DEC)

#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTD(x)

#endif


ESP8266WebServer server(80);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//NTP Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800, 60000);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//WiFi Manager Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char mqtt_server[4]; //default40
char mqtt_port[6] = "8080";
char blynk_token[12] = "BLYNK_TOKEN"; //default 33


//default custom static IP
char static_ip[16] = "192.168.1.152";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  PRINTS("\nShould save config");
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//OTA Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <ESP8266HTTPUpdateServer.h>
const char* otahost = "magicscroller-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(90);
ESP8266HTTPUpdateServer httpUpdater;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Calendar events Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <WiFiClientSecure.h>
const char* googleHost = "script.google.com";
const char* googleRedirHost = "script.googleusercontent.com";
const char *GScriptId = "AKfycbykmeIqlL_GbYduAfWaizlo35y9l_zOfPLXPCfjki5bvmnSUtLe";
String calUrl = String("/macros/s/") + GScriptId + "/exec?cal";
String redirUrl;

//const char* host = "api.github.com";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
//const char* fingerprint = "CF 05 98 89 CA FF 8E D8 5E 5C E0 C2 E4 F7 E6 C3 C7 50 DD 5C";

const char* fingerprint = "4"; //"4A:49:82:5B:49:6F:B2:1C:67:9B:0A:4A:24:AE:43:1F:80:1F:62:F5";
//const char* fingerprint2 = "94:21:5D:7B:55:41:70:88:70:BF:97:64:36:E2:90:28:71:0D:BD:99";

//String events[10];         //array to store calendar events. Match the number with int maxEvents. Default value is 20.
byte maxEvents = 10;   // How many maximum events to be extracted from calendar (in-case calendar returns many events). Default value is 20.
byte eventsCount;       //this parameter remembers how many events extracted from calendar and later used to scroll in loop.

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//News Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//common parameters
byte worded = 0;           //counter to check how many charecters matched while matching target.
byte maxHeadlines = 20;   // How many maximum headlines to be extracted from rss feed. Default value is 20.

/* TOI switched to https so will not work
//TimesOfIndia
const char* hostTOI = "timesofindia.indiatimes.com";
String urlTOI = "/rssfeeds/-2128936835.cms";     // Times of india feed india
char targetTOI[] = {'<', 'i', 't', 'e', 'm', '>', '<', 't', 'i', 't', 'l', 'e', '>' };
byte headLineCountTOI;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String newsTOI[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.


//Times of india feed top headlines
const char* hostTOI2 = "timesofindia.indiatimes.com";
String urlTOI2 = "/rssfeedstopstories.cms";       // Times of india feed top headlines,feed not working as it changed to https
char targetTOI2[] = {'<', 'i', 't', 'e', 'm', '>', '<', 't', 'i', 't', 'l', 'e', '>' };
byte headLineCountTOI2;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String newsTOI2[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.
*/

//BBC
const char* hostBBC = "feeds.bbci.co.uk";
String urlBBC = "/news/world/rss.xml";  //world news
char targetBBC[] = {'<', '!', '[', 'C', 'D', 'A', 'T', 'A',0x00}; //null terminated so that strlen can count length of array.
byte headLineCountBBC;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.



//business-standard
const char* hostBS = "business-standard.com";
String urlBS = "/rss/current-affairs-115.rss";  //Business-standard
//char targetIT[] = {'<', 'i', 't', 'e', 'm', '>', '\n', '<', 't', 'i', 't', 'l', 'e', '>' };
char targetBS[] = {'<', 't', 'i', 't', 'l', 'e', '>','<', '!', '[', 'C', 'D', 'A', 'T', 'A',0x00};
//char targetIT[] = {'<', '!', '[', 'C', 'D', 'A', 'T', 'A',0x00 }; //null terminated so that strlen can count length of array.
byte headLineCountBS;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String newsIT[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.


//zee
const char* hostZee = "zeenews.india.com";
String urlZBT = "/rss/india-national-news.xml";  //world news
char targetZee[] = {'<', 't', 'i', 't', 'l', 'e', '>','<', '!', '[', 'C', 'D', 'A', 'T', 'A', 0x00};
byte headLineCountZB;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.

//http://zeenews.india.com/rss/business.xml
String urlZBE = "/rss/business.xml";  

//http://zeenews.india.com/rss/india-news.xml
String urlZBP = "/rss/india-news.xml";  

//  http://zeenews.india.com/rss/world-news.xml
String urlZBwP = "/rss/world-news.xml";  

//http://zeenews.india.com/rss/health-news.xml
String urlZBM = "/rss/health-news.xml"; 


//www.financialexpress.com/feed/
const char* hostFE = "financialexpress.com";
String urlFE = "/feed"; 
char targetFE[] = {'<', 'i', 't', 'e', 'm', '>', '\n', '<', 't', 'i', 't', 'l', 'e', '>',0x00 };
byte headLineCountFE;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.




//quote of the day author http://feeds.feedburner.com/brainyquote/QUOTEBR
const char* hostQod = "feeds.feedburner.com"; //do not prefix http://www.
String urlQod = "/brainyquote/QUOTEBR";  
char targetQod[] = {'<', 't', 'i', 't', 'l', 'e', '>'};
byte countQod;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String qodA[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 10.

//quote of the day description http://feeds.feedburner.com/brainyquote/QUOTEBR
char targetQodD[] = {'<', 'd', 'e', 's', 'c', 'r', 'i', 'p', 't', 'i', 'o', 'n', '>'};
byte countQodD;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String qodD[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 10.



//crickinfo section
const char* hostcricket = "static.cricinfo.com"; //do not prefix http://www.
String urlcricket = "/rss/livescores.xml";  //india today feed not working
char targetcricket[] = {'<', 'i', 't', 'e', 'm', '>', '\n', '<', 't', 'i', 't', 'l', 'e', '>',0x00 };

byte headLineCountcricket;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
//String newscricket[10];

/*
//Dictionary.com Word of the Day http://www.macmillandictionary.com/wotd/wotdrss.xml
const char* hostDect = "macmillandictionary.com"; //do not prefix http://www.
String urlDect = "/wotd/wotdrss.xml";  
char targetDect[] = {'<', 't', 'i', 't', 'l', 'e', '>'};
byte countDect;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
String dect[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.
//Defination
char targetDectD[] = {'<', 's', 'u', 'm', 'm', 'a', 'r', 'y', '>'};
byte countDectD;       //this parameter remembers how many headlines extracted from rss feed and later used to scroll in loop.
String dectD[10];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Motion Detection PIR
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
byte calibrationTime = 5;        
//int pirPin = D4;    //Define with other pins in following lines. the digital pin connected to the PIR sensor's output
boolean scrollOn = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RTC CONNECTIONS:
// DS3231 SDA --> SDA
// DS3231 SCL --> SCL
// DS3231 VCC --> 3.3v or 5v
// DS3231 GND --> GND


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Pin assignment and LED Matrix related stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may 
// need to be adapted
//RTC on pins D1 and D2
#define MAX_DEVICES 8
#define MAX_ZONES 2

#define  CLK_PIN  D5  // or SCK --GPIO14
#define DATA_PIN  D7  // or MOSI --GPIO13
#define CS_PIN    D8  // or SS -- GPIO15
#define pirPin    D4  // pir pin
#define tempPin A0

PMButton button1(D3);

// SPI hardware interface
//MD_MAX72XX mx = MD_MAX72XX(CS_PIN, MAX_DEVICES); uncomment when not using parola
MD_Parola P = MD_Parola(CS_PIN, MAX_DEVICES);

// Arbitrary pins
// MD_MAX72XX mx = MD_MAX72XX(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// We always wait a bit between updates of the display 
#define  DELAYTIME  33  // in milliseconds default 100 (it is scroll speed).tested works well at 40.
uint16_t SPEED_TIME = 25;    //for Parola The speed of the display is the 'tick' time between animation frames. The lower this time the faster the animation; set it to zero to run as fast as possible.
#define PAUSE_TIME  1000   //for Parola. Between each entry and exit, the library will pause by the number of milliseconds specified to allow the viewer to read the message. For continuous scrolling displays this should be set to the same value as the display speed.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

//webserver to select content to be scrolled
void handleRoot(){
  Serial.println(server.arg(0) + "##" + server.arg(1) + "##" + server.arg(2)+"##"+server.arg(3) + "##" + server.arg(4) + "##" + server.arg(5) + "##" + server.arg(6)); 

//set intensity
if (server.arg("intensity").toInt() >= 0) {
  intensity = server.arg("intensity").toInt() ;
  P.setIntensity(intensity);}
Serial.print("Handle Intensity set to = ");
Serial.println(intensity);


//set Scroll speed
if (server.arg("speedTime").toInt() > 0) {
  SPEED_TIME = server.arg("speedTime").toInt() ;
  }
Serial.print("Scroll Speed set to = ");
Serial.println(SPEED_TIME);


  if (server.arg(0).length() > 2){    //if arguments present in url then set userinput 
    userInput=server.arg(0) + "##" + server.arg(1) + "##" + server.arg(2)+"##"+server.arg(3) + "##" + server.arg(4) + "##" + server.arg(5) + "##" + server.arg(6)+ "##"  + server.arg(7)+ "##"  + server.arg(8)+ "##"  + server.arg(9)+ "##"  + server.arg(10)+ "##"  + "WelcomeMsg=" + server.arg("WelcomeMsg");
  }


setZones();
  
PRINT("\nUserInput received is " , userInput);
String webpage = MAIN_page;
  server.send(200, "text/html", webpage);
  breakLoop=true;
 }


///////////////////////////////////////////////////
void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
 }



void setup(){
#if  DEBUG
  Serial.begin(115200);
  delay(1000);
#endif
  
  P.begin(MAX_ZONES);
  P.setInvert(false);

  P.setZone(0, 0, 7);
  
  //P.setZone(1, 5, 7);

/////////////////////////////////RTC Code/////////////////////////
 Rtc.Begin();

 if (!Rtc.IsDateTimeValid()) 
    {
        // Common Cuases:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");

        // following line sets the RTC to the date & time this sketch was compiled
        // it will also reset the valid flag internally unless the Rtc device is
        // having an issue

       // Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

// never assume the Rtc was last configured by you, so
// just clear them to your needed state
     Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
///////////////////////////////////////////////////////////////////
////setup buttons
  button1.begin();
   
  //You can set button timing values for each button to fine tune interaction.
 // button1.debounce(20);//Default is 10 milliseconds
 // button1.dcGap(300);//Time between clicks for Double click. Default is 200 milliseconds
 // button1.holdTime(3000);//Default is 2 seconds
 // button1.longHoldTime(8000);//Default is 5 seconds

//////////////////////////////////////////////////////////////////

//mx.begin(); uncomment if not using parola
PRINTS("\n[MD_MAX72XX Test & Demo]");
scrollString("MAGIC SCROLLER    "); //uncomment if not using parola

manageWiFi();

if (WiFi.status() == WL_CONNECTED) {
PRINT("\nIP number assigned is ", WiFi.localIP());
scrollString("WIFI connected    ");
delay(250);
timeClient.begin();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//OTA Code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MDNS.begin(otahost);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 90);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", otahost, update_path, update_username, update_password);

 //OTA code end
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Webserver code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
 
  server.begin();
  PRINTS("\nHTTP server started");
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
else { scrollString("WiFi not connected    ");}



//Motion Detection Interrupt code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    pinMode(pirPin, INPUT);
    digitalWrite(pirPin, LOW);

//give the sensor some time to calibrate
      PRINTS("\nCalibrating sensor ");
      scrollString("Calibrating Motion Sensor ...   ");
        for(byte i = 0; i < calibrationTime; i++){
          PRINTS(".");
          scrollString("...");
          delay(1000);
          }
        PRINTS(" Done. SENSOR ACTIVE");
        
        delay(50);
  
//attachInterrupt(pirPin, setScrollOn, RISING); // enable the interrupt , and when D2 rising jump into the blink() function. (Disabled due to false trigger of pir)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


}

void loop() {

//  PRINT("Heap in loop start: ", system_get_free_heap_size());
breakLoop=false; //set breakLoop to false. we will check breakLoop in loop,if it's true, we will reastart loop from top.
    
    
Serial.println(displayItem);
////////////////////////////////////////////////////////////////////////////
  //check_switches to get the current state
  button1.checkSwitch();

  //used to see the state change. shutDisplay flag also set in this.
  useButonCheck();

   
  if (breakLoop) {return ;} // Restart main program loop on button press
////////////////////////////////////////////////////////////////////////////



  
    if (WiFi.status() == WL_CONNECTED) {
       timeClient.update();  //NTP
       httpServer.handleClient(); // OTA code
       server.handleClient(); //webpage handle for user input
    }
  
  yield();

//print RTC temp
    //RtcTemperature temp = Rtc.GetTemperature();
   //Serial.print(temp.AsFloat());
    //Serial.println("C");
// print RTC data end 
  
// If 02 Zones selected in UI then zone-1 is for clock
zoneOneTimePrint();

  
  if (scrollPt==14) {scrollPt=0;}
//  Serial.println();
//  Serial.println(scrollPt);

 //Scroll messages if motion detected 
  if(digitalRead(pirPin) == HIGH){  //its time to scroll messages
  //PRINTS ("\nMotion detected");
   //scrollOn = false ;
  ledMatrixScroll();
    }     
    else {
    PRINTS ("\nMotion not detected");
    }
P.displayAnimate();

}// loop ends here


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//LED Matrix Scroll functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ledMatrixScroll(){      //set here messages to scroll

if ((scrollPt==0)&& P.getZoneStatus(0)){
  scrollPt++;    //this makes scrollpt=1

if(userInput.indexOf("welcome message") != -1){
  int index1=userInput.indexOf("Msg=");
  int index2=userInput.length();
String welcome=userInput.substring(index1+4,index2);

PRINT("\nindex1=",index1); PRINT("\nindex2=",index2); PRINT("\nwelcome=",welcome);

if(timeClient.getHours() >= 3 && timeClient.getHours() <= 12){
welcome = "Good Morning !!!   " + welcome;
}
else if(timeClient.getHours() > 12 && timeClient.getHours() <= 16){
welcome = "Good Afternoon !!!  " + welcome;
}
else {
welcome = "Good Evening !!!  " + welcome;
}

PRINTS("\nScrolling welcome text\n");
scrollString(welcome);

//P.setFont(0);
//P.displayZoneText(0, const_cast<char*>(welcome.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
//P.displayReset(0);  
}
}



//Scroll Time
if ((scrollPt==1)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=2
if(userInput.indexOf("timeDisplay") != -1){
    if (WiFi.status() != WL_CONNECTED) {
     //set time string
    RtcDateTime now = Rtc.GetDateTime();
    printTime(now); //this sets myTime string to RTC time     
    }
    else {    myTime=timeClient.getFormattedTime();} // time in hh:mm:ss format
          //scrollString(myTime);
          P.setFont(0, dig6x8);
          P.displayZoneText(0, const_cast<char*>(myTime.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
          P.displayReset(0); 
        }
}


//Scroll Date
if ((scrollPt==2)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=3
  if ((userInput.indexOf("2Zone") >= 0)){
  dateDisplay();
}
}

server.handleClient();
button1.checkSwitch();
useButonCheck();
if (breakLoop) {return ;} // Restart main program loop on button press

//Scroll temp
if ((scrollPt==3)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=4
if(userInput.indexOf("temperature") != -1){
temperature();
String myTemp = "Temp: ";
myTemp = myTemp + temp + " Deg";
scrollString(myTemp);
       // P.setFont(0);
       // P.displayZoneText(0, const_cast<char*>(myTemp.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
       // P.displayReset(0); 
//dtostrf(temp,5, 2, tempChar);
yield();
}
}



//Scroll Calendar
if ((scrollPt==4)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=5
if(userInput.indexOf("calendar events") != -1){
//call events, news and Qod functions
getCalendarEvents();
//scrollstrArr(events , eventsCount );
}
}


//Scroll cricket
if ((scrollPt==5)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=6
if(userInput.indexOf("cricket") != -1){
  ///scrollText("Live cricket scores from crickinfo.com");
  newGETnews(hostcricket, urlcricket, targetcricket, headLineCountcricket);
//GETnews(newscricket, hostcricket, urlcricket, targetcricket, headLineCountcricket);
//scrollstrArr(newscricket, headLineCountcricket );
}
}


//Scroll QOD
if ((scrollPt==6)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=7
if(userInput.indexOf("qod") != -1){
String qodA[8];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 8.
String qodD[8];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 8.
GETnews(qodA, hostQod, urlQod, targetQod, countQod);
GETnews(qodD, hostQod, urlQod, targetQodD, countQodD);
//Join Description and Auther
for(byte i = 0; i < countQodD; i++){
qodD[i].concat(" -> ");
qodD[i] += qodA[i];
}
scrollstrArr(qodD , countQodD );
}
}


//Scroll ITnews
if ((scrollPt==7)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=8
if(userInput.indexOf("bbc") != -1){
newGETnews(hostBBC, urlBBC, targetBBC, headLineCountBBC);
//GETnews(newsIT, hostIT, urlIT, targetIT, headLineCountIT);
//scrollstrArr(newsIT, headLineCountIT );
}
}


//Scroll Zee news
if ((scrollPt==8)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=9
if(userInput.indexOf("zee") != -1){
newGETnews(hostZee, urlZBT, targetZee, headLineCountZB);
newGETnews(hostZee, urlZBE, targetZee, headLineCountZB);
newGETnews(hostZee, urlZBP, targetZee, headLineCountZB);
newGETnews(hostZee, urlZBwP, targetZee, headLineCountZB);
newGETnews(hostZee, urlZBM, targetZee, headLineCountZB);
}
}


//GETnews(dect, hostDect, urlDect, targetDect, countDect);
//GETnews(dectD, hostDect, urlDect, targetDectD, countDectD);


//Scroll quotes randomLineFromSpiffFile
if ((scrollPt==9)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=10
 if(userInput.indexOf("spiff_quote") != -1){ 
  scrollString(randomLineFromSpiffFile("/quotes.txt", 1659, 100));
       // P.setFont(0);
       // P.displayZoneText(0, const_cast<char*>(randomLineFromSpiffFile("/quotes.txt", 1659, 100).c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
       // P.displayReset(0); 
  }
  }
 

 //Scroll facts randomLineFromSpiffFile
if ((scrollPt==10)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=11
 if(userInput.indexOf("spiff_facts") != -1){ 
    scrollString(randomLineFromSpiffFile("/facts.txt", 3226, 91));    
        //P.setFont(0);
        //P.displayZoneText(0, const_cast<char*>(randomLineFromSpiffFile("/facts.txt", 3226, 91).c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        //P.displayReset(0); 
  }
  }
 

  //Scroll taglines randomLineFromSpiffFile
if ((scrollPt==11)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=12
 if(userInput.indexOf("spiff_taglines") != -1){ 
  scrollString(randomLineFromSpiffFile("/taglines.txt", 373, 40));
       // P.setFont(0);
       // P.displayZoneText(0, const_cast<char*>(randomLineFromSpiffFile("/taglines.txt", 373, 40).c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
       // P.displayReset(0); 
  }
  }


//Scroll business standard
if ((scrollPt==12)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=13
if(userInput.indexOf("bsnews") != -1){
newGETnews(hostBS, urlBS, targetBS, headLineCountBS);
}
}


//Scroll Finencial Exp
if ((scrollPt==13)&& P.getZoneStatus(0)){
  scrollPt++;   //this makes scrollpt=14
if(userInput.indexOf("finexp") != -1){
newGETnews(hostFE, urlFE, targetFE, headLineCountFE);
}
}

 
///scrollString(randomLineFromSpiffFile("/quotes.txt", 1659, 100));
///scrollString(randomLineFromSpiffFile("/facts.txt", 3226, 91));
///scrollString(randomLineFromSpiffFile("/taglines.txt", 373, 40));
///mx.clear();  

////PRINTS("\nscroll function end");
} // ledMatrixScroll ends here





 /*
////////////////
void ledMatrixScroll(){      //set here messages to scroll

if ((scrollPt==0)&& P.getZoneStatus(0)){
  scrollPt++;

if(userInput.indexOf("welcome message") != -1){
  int index1=userInput.indexOf("Msg=");
    int index2=userInput.length();
String welcome=userInput.substring(index1+4,index2);

PRINT("\nindex1=",index1);
PRINT("\nindex2=",index2);
PRINT("\nwelcome=",welcome);

if(timeClient.getHours() >= 3 && timeClient.getHours() <= 12){
///scrollText("Good Morning !!!  ");
}
else if(timeClient.getHours() > 12 && timeClient.getHours() <= 16){
///scrollText("Good Afternoon !!!  ");
}
else {
///scrollText("Good Evening !!!  ");
}
//Serial.println("Hariramani Mohita Suresh Laveena and Vatsal Welcome You !!!  ");

///scrollString(welcome);
PRINTS("\nScrolling text\n");

P.displayZoneText(0, const_cast<char*>(welcome.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
P.displayReset(0);
}
}
timeDisplay(); 

if ((scrollPt==1)&& P.getZoneStatus(0)){
  scrollPt++;
dateDisplay();
}

server.handleClient();
 /// Scroll temp
 
if(userInput.indexOf("temperature") != -1){
temperature();
char tempChar[7];
dtostrf(temp,5, 2, tempChar);
///scrollText("Temp");
///scrollText(tempChar);
///scrollText("Degree");
yield();
}

///bullseye();

if(userInput.indexOf("calendar events") != -1){
//call events, news and Qod functions
getCalendarEvents();
//scrollstrArr(events , eventsCount );
}

if(userInput.indexOf("cricket") != -1){
  ///scrollText("Live cricket scores from crickinfo.com");
  newGETnews(hostcricket, urlcricket, targetcricket, headLineCountcricket);
//GETnews(newscricket, hostcricket, urlcricket, targetcricket, headLineCountcricket);
//scrollstrArr(newscricket, headLineCountcricket );
}

if(userInput.indexOf("qod") != -1){
String qodA[8];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 8.
String qodD[8];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 8.
GETnews(qodA, hostQod, urlQod, targetQod, countQod);
GETnews(qodD, hostQod, urlQod, targetQodD, countQodD);
//Join Description and Auther
for(byte i = 0; i < countQodD; i++){
qodD[i].concat(" -> ");
qodD[i] += qodA[i];
}
///scrollstrArr(qodD , countQodD );
}

if(userInput.indexOf("ITnews") != -1){
newGETnews(hostIT, urlIT, targetIT, headLineCountIT);
//GETnews(newsIT, hostIT, urlIT, targetIT, headLineCountIT);
//scrollstrArr(newsIT, headLineCountIT );
}

if(userInput.indexOf("Zeebiz") != -1){
newGETnews(hostTOI, urlTOI, targetTOI, headLineCountTOI);
//GETnews(newsTOI, hostTOI, urlTOI, targetTOI, headLineCountTOI);
//scrollstrArr(newsTOI, headLineCountTOI );

newGETnews(hostTOI2, urlTOI2, targetTOI2, headLineCountTOI2);
//GETnews(newsTOI2, hostTOI2, urlTOI2, targetTOI2, headLineCountTOI2);
//scrollstrArr(newsTOI2, headLineCountTOI2 );
}


//GETnews(dect, hostDect, urlDect, targetDect, countDect);
//GETnews(dectD, hostDect, urlDect, targetDectD, countDectD);

///scrollString(randomLineFromSpiffFile("/quotes.txt", 1659, 100));
///scrollString(randomLineFromSpiffFile("/facts.txt", 3226, 91));
///scrollString(randomLineFromSpiffFile("/taglines.txt", 373, 40));


///mx.clear();  
PRINTS("\nscroll function end");
} // ledMatrixScroll ends here

*/
//////////////////////////////////////////////////////
//Scroll string array if motion detected
void scrollstrArr(String *strArr, byte& strCount ){
  if(digitalRead(pirPin) == HIGH){
  for(int i=0; i<(strCount+1); i++){

    if (P.displayAnimate())  {  // animates and returns true when an animation is completed
   
zoneOneTimePrint();


       if (P.getZoneStatus(0))
      {
        P.setFont(0);
        P.displayZoneText(0, const_cast<char*>(strArr[i].c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        P.displayReset(0);
        Serial.println();
        Serial.println(i + "  strArr " + strArr[i]);
      }
    }
  while (P.getZoneStatus(0) == false) {server.handleClient();button1.checkSwitch();useButonCheck();if (breakLoop) {return ;} zoneOneTimePrint();P.displayAnimate(); yield();} //Do nothing and wait for scroll to end
  
  yield();
                }//for end here
          } //if end here
} 
//////////////////////////////////////////////////////////////////
//Scroll string  if motion detected
void scrollString(String str2scroll){
  if(digitalRead(pirPin) == HIGH){
  
          if (P.displayAnimate())  {  // animates and returns true when an animation is completed
              zoneOneTimePrint();
      
             if (P.getZoneStatus(0))
            {
              P.setFont(0);
              P.displayZoneText(0, const_cast<char*>(str2scroll.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
              P.displayReset(0);
              Serial.println();
              Serial.println("  strArr " + str2scroll);
            }
          }
  while (P.getZoneStatus(0) == false) {server.handleClient();button1.checkSwitch();useButonCheck();if (breakLoop) {return ;} zoneOneTimePrint();P.displayAnimate(); yield();} //Do nothing and wait for scroll to end
  server.handleClient();
  yield();
              
          } //if end here
       server.handleClient(); //webpage handle for user input
       button1.checkSwitch();useButonCheck(); //check button press
       if (breakLoop) {return ;} 
} 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Scroll Time and Date Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void timeDisplay() {
// Populate myTime String
  myTime=timeClient.getFormattedTime();
  myTime= (" " + myTime + "               ");
  Serial.println("\nmyTime: " + myTime);


int myTime_len = myTime.length() + 1;
char myTime_charArray[myTime_len];
myTime.toCharArray(myTime_charArray, myTime_len);

PRINT("myTime_charArray: ",myTime_charArray);
//Serial.println(myTime_charArray);
//scrollText(myTime_charArray);
}
*/

void dateDisplay() {
uint8_t myMonth = timeClient.getFormattedDate().substring(5,7).toInt();
 
   // Populate myDate String
myDate= dayShortStr(timeClient.getDay()+1) ;
myDate= myDate + "  " + timeClient.getFormattedDate().substring(8,10) + " " +monthShortStr(myMonth) + " " +  timeClient.getFormattedDate().substring(0,4) ;
myDate= (" " + myDate + "               ");
PRINT("\nmyDate: " , myDate);

PRINTS("\nScrolling myDate\n");
//P.displayZoneText(0, const_cast<char*>(myDate.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
P.setFont(0);
P.displayZoneText(0, const_cast<char*>(myDate.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
P.displayReset(0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Day and Month name string Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
char* monthShortStr(uint8_t month)
{
   for (int i=0; i < 3; i++)      
      buffer[i] = pgm_read_byte(&(monthShortNames_P[i+ (month*3)]));  
   buffer[3] = 0;
   return buffer;
}



char* dayShortStr(uint8_t day) 
{
   uint8_t index = day*3;
   for (int i=0; i < 3; i++)      
      buffer[i] = pgm_read_byte(&(dayShortNames_P[index + i]));  
   buffer[3] = 0; 
   return buffer;
}
*/ 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------Set Motion Detected  ----------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setScrollOn() { scrollOn = true;
PRINTS("\nmotion detectted-true set");
}        // Set Motion Detected




////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//News Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GETnews(String *news, const char* host, String& url, char* target, byte& headLineCount  ){

 PRINT("connecting to " ,host );
 
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    PRINTS("\nConnection failed");
    return;
  }
  
  
  
  PRINT("Requesting URL: " , url);
  
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      PRINTS("\n>>> Client Timeout !");
      client.stop();
      return;
    }
  yield(); //inserted by suresh   
  }

  headLineCount=0;
  // Read all the lines of the reply from server and print them to Serial
  while (client.available()){
    //String line = client.readStringUntil('\r');   // uncomment if the response has many lines
    //Serial.print(line);


//Suresh code to get TITLE Strings
char c = client.read();
//Serial.println(c);
if(c == '<'){
  worded= 0;
  delay(1);
 //Serial.println("Start Match ITEM TITLE worded=" + worded);
  
    for(byte i=0; i<strlen(target); i++){
  //Serial.print(c);
  //Serial.print(c + 0);
 //Serial.println();
  //Serial.println(target[i]);
  //Serial.println(worded);
  
                  if(c == target[i]){ 
                    worded=++worded; }
                    else {
                      //Serial.println("breaking");
                      break;}

                    
                  if(i==(strlen(target)-1) && worded==strlen(target)){
                    ++headLineCount;
                      news[headLineCount] = client.readStringUntil('<');
                      Serial.println();
                       PRINT("\nstored in ",headLineCount);
                      Serial.println(news[headLineCount]);
                    //  Serial.println();
                      
                   }    //end of news if
                  
                  
     c = client.read();


     
     }             //end of for loop              
                  
                  
    
//String line = client.readStringUntil('\r');
 // Serial.println(line);

}  //end of '<' if
//Suresh Code ends here

     //c = client.read();

if (headLineCount==(maxHeadlines - 1)) { break;} // break out from while loop if we already have  extracted headlines defined by maxHeadlines parameter.
     
  } // end of while
  
  Serial.println();
  PRINTS("\nClosing connection");

}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Temperature Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void temperature() {
int val = analogRead(tempPin);         //Read from LM35
float mv = ( val/1024.0)*3300;         //5000 for 5v vcc or 3300 for 3.3v vcc
temp = mv/10;                          //temperature in cel
//float farh = (cel*9)/5 + 32;          //temperature in farenhite
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CalendarEvents Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void getCalendarEvents() {

  String events[maxEvents];         //array to store calendar events. Match the number with int maxEvents. Default value is 20.
// Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
   
//connect and send google script
  PRINT("\nConnecting to " , googleHost);

PRINT("Heap in getCalendarEvents start: ", system_get_free_heap_size());
  
    if (!client.connect(googleHost, httpsPort)) {
    PRINTS("\nConnection failed");
    return;
  }

PRINTS("\nConnection made");

  if (client.verify(fingerprint, googleHost)) {        //match certificates. Note- processing will continue evenif certificates do match. As of now no action is taken when certificates do not match.
    PRINTS("\nCertificate matches");
  } else {
    PRINTS("\nCertificate doesn't match");
  }

  PRINT("\nRequesting URL: ",calUrl );
  

  client.print(String("GET ") + calUrl + " HTTP/1.1\r\n" +
               "Host: " + googleHost + "\r\n" +
               "User-Agent: myESP8266\r\n" +
               "Connection: close\r\n\r\n");

  PRINTS("\nRequest sent");



unsigned long startTime = millis();
unsigned int httpResponseTimeOut = 5000; // 5 sec
  
  while (client.connected() && ((millis() - startTime) < httpResponseTimeOut)) {
      if (client.available()) {
              String line = client.readStringUntil('\n');
    
              if (line == "\r") {
                PRINTS("\nHeaders received");
                break;      //exit from while loop
              } //endif

              if (line.startsWith("Location")) {
                redirUrl=line;
                PRINT("/nredirUrl found:" , line);
               } //endif
      }
     else {
          PRINTS(".");
          delay(100);
      }
      
    yield(); // inserted by suresh
  } //end of while

  // Exit function if redirUrl is empty
  if (redirUrl.length() < 2) { return ;}

  //Serial.print("Heap in google function Line566: ");
  //Serial.println(system_get_free_heap_size());

//Connect to redirect url
PRINT("\nConnecting to ",googleRedirHost );

PRINT("Heap Connecting to googleRedirHost: ", system_get_free_heap_size());
  
  if (!client.connect(googleRedirHost, httpsPort)) {
    PRINTS("\nConnection failed");
    return;                                             //come out of function if googleRedirHost is not connected.
  }

  if (client.verify(fingerprint, googleRedirHost)) {        //match certificates. Note- processing will continue evenif certificates do match. As of now no action is taken when certificates do not match.
    PRINTS("\nCertificate matches");
  } else {
    PRINTS("\nCertificate doesn't match");
  }

  //Now fatch the data

  PRINT("\nRequesting URL: ",redirUrl );
  
  redirUrl.remove(0,46);  //Remove Location http
  PRINT("\nAfterCut" , redirUrl);
              
  client.print(String("GET ") + redirUrl + " HTTP/1.1\r\n" +
               "Host: " + googleRedirHost + "\r\n" +
               "User-Agent: myESP8266\r\n" +
               "Connection: close\r\n\r\n");


  PRINTS("\nRequest sent");

eventsCount=0;          //reset events counter to zero


    
startTime = millis();
//unsigned long httpResponseTimeOut = 5000; // 5 sec
  while (client.connected() && ((millis() - startTime) < httpResponseTimeOut)) {
         if (client.available()) {
            String line = client.readStringUntil('\n');
    
            //Serial.println("events received:" + line);
 
                       if (line.indexOf("scroll") != -1) {
                          PRINT("events received:" , line);
                      
                                            ++eventsCount;
                                            //line.remove(0,51);  //Remove scroll (earlier incase only scroll prefix then removed 7) [this line not required if using function chkEvents(events), as we will like to preserve timestamps]
                                            events[eventsCount] = line;
                                            Serial.println();
                                             PRINT("\nStored in " , eventsCount);
                                            Serial.println(events[eventsCount]);
                                            Serial.println();
                      
                          
                         } //endif
            }
            else {
                 PRINTS(".");
                 delay(100);
            }
    yield(); // inserted by suresh
  } //end of while

  PRINT("Heap end of function getCalendarEvents: ", system_get_free_heap_size());

 // scrollstrArr(events , eventsCount ); //This scrolls all the events in array i.e. events for next 24 hours fatched from google
 chkEvents(events); // This checks for the start and end time before scrolling event

} //end of function getCalendarEvents
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//work on events received
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void chkEvents(String *myevents){ // arguments are array of events; string to lookup in event, corresponding ESP pin and eventFlag
for(int i=0; i<(eventsCount+1); i++){
  
  myevents[i].trim();
  //Serial.println(events[i]);
  
  //Make time from event
  time_t startEvt;// Declare a time_t object for the start time (time_t)
  time_t endEvt;// Declare a time_t object for the end time (time_t)
  tmElements_t tmStartEvt; // Declare a tmElement_t object to manually input the start Time
  tmElements_t tmEndEvt; // Declare a tmElement_t object to manually input the start Time
  
  // Creates a tmElement by putting together all of Maggies Start Time and Dates
    tmStartEvt.Year = myevents[i].substring(2,4).toInt() + 30;  // years since 1970 in two digits
    tmStartEvt.Month = myevents[i].substring(5,7).toInt();
    tmStartEvt.Day = myevents[i].substring(8,10).toInt();
    tmStartEvt.Hour= myevents[i].substring(11,13).toInt();
    tmStartEvt.Minute = myevents[i].substring(14,16).toInt();
    tmStartEvt.Second = myevents[i].substring(17,19).toInt();

//Serial.print(myevents[i].substring(2,4).toInt());
//Serial.print(myevents[i].substring(5,7).toInt());
//Serial.print(myevents[i].substring(8,10).toInt());
//Serial.print(tmStartEvt.Hour);
//Serial.print(tmStartEvt.Minute);
//Serial.print(myevents[i].substring(17,19).toInt());
//Serial.println();

  // Creates a tmElement by putting together all of Maggies Start Time and Dates
    tmEndEvt.Year = myevents[i].substring(25,27).toInt() + 30;  // years since 1970 in two digits
    tmEndEvt.Month = myevents[i].substring(28,30).toInt();
    tmEndEvt.Day = myevents[i].substring(31,33).toInt();
    tmEndEvt.Hour= myevents[i].substring(34,36).toInt();
    tmEndEvt.Minute = myevents[i].substring(37,39).toInt();
    tmEndEvt.Second = myevents[i].substring(40,42).toInt();

// Takes the tmElement and converts it to a time_t variable. Which can now be compared to the current (now) unix time
    startEvt = makeTime(tmStartEvt);
    endEvt = makeTime(tmEndEvt);

//Serial.println(startEvt);
//Serial.println(endEvt);
//Serial.println( getEpochTime());

String appliance= myevents[i].substring(51,(myevents[i].length()-1));   //which appliance to be switched on/off
  
  
  if (startEvt < timeClient.getEpochTime() && endEvt > timeClient.getEpochTime()){
                                PRINT("\nTurning on " , appliance );
                scrollString(myevents[i].substring(51,(myevents[i].length()-1)));
                return;}   
   
    } //end of for

  
  } // end of function
  
  
/////////////////////////////////////////////////////////////////////////////////////////////////////////












/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions of MD_MAX72xx for LED Matrix display
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scrollText(char *p)
{
  uint8_t charWidth;
  uint8_t cBuf[8];  // this should be ok for all built-in fonts
     
  PRINTS("\nScrolling text");
  mx.clear();
  
  while (*p != '\0') 
  {
    charWidth = mx.getChar(*p++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);

    for (uint8_t i=0; i<charWidth + 1; i++) // allow space between characters
    {
      mx.transform(MD_MAX72XX::TSL);
      if (i < charWidth)
        mx.setColumn(0, cBuf[i]);
      delay(DELAYTIME);
    }
  }
       server.handleClient(); //handle web request. loop is long so we need to check web request several times.
}

void zeroPointSet()
// Demonstrates the use of setPoint and 
// show where the zero point is in the display
{
  PRINTS("\nZero point highlight");
  mx.clear();

  if (MAX_DEVICES > 1)
    mx.setChar((2*COL_SIZE)-1, '0');

  for (uint8_t i=0; i<ROW_SIZE; i++)
  {
    mx.setPoint(i, i, true);
    mx.setPoint(0, i, true);
    mx.setPoint(i, 0, true);
    delay(DELAYTIME);
  }

  delay(DELAYTIME*3);
}

void lines()
// Demonstrate the use of drawLine().
// fan out lines from each corner for up to 4 device blocks
{
  PRINTS("\nLines");
  const uint8_t stepSize = 3;
  const uint8_t maxDev = (MAX_DEVICES > 4 ? 4 : MAX_DEVICES);

  mx.clear();
  for (uint16_t c=0; c<(maxDev*COL_SIZE)-1; c+=stepSize)
  {
    mx.drawLine(0, 0, ROW_SIZE-1, c, true);
    delay(DELAYTIME);
  }

  mx.clear();
  for (uint16_t c=0; c<(maxDev*COL_SIZE)-1; c+=stepSize)
  {
    mx.drawLine(ROW_SIZE-1, 0, 0, c, true);
    delay(DELAYTIME);
  }

  mx.clear();
  for (uint16_t c=0; c<(maxDev*COL_SIZE)-1; c+=stepSize)
  {
    mx.drawLine(ROW_SIZE-1, (MAX_DEVICES*COL_SIZE)-1, 0, (MAX_DEVICES*COL_SIZE)-1-c, true);
    delay(DELAYTIME);
  }

  mx.clear();
  for (uint16_t c=0; c<(maxDev*COL_SIZE)-1; c+=stepSize)
  {
    mx.drawLine(0, (MAX_DEVICES*COL_SIZE)-1, ROW_SIZE-1, (MAX_DEVICES*COL_SIZE)-1-c, true);
    delay(DELAYTIME);
  }
}

void rows() 
// Demonstrates the use of setRow()
{
  PRINTS("\nRows 0->7");
  mx.clear();

  for (uint8_t row=0; row<ROW_SIZE; row++) 
  {
    mx.setRow(row, 0xff);
    delay(DELAYTIME);
    mx.setRow(row, 0x00);
  }
}

void columns() 
// Demonstrates the use of setColumn()
{
  PRINTS("\nCols 0->max");
  mx.clear();

  for (uint8_t col=0; col<mx.getColumnCount(); col++) 
  {
    mx.setColumn(col, 0xff);
    delay(DELAYTIME/MAX_DEVICES);
    mx.setColumn(col, 0x00);
  }
}

void cross() 
// Combination of setRow() and setColumn() with user controlled
// display updates to ensure concurrent changes.
{
  PRINTS("\nMoving cross");
  mx.clear();
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // diagonally down the display R to L
  for (uint8_t i=0; i<ROW_SIZE; i++) 
  {
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0xff);
      mx.setRow(j, i, 0xff);
    }
    mx.update();
    delay(3*DELAYTIME);
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0x00);
      mx.setRow(j, i, 0x00);
    }
  }
  
  // moving up the display on the R
  for (int8_t i=ROW_SIZE-1; i>=0; i--)
  {
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0xff);
      mx.setRow(j, ROW_SIZE-1, 0xff);
    }
    mx.update();
    delay(3*DELAYTIME);
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0x00);
      mx.setRow(j, ROW_SIZE-1, 0x00);
    }
  }
  
  // diagonally up the display L to R
  for (uint8_t i=0; i<ROW_SIZE; i++) 
  {
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0xff);
      mx.setRow(j, ROW_SIZE-1-i, 0xff);
    }
    mx.update();
    delay(3*DELAYTIME);
    for (uint8_t j=0; j<MAX_DEVICES; j++)
    {
      mx.setColumn(j, i, 0x00);
      mx.setRow(j, ROW_SIZE-1-i, 0x00);
    }
  }
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void bullseye()
// Demonstrate the use of buffer based repeated patterns
// across all devices.
{
  PRINTS("\nBullseye");
  mx.clear();
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  for (uint8_t n=0; n<3; n++)
  {
    byte  b = 0xff;
    int   i = 0;

    while (b != 0x00)
    {
      for (uint8_t j=0; j<MAX_DEVICES+1; j++)
      {
        mx.setRow(j, i, b);
        mx.setColumn(j, i, b);
        mx.setRow(j, ROW_SIZE-1-i, b);
        mx.setColumn(j, COL_SIZE-1-i, b);
      }
      mx.update();
      delay(3*DELAYTIME);
      for (uint8_t j=0; j<MAX_DEVICES+1; j++)
      {
        mx.setRow(j, i, 0);
        mx.setColumn(j, i, 0);
        mx.setRow(j, ROW_SIZE-1-i, 0);
        mx.setColumn(j, COL_SIZE-1-i, 0);
      }
        
      bitClear(b, i);
      bitClear(b, 7-i);
      i++;
    }
    
    while (b != 0xff)
    {
      for (uint8_t j=0; j<MAX_DEVICES+1; j++)
      {
        mx.setRow(j, i, b);
        mx.setColumn(j, i, b);
        mx.setRow(j, ROW_SIZE-1-i, b);
        mx.setColumn(j, COL_SIZE-1-i, b);
      }
      mx.update();
      delay(3*DELAYTIME);
      for (uint8_t j=0; j<MAX_DEVICES+1; j++)
      {
        mx.setRow(j, i, 0);
        mx.setColumn(j, i, 0);
        mx.setRow(j, ROW_SIZE-1-i, 0);
        mx.setColumn(j, COL_SIZE-1-i, 0);
      }
        
      i--;
      bitSet(b, i);
      bitSet(b, 7-i);
    }
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void stripe() 
// Demonstrates animation of a diagonal stripe moving across the display
// with points plotted outside the display region ignored.
{
  const uint16_t maxCol = MAX_DEVICES*ROW_SIZE;
  const uint8_t stripeWidth = 10;

  PRINTS("\nEach individually by row then col");
  mx.clear();

  for (uint16_t col=0; col<maxCol + ROW_SIZE + stripeWidth; col++) 
  {
    for (uint8_t row=0; row < ROW_SIZE; row++)
    {
      mx.setPoint(row, col-row, true);
      mx.setPoint(row, col-row - stripeWidth, false);
    }
    delay(DELAYTIME);
  }
}

void spiral() 
// setPoint() used to draw a spiral across the whole display
{
  PRINTS("\nSpiral in");
  int  rmin = 0, rmax = ROW_SIZE-1;
  int  cmin = 0, cmax = (COL_SIZE*MAX_DEVICES)-1;
 
  mx.clear();
  while ((rmax > rmin) && (cmax > cmin))
  {
    // do row
    for (int i=cmin; i<=cmax; i++)
    {
      mx.setPoint(rmin, i, true);
      delay(DELAYTIME/MAX_DEVICES);
    }
    rmin++;
  
    // do column
    for (uint8_t i=rmin; i<=rmax; i++)
    {
      mx.setPoint(i, cmax, true);
      delay(DELAYTIME/MAX_DEVICES);
    }
    cmax--;

    // do row
    for (int i=cmax; i>=cmin; i--)
    {
      mx.setPoint(rmax, i, true);
      delay(DELAYTIME/MAX_DEVICES);
    }
    rmax--;
    
    // do column
    for (uint8_t i=rmax; i>=rmin; i--)
    {
      mx.setPoint(i, cmin, true);
      delay(DELAYTIME/MAX_DEVICES);
    }
    cmin++;
  }
}

void bounce() 
// Animation of a bouncing ball
{
  const int minC = 0;
  const int maxC = mx.getColumnCount()-1;
  const int minR = 0;
  const int maxR = ROW_SIZE-1;

  int  nCounter = 0;
  
  int  r = 0, c = 2;
  int8_t dR = 1, dC = 1;  // delta row and column
  
  PRINTS("\nBouncing ball");
  mx.clear();

  while (nCounter++ < 200)
  {
    mx.setPoint(r, c, false);
    r += dR;
    c += dC;
    mx.setPoint(r, c, true);
    delay(DELAYTIME/2);
    
    if ((r == minR) || (r == maxR)) 
      dR = -dR;
    if ((c == minC) || (c == maxC)) 
      dC = -dC;
  }
}

void intensity() 
// Demonstrates the control of display intensity (brightness) across
// the full range.
{
  uint8_t row;

  PRINTS("\nVary intensity ");

  mx.clear();

  // Grow and get brighter
  row = 0;
  for (int8_t i=0; i<=MAX_INTENSITY; i++)
  {
    mx.control(MD_MAX72XX::INTENSITY, i);
    if (i%2 == 0)
      mx.setRow(row++, 0xff);
    delay(DELAYTIME*3);
  }

  mx.control(MD_MAX72XX::INTENSITY, 8);
}

void blinking() 
// Uses the test function of the MAX72xx to blink the display on and off.
{
  int  nDelay = 1000;

  PRINTS("\nBlinking");
  mx.clear();

  while (nDelay > 0)
  {
    mx.control(MD_MAX72XX::TEST, MD_MAX72XX::ON);
    delay(nDelay);
    mx.control(MD_MAX72XX::TEST, MD_MAX72XX::OFF);
    delay(nDelay);

    nDelay -= DELAYTIME;
  }
}

void scanLimit(void)
// Uses scan limit function to restrict the number of rows displayed.
{
  PRINTS("\nScan Limit");
  mx.clear();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t row=0; row<ROW_SIZE; row++) 
    mx.setRow(row, 0xff);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

  for (int8_t s=MAX_SCANLIMIT; s>=0; s--) 
  {
    mx.control(MD_MAX72XX::SCANLIMIT, s);
    delay(DELAYTIME*5);
  }
  mx.control(MD_MAX72XX::SCANLIMIT, MAX_SCANLIMIT);
}

void transformation1() 
// Demonstrates the use of transform() to move bitmaps on the display
// In this case a user defined bitmap is created and animated.
{
  uint8_t arrow[COL_SIZE] = 
  { 
    0b00001000, 
    0b00011100, 
    0b00111110, 
    0b01111111, 
    0b00011100, 
    0b00011100, 
    0b00111110, 
    0b00000000
  };     

  MD_MAX72XX::transformType_t  t[] = 
  {
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TFLR,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TRC,
    MD_MAX72XX::TSD, MD_MAX72XX::TSD, MD_MAX72XX::TSD, MD_MAX72XX::TSD,
    MD_MAX72XX::TSD, MD_MAX72XX::TSD, MD_MAX72XX::TSD, MD_MAX72XX::TSD,
    MD_MAX72XX::TFUD,
    MD_MAX72XX::TSU, MD_MAX72XX::TSU, MD_MAX72XX::TSU, MD_MAX72XX::TSU,
    MD_MAX72XX::TSU, MD_MAX72XX::TSU, MD_MAX72XX::TSU, MD_MAX72XX::TSU,
    MD_MAX72XX::TINV,
    MD_MAX72XX::TRC, MD_MAX72XX::TRC, MD_MAX72XX::TRC, MD_MAX72XX::TRC,
    MD_MAX72XX::TINV
  };
    
  PRINTS("\nTransformation1");
  mx.clear();

  // use the arrow bitmap
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t j=0; j<mx.getDeviceCount(); j++)
    mx.setBuffer(((j+1)*COL_SIZE)-1, COL_SIZE, arrow);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  delay(DELAYTIME);

  // run through the transformations
  mx.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::ON);
  for (uint8_t i=0; i<(sizeof(t)/sizeof(t[0])); i++)
  {
    mx.transform(t[i]);
    delay(DELAYTIME*4);
  }
  mx.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::OFF);
}

void transformation2() 
// Demonstrates the use of transform() to move bitmaps on the display
// In this case font characters are loaded into the display for animation.
{
  MD_MAX72XX::transformType_t  t[] = 
  {
    MD_MAX72XX::TINV,
    MD_MAX72XX::TRC, MD_MAX72XX::TRC, MD_MAX72XX::TRC, MD_MAX72XX::TRC,
    MD_MAX72XX::TINV,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, 
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TSD, MD_MAX72XX::TSU, MD_MAX72XX::TSD, MD_MAX72XX::TSU,
    MD_MAX72XX::TFLR, MD_MAX72XX::TFLR, MD_MAX72XX::TFUD, MD_MAX72XX::TFUD
  };
    
  PRINTS("\nTransformation2");
  mx.clear();
  mx.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::OFF);

  // draw something that will show changes
  for (uint8_t j=0; j<mx.getDeviceCount(); j++)
  {
    mx.setChar(((j+1)*COL_SIZE)-1, '0'+j);
  }
  delay(DELAYTIME*5);
  
  // run thru transformations
  for (uint8_t i=0; i<(sizeof(t)/sizeof(t[0])); i++)
  {
    mx.transform(t[i]);
    delay(DELAYTIME*3);
  }
}

void wrapText()
// Display text and animate scrolling using auto wraparound of the buffer
{
  PRINTS("\nwrapText");
  mx.clear();
  mx.wraparound(MD_MAX72XX::ON);

  // draw something that will show changes
  for (uint16_t j=0; j<mx.getDeviceCount(); j++)
  {
    mx.setChar(((j+1)*COL_SIZE)-1, (j&1 ? 'M' : 'W'));
  }
  delay(DELAYTIME*5);
  
  // run thru transformations
  for (uint16_t i=0; i<3*COL_SIZE*MAX_DEVICES; i++)
  {
    mx.transform(MD_MAX72XX::TSL);
    delay(DELAYTIME/2);
  }
  for (uint16_t i=0; i<3*COL_SIZE*MAX_DEVICES; i++)
  {
    mx.transform(MD_MAX72XX::TSR);
    delay(DELAYTIME/2);
  }
  for (uint8_t i=0; i<ROW_SIZE; i++)
  {
    mx.transform(MD_MAX72XX::TSU);
    delay(DELAYTIME*2);
  }
  for (uint8_t i=0; i<ROW_SIZE; i++)
  {
    mx.transform(MD_MAX72XX::TSD);
    delay(DELAYTIME*2);
  }

  mx.wraparound(MD_MAX72XX::OFF);
}

void showCharset(void)
// Run through display of the the entire font characters set
{
  mx.clear();
  mx.update(MD_MAX72XX::OFF);

  for (uint16_t i=0; i<256; i++)
  {
    mx.clear(0);
    mx.setChar(COL_SIZE-1, i);

    if (MAX_DEVICES >= 3)
    { 
      char hex[3];

      sprintf(hex, "%02X", i);

      mx.clear(1);
      mx.setChar((2*COL_SIZE)-1,hex[1]);
      mx.clear(2);
      mx.setChar((3*COL_SIZE)-1,hex[0]);
    }

    mx.update();
    delay(DELAYTIME*2);
  }
  mx.update(MD_MAX72XX::ON);
}
void scrollString(String str2scroll) {
PRINT("\nNow scrolling ", str2scroll);
int str2scroll_len = str2scroll.length() + 1;
char str2scroll_charArray[str2scroll_len];
str2scroll.toCharArray(str2scroll_charArray, str2scroll_len);
scrollText(str2scroll_charArray);
}

*/
/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void newGETnews(const char* host, String& url, char* target, byte& headLineCount  ){

String newsArray[maxHeadlines];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.
 PRINT("connecting to " ,host );
 
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    PRINTS("\nConnection failed");
    return;
  }
  
  
  
  PRINT("Requesting URL: " , url);
  
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      PRINTS("\n>>> Client Timeout !");
      client.stop();
      return;
    }
  yield(); //inserted by suresh   
  }

  headLineCount=0;
  // Read all the lines of the reply from server and print them to Serial
  while (client.available()){
    //String line = client.readStringUntil('\r');   // uncomment if the response has many lines
    //Serial.print(line);


//Suresh code to get TITLE Strings
char c = client.read();
Serial.println(c);
if(c == target[0]){ //first target character matches
  worded= 0;
  delay(1);
  Serial.print("strlen(target):");
  Serial.println(strlen(target));
 Serial.println("Start Match ITEM TITLE worded=" + worded);
  
    for(byte i=0; i<strlen(target); i++){
  Serial.print(c);
  Serial.print(c + 0); //this prints ASCII value of character
 Serial.println("\nNow print i,target[i],worded");
 Serial.println(i);
 Serial.println(target[i]);
 Serial.println(worded);
  
                  if(c == target[i]){ 
                    worded=++worded; }
                    else {
                      Serial.println("breaking from match");
                      break;}

                    
                  if(i==(strlen(target)-1) && worded==strlen(target)){
                    ++headLineCount;
                      newsArray[headLineCount] = client.readStringUntil('<');
                      Serial.println();
                      Serial.println("stored in " + headLineCount);
                       PRINT("\nstored in ",headLineCount);
                      Serial.println(newsArray[headLineCount]);
                      Serial.println();
                      
                   }    //end of news if
                  
                  
     c = client.read();


     
     }             //end of for loop              
                  
                  
    
//String line = client.readStringUntil('\r');
 // Serial.println(line);

}  //end of '<' if
//Suresh Code ends here

     //c = client.read();

if (headLineCount==(maxHeadlines - 1)) { break;} // break out from while loop if we already have  extracted headlines defined by maxHeadlines parameter.
     
  } // end of while
  
  Serial.println();
  PRINTS("\nClosing connection");

scrollstrArr(newsArray, headLineCount );

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void httpsGETnews(const char* host, String& url, char* target, byte& headLineCount  ){

String newsArray[1];         //array to store news headlines. Match the number with int maxHeadlines. Default value is 20.

// Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;

 PRINT("\nConnecting to " ,host );
 
 PRINT("Heap in getCalendarEvents start: ", system_get_free_heap_size());
  
  // Use secureWiFiClient class to create connections
    if (!client.connect(host, httpsPort)) {
    PRINTS("\nConnection failed");
    return;
  }
  
  PRINTS("\nConnection made");
  
    if (client.verify(fingerprint, host)) {        //match certificates. Note- processing will continue evenif certificates do match. As of now no action is taken when certificates do not match.
    PRINTS("\nCertificate matches");
  } else {
    PRINTS("\nCertificate doesn't match");
  }
  
  
  
  PRINT("\nRequesting URL: " , url);
  
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "User-Agent: myESP8266\r\n" +         
               "Connection: close\r\n\r\n");

  PRINTS("\nRequest sent");        
         
  headLineCount=0;         
//unsigned long timeout = millis();
unsigned long startTime = millis();
unsigned int httpResponseTimeOut = 5000; // 5 sec

startTime = millis();

  while (client.connected() && ((millis() - startTime) < httpResponseTimeOut)) {
         if (client.available()) {
            String line = client.readStringUntil('\n');
    
            Serial.println("events received:" + line);

 
//Suresh code to get TITLE Strings
char c = client.read();
Serial.println(c);
if(c == '<'){
  worded= 0;
  delay(1);
 Serial.println("Start Match ITEM TITLE worded=" + worded);
  
    for(byte i=0; i<strlen(target); i++){
  Serial.print(c);
  Serial.print(c + 0);
 Serial.println();
 Serial.println(target[i]);
 Serial.println(worded);
  
                  if(c == target[i]){ 
                    worded=++worded; }
                    else {
                      Serial.println("breaking");
                      break;}

                    
                  if(i==(strlen(target)-1) && worded==strlen(target)){
                    ++headLineCount;
                      newsArray[headLineCount] = client.readStringUntil('<');
                      Serial.println();
                      Serial.println("stored in " + headLineCount);
                       PRINT("\nstored in ",headLineCount);
                      Serial.println(newsArray[headLineCount]);
                      Serial.println();
                      
                   }    //end of news if
                  
                  
     c = client.read();


     
     }             //end of for loop              
                  
                  
    
//String line = client.readStringUntil('\r');
 // Serial.println(line);

}  //end of '<' if
//Suresh Code ends here

     //c = client.read();
}
   
if (headLineCount==(maxHeadlines - 1)) { break;} // break out from while loop if we already have  extracted headlines defined by maxHeadlines parameter.
     
  } // end of while
  
  Serial.println();
  PRINTS("\nClosing connection");

scrollstrArr(newsArray, headLineCount );

}






///////////////////////////////////////////////
//WiFi Manage Function
//////////////////////////////////////////////
void manageWiFi(){
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  PRINTS("\nmounting FS...");
  
  if (SPIFFS.begin()) {
    PRINTS("\nmounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      PRINTS("\nreading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        PRINTS("\nopened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          PRINTS("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

          if(json["ip"]) {
            PRINTS("\nsetting custom ip from config");
            //static_ip = json["ip"];
            strcpy(static_ip, json["ip"]);
            strcpy(static_gw, json["gateway"]);
            strcpy(static_sn, json["subnet"]);
            //strcat(static_ip, json["ip"]);
            //static_gw = json["gateway"];
            //static_sn = json["subnet"];
            Serial.println(static_ip);
/*            Serial.println("converting ip");
            IPAddress ip = ipFromCharArray(static_ip);
            Serial.println(ip);*/
          } else {
            PRINTS("\nno custom ip in config");
          }
        } else {
          PRINTS("\nfailed to load json config");
        }
      }
    }
  } else {
    PRINTS("\nfailed to mount FS");
  }
  //end read

  
  Serial.println(static_ip);
  Serial.println(blynk_token);
  Serial.println(mqtt_server);


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("MagicScrollerAP", "password")) {
    PRINTS("\nfailed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset(); //commented so that we move to main loop.
    //delay(5000);
  }

  //if you get here you have connected to the WiFi
  PRINTS("\nconnected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    json["ip"] = WiFi.localIP().toString();
    json["gateway"] = WiFi.gatewayIP().toString();
    json["subnet"] = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      PRINTS("\nfailed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
 }

//Function to get random line from file in spiff
String randomLineFromSpiffFile(String filename, int totalLines, int avgCharParLine) {
    
    String myLine;
    int randnum = random(1, totalLines);

    Serial.println(randnum);

    File  f = SPIFFS.open(filename, "r");   //open file for reading filename="/quotes.txt"
  
    if (!f) {
      Serial.println("file open failed");
    }  Serial.println("====== Reading from SPIFFS file =======");
  
  // read strings from file

    f.seek(randnum * avgCharParLine,SeekSet); //if mode is SeekSet, position is set to offset bytes from the beginning.if mode is SeekCur, current position is moved by offset bytes.if mode is SeekEnd, position is set to offset bytes from the end of the file.
    
    myLine = f.readStringUntil('\r');  //read from current position set by SeekSet, upto next line feed
    myLine = f.readStringUntil('\r');   // read next full line
    f.close(); //Close the file. No other operations should be performed on File object after close function was called.
    Serial.println(myLine);
  
    return myLine;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Set Zones
///////////////
//Set Zones on receipt of user input in handle root
 void setZones() {
  if(userInput.indexOf("2Zone") != -1){
    P.setZone(0, 0, 5);
    P.setZone(1, 6, 7);
    P.setFont(1, dig3x5);}   
  else {
    P.setZone(0, 0, MAX_DEVICES - 1);}
  }

/////////////////////////////////////////////////
/*//Scroll string 
void scrollString(String str2scroll) {
   if (P.displayAnimate())  {  // animates and returns true when an animation is completed
   

       if (P.getZoneStatus(0))
      {
        P.setFont(0);
        P.displayZoneText(0, const_cast<char*>(str2scroll.c_str()), PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        P.displayReset(0);
        Serial.println();
        Serial.println(str2scroll);
      }
    }
  while (P.getZoneStatus(0) == false) {zoneOneTimePrint(); P.displayAnimate(); yield();} //Do nothing and wait for scroll to end
  server.handleClient();
  yield();
}
*/
/////////////////////////////////////////////////////////////////////////////////////////
void zoneOneTimePrint() {
// If 02 Zones selected in UI then zone-1 is for clock 
if ((userInput.indexOf("2Zone") >= 0) && P.getZoneStatus(1) && (digitalRead(pirPin) == HIGH)){
      myTime=timeClient.getFormattedTime();
      myTime.remove(5,3); // time in hh:mm format
      myTime.remove(2,1); // time in hhmm format. Limit digits to 2 8x8 modules
      
      P.setFont(1, dig3x5);
      P.displayZoneText(1, const_cast<char*>(myTime.c_str()), PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
      P.displayReset(1);     
}  
}

//////////////////////////////////////////////////////////////////////////////////////
//RTC Functions 
//This sets myTime string with formated time string

#define countof(a) (sizeof(a) / sizeof(a[0]))
void printTime(const RtcDateTime& dt)
{
    char timestring[8];

    snprintf_P(timestring, 
            countof(timestring),
            PSTR("%02u:%02u:%02u"),
           // dt.Month(),
            //dt.Day(),
            //dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.println(timestring);
    myTime=timestring;
}

/////////////////////////////////////////////////////////////////////////////////////////
//Serial Feedback
void useButonCheck()
{
  if(button1.held()){
    Serial.println("button1 was held.");
    Serial.println("ESP8266 in shutDisplay mode toggle");
    shutDisplay = !shutDisplay;
    P.displayShutdown(shutDisplay);
    }
  
  if(button1.heldLong()){
    Serial.println("button1 held for a long time!");
    Serial.println("Reset ESP Now !!!");
    ESP.reset(); //reset ESP
    delay(5000);
    }
  
  if(button1.clicked()){
    Serial.println("button1 Just clicked.");
    displayItem ++;
    if (displayItem > 6){displayItem =1;}

    switch (displayItem) {
    case 1:
    userInput="timeDisplay##Welcome to home of creators####################WelcomeMsg=Welcome to home of creators";
    scrollString("Time Display Mode    ");
     setZones();
    breakLoop=true;
    break;
    case 2:
    userInput="2Zone##Welcome to home of creators####################WelcomeMsg=Welcome to home of creators";
    scrollString("2 Zone Mode: Time and Date    ");
     setZones();
    breakLoop=true;
    break;
    case 3:
    userInput="2Zone##bbc##zee##Welcome to home of creators##################WelcomeMsg=Welcome to home of creators";
    scrollString("BBC and Zee News    ");
    setZones();
    breakLoop=true;
    break;
    case 4:
    userInput="2Zone##qod##Welcome to home of creators##################WelcomeMsg=Welcome to home of creators";
    scrollString("Scroll QoD from internet    ");
    setZones();
    breakLoop=true;
    break;
    case 5:
    userInput="2Zone##bbc##Welcome to home of creators##################WelcomeMsg=Welcome to home of creators";
    scrollString("BBC News    ");
    setZones();
    breakLoop=true;
    break;
    case 6:
    userInput="2Zone##spiff_quote##spiff_facts##spiff_taglines##Welcome to home of creators##############WelcomeMsg=Welcome to home of creators";
    scrollString("Scroll from spiff: QoD, Facts, Taglines    ");
     setZones();
    breakLoop=true;
    break;
  } //switch end
    } //if button clicked end
  
  if(button1.doubleClicked()){Serial.println("button1 Just double clicked!");}

  //These are commented out as they fire often and make the Serial output a bit messy to look at
  
  //if(button1.pressed()){Serial.println("button1 Just pressed!");}
  //if(button1.released()){Serial.println("button1 Just released!");}
}


/////////////////////////////////////////////////////////////////////////////////////////
//TO DO
/////////////////////////////////////////////////////////////////////////////////////////

/*
Quote of the day Title (Auther) - Description
Salutation morning, evening
 */
 
