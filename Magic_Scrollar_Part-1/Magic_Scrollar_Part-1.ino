/*
Magic Scroller by Vatsal and Suresh Hariramani
Based on Program to exercise the MD_MAX72XX library, Uses most of the functions in the library

Part 1: Clock - Scroll Date and Time (NTP Client)


TO DO
Part 2: Scroll custom message
Part 3: Scroll RSS feeds (News, Quote of the Day, Cricket Live from cricinfo)
Part 4: Lets Schedule what you want to scroll, interface with google calendar
Part 5: Add a webpage to choose whatever you want to scroll
*/


#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>


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
const char ssid[] = "xxxxxx";  //  your network SSID (name)
const char pass[] = "xxxxxxxx";       // your network password

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//NTP CODE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <TimeLib.h> 

// NTP Servers:
static const char ntpServerName[] = "in.pool.ntp.org";
//IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov


const float timeZone = 5.5;     // Indian Standard Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
static boolean isLongFormat = false;

String myTime;
String myDate;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Pin assignment and LED Matrix related stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may 
// need to be adapted
#define MAX_DEVICES 4

#define  CLK_PIN  D5  // or SCK --GPIO14
#define DATA_PIN  D7  // or MOSI --GPIO13
#define CS_PIN    D8  // or SS -- GPIO15


// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(CS_PIN, MAX_DEVICES);
// Arbitrary pins
// MD_MAX72XX mx = MD_MAX72XX(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// We always wait a bit between updates of the display 
#define  DELAYTIME  100  // in milliseconds. Lower the value, higher the scroll speed.


void setup()
{
  mx.begin();

#if  DEBUG
  Serial.begin(115200);
  delay(1000);
#endif
  PRINTS("\n[MD_MAX72XX Magic Scrollar]");

delay(250);
  PRINTS("\nTimeNTP Example");
  PRINT("\nConnecting to ", ssid);
  
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    PRINTS(".");
  }
  PRINT("\nIP number assigned by DHCP is ", WiFi.localIP());
  PRINTS("\nStarting UDP");
  Udp.begin(localPort);
  PRINT("\nLocal port: ", Udp.localPort());
  PRINTS("\nwaiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(120);  // NTP re-sync interval in seconds
  

}

//time_t prevDisplay = 0; // when the digital clock was displayed

void loop() 
{
       
/*
//NTP CODE
if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time minute value has changed (if you want to run function digitalClockDispaly in each loop iteration then write now() instead of minute())
      prevDisplay = now();
    }
  }

*/

delay(200);

ledMatrixScroll(); 

} // loop ends here

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//LED Scroll functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ledMatrixScroll(){      //set here messages to scroll

timeDisplay(); 
dateDisplay();

mx.clear();  
PRINTS("\nScroll function end");
} // ledMatrixScroll ends here


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Scroll Time and Date Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void timeDisplay() {
// Populate myDate String
myTime= hour();

if(minute() < 10){
myTime= myTime + ":0" + minute();
}
else {
myTime= myTime + ":" + minute();
}


if(second() < 10){
myTime= myTime + ":0" + second();
}
else {
myTime= myTime + ":" + second();
}


myTime= (" " + myTime +" ");
PRINT("\nMyTime: ", myTime);

int myTime_len = myTime.length() + 1;
char myTime_charArray[myTime_len];
myTime.toCharArray(myTime_charArray, myTime_len);

PRINT("\nMyTime_charArray: ",myTime_charArray );

scrollText(myTime_charArray);
}







void dateDisplay() {
// Populate myDate String
myDate= dayShortStr(weekday())  ;
myDate= myDate + "  " + day() + " " + monthShortStr(month()) + " " + year() ;
myDate= (" " + myDate + " ");
PRINT("\nMyDate: " , myDate);


// covert from string/float to char array
int myDate_len = myDate.length() + 1;
char myDate_charArray[myDate_len];
myDate.toCharArray(myDate_charArray, myDate_len);

PRINT("\nMyDate_charArray: ", myDate_charArray);

scrollText(myDate_charArray);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*-------- NTP code ----------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


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
int str2scroll_len = str2scroll.length() + 1;
char str2scroll_charArray[str2scroll_len];
str2scroll.toCharArray(str2scroll_charArray, str2scroll_len);
scrollText(str2scroll_charArray);
}



 
