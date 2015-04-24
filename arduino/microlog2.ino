// ---[ INFO ]----------------------------------------

// Program: MyPower.CZ Microlog2
// License: free for non-commercial use
// Details: info@mypower.cz

// ---[ VERSION ]-------------------------------------

#define MPWMicroLogVersion     "2.04"

// ---[ INCLUDE ]-------------------------------------

#include <SPI.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <utility/w5100.h>

// ---[ BUILT-IN CONFIG ]-----------------------------

int sendInterval  = 30; // sec default: 120
int sendRetry     = 30; // sec default: 30
byte maxErrors    = 3;  // default: 3

byte NETmac[6]    = { 0xF6, 0x88, 0x00, 0x00, 0x00, 0x01 };
//byte NETip[4]     = { 255, 255, 255, 255 }; // = DHCP
/*
byte NETip[4]     = { 10, 42, 0, 10 };  // { 255, 255, 255, 255 }; // = DHCP
byte NETgw[4]     = { 10, 42, 0, 1 };
byte NETmask[4]   = { 255, 255, 255, 0 };
byte NETdns[4]    = { 10, 42, 0, 1 };
*/
byte NETip[4]     = { 192, 168, 0, 10 };  // { 255, 255, 255, 255 }; // = DHCP
byte NETgw[4]     = { 192, 168, 0, 1 };
byte NETmask[4]   = { 255, 255, 255, 0 };
byte NETdns[4]    = { 192, 168, 0, 1 };

// ---[ DPIN 22..49 map ]--------------------------
//                    00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  15
//                    [....... LED .....][.not used.][........... RELE .............]
byte DPINmap[16]  = { 40, 42, 44, 46, 48, 0,  0,  0,  22, 23, 24, 25, 26, 27, 28, 29 };
uint16_t DPINstatus    = 0x0000;
//                         <15  08>          <07  >00                          
uint16_t DPINlogicmask = (B00000000 << 8) | B00000011;  // pin/bit  0 = standard logic 0 OFF 1 ON,   1 = negative logic 0 ON 1 OFF

// ---[ MODS ]----------------------------------------

// mod MyPowerLog
#define modMyPowerLog      1                    // 1=on 0=off
#define modMyPowerHost     "log.camel.cz"     // log.mypower.cz
#define modMyPowerURL      "/log/write"         // /
#define modMyPowerFVEID    "camel_venku"        // mojefve1234

// mod StausLed
#define modStatusLed       1         // 1=on 0=off
#define modStatusLedPin    41        // default: 40

// mod SerialLog
#define modSerialLog       1         // 1=on 0=off
#define modSerialForce     1         
#define modSerialBaudRate  115200    // 9600  ....  115200

// mod WebServer
#define modWebServer       1         // 1=on 0=off
#define modWebServerPort   80        // default: 80

// mod Display
#define modDisplay         1         // 1=on 0=off
#define modDisplayType     1602      // typ displaye

// mod DisplayEmulation
#define modDisplayEmulation     0    // 1=display neni fyzicky pripojen - zobrazeni jen na www rozhrani 
                                     // 0=display pripojen - zobrazeni na fyzickem displayi i na www rozhrani
// mod ExternalBus
#define modExtBus          1    // 1=enabled
                                // 0=disabled

#if modExtBus == 1
 #include <OneWire.h>
 #include <DallasTemperature.h>
 #include <dht11.h>
 #include "modExtBus.h"

// setup
 // null terminated array of protocols used
 byte modExtBusPINproto[] = {modExtBus1Wire, modExtBusDHT11, 0};
 // pins for corresponding protocols above
 byte modExtBusPINmap[] = {3,2};
#endif

// ---------------------------------------------------

byte lxLed=0;
byte lxTime=0;
uint32_t lxSeconds=0;
uint32_t lxSendIndex=0;
byte lxHttpOk=0;
byte lxHttpErrors=0;

// ---------------------------------------------------

#define dLCDKeyPad1602   1602

// ---------------------------------------------------

//  TTT GG E   
// 0000 0000

#define xpinOPTdefault   B00000000
#define xpinOPTenabled   B00000001

#define xpinOPTtypeMask  B01110000

#define xpinOPTsample    B00000000
#define xpinOPTavg       B00010000
#define xpinOPTeffective B00100000
#define xpinOPTmin       B00110000
#define xpinOPTmax       B01000000
#define xpinOPTres1      B01010000
#define xpinOPTres2      B01100000
#define xpinOPTdefault   B01110000

#define xpinOPTgroupMask B00001100

#define xpinOPTsampleG0  B00000000
#define xpinOPTsampleG1  B00000100
#define xpinOPTsampleG2  B00001000
#define xpinOPTsampleG3  B00001100

struct tPinConf {
char xname[21];char xunit[6];byte xopt;
signed long xmin,xmax;int xmin10000,xmax10000;
byte chsum;
};

struct tDPinConf {
char xname[20];
};

#define xComStatusNop               B00000000
#define xComStatusASchanged         B00000001
#define xComStatusDSchanged         B00000010
#define xComStatusIsLastDayInMonth  B00000100
#define xComStatusIsDST             B00001000

#define xComStatusXXDaysMonthMask   B00110000
#define xComStatus28DaysMonth       B00000000
#define xComStatus29DaysMonth       B00010000
#define xComStatus30DaysMonth       B00100000
#define xComStatus31DaysMonth       B00110000

struct tMpwAccess { 
  byte xComStatus;                    //  flags
  uint16_t mpwremoteDateYear;         //  2013 ...
   uint8_t mpwremoteDateMonth;        //  1-12
   uint8_t mpwremoteDateDay;          //  1-31
   uint8_t mpwremoteDateWeek;         //  1-53
   uint8_t mpwremoteDateDayOfWeek;    //  1 (for Monday) through 7 (for Sunday)
  signed long mpwremoteTimeS;         //  0-86400, -1 - not known
  uint32_t mpwremoteCheckedAtS;       //  local timeS (seconds counter)
  uint32_t mpwlogintimeS;             //  login local timeS 
  uint32_t mpwDeviceId;               //  your device ID on mpw server
  uint32_t mpwAccessId;               //  your access ID on mpw server
} xMpwAccess;

struct tMpwDateTime { 
  int xyear, xmonth, xday, xhour, xminute, xsecond; 
  };

#define xdcOptBuiltIn      0x00;
#define xdcOptActive       0x01;

struct tSampledPin { int pin; byte smpHi8[128]; byte smpLo2[32]; unsigned long smpSqr,smpAvg; int smpMin, smpMax, smpCount; };
struct tPinValue { int smpVal,smpSqr,smpAvg,smpMin,smpMax; unsigned long smpTime; };
struct tSamplerStruct { tPinValue tPinValues[NUM_ANALOG_INPUTS]; tSampledPin tPinSamples[4]; 
  byte tPinTimes[128]; unsigned long tPinMicros; } xSamplerStruct;

struct tDeviceConf {
char xfveid[21];
int xsendInterval;
int xsendRetry;
byte xmaxErrors;
byte xdcOpt;
byte chsum;
};

struct int64 { word hh,hl,lh,ll; boolean msf; };

// ---------------------------------------------------

#if modWebServer == 1
EthernetServer server(modWebServerPort);
#endif

#if (modDisplay == 1)
byte lcdCurrentPage=0xFF;
byte lcdMaxPage=0;

#if (modDisplayType == dLCDKeyPad1602)
struct tDispStats { int xvarscount; int xvarram; };
char _lcdlbuf[17];
char * _lcdweblbuf0=NULL;
char * _lcdweblbuf1=NULL;
tDispStats * _lcdwebdispstats=NULL;

#if (modDisplayEmulation == 0)
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 14, 5, 6, 7);
void lcdClear() { lcd.clear(); }
void lcdInit() { lcd.begin(16, 2); lcdClear(); }
#else
void lcdClear() { }
void lcdInit() { lcdClear(); }
#endif

void lcdPrintRight(char * str) { int x=16-strlen(str); lcdPrintAt(str,(x<0?0:x)); }
void lcdPrintCenter(char * str) { int x=8-(strlen(str)/2); lcdPrintAt(str,(x<0?0:x)); }
void lcdPrintLeft(char * str) { lcdPrintAt(str,0); }
void lcdPrintAt(char * str,int x) { 
  int ix=strlen(str);for (int i=0;((i+x<sizeof(_lcdlbuf)-1) && (i<ix));i++) if (x>=0) _lcdlbuf[i+x]=str[i]; }
//  int ix=strlen(str);for (int i=0;((i+x<sizeof(_lcdlbuf)-1) && (i<ix));i++) if ((x+i)>=0) _lcdlbuf[i+x]=str[i]; } ??????
void lcdClearLine() { int i; for (i=0;i<sizeof(_lcdlbuf)-1;i++) _lcdlbuf[i]=32; _lcdlbuf[i]=0; }
void lcdPrintLine(int y) { 
  if ((_lcdweblbuf0!=NULL) && (_lcdweblbuf1!=NULL))
    { char * p=_lcdweblbuf0; if (y==1) p=_lcdweblbuf1;strcpy(p,_lcdlbuf); }
  else { 
#if (modDisplayEmulation == 0) 
  lcd.setCursor(0,y); lcd.print(_lcdlbuf); 
#endif
} }

#endif

#endif


// -- [SETUP] ----------------------------------------------

void setup()
{
#if modExtBus == 1
modExtBusSetup();
#endif


#if modStatusLed == 1
pinMode(modStatusLedPin, OUTPUT); SetLiveLed(HIGH);
#endif


#if ((modSerialLog == 1) || (modSerialForce == 1))
Serial.begin(modSerialBaudRate);
#endif


#if ((modSerialLog == 1) || (modDisplay == 1))
char sn1[]="MyPower.CZ";
char sn2[]="MicroLOG";

#if (modSerialLog == 1)
XLog(sn1);XLog(sn2);XLog(MPWMicroLogVersion);

#if (modMyPowerLog == 1)
xMpwAccess.mpwremoteDateYear=0;
xMpwAccess.mpwremoteDateMonth=0;
xMpwAccess.mpwremoteDateDay=0;
xMpwAccess.mpwremoteDateWeek=0;
xMpwAccess.mpwremoteDateDayOfWeek=0;
xMpwAccess.mpwremoteTimeS=-1;
xMpwAccess.mpwremoteCheckedAtS=0;
xMpwAccess.mpwlogintimeS=0;
xMpwAccess.mpwDeviceId=0;
xMpwAccess.mpwAccessId=0;
xMpwAccess.xComStatus=xComStatusNop;
char cx[50]="+mpwlog @ ";strcat(cx,modMyPowerHost);XLog(cx);
#endif

#if (modDisplay == 1)
char ccx[50]="";
#if (modDisplayEmulation == 1 )
sprintf(ccx,"+dispemu @ %04d",modDisplayType);
#else
sprintf(ccx,"+disp @ %04d",modDisplayType);
#endif
XLog(ccx);
#endif

#endif

#if (modDisplay == 1)
lcdInit();lcdCurrentPage=0xFF;
lcdClearLine();lcdPrintCenter(sn1);lcdPrintLine(0);
lcdClearLine();lcdPrintLeft(sn2);lcdPrintRight(MPWMicroLogVersion);lcdPrintLine(1);
#endif

#endif

DPINResetHW();
pinMode(53, OUTPUT);digitalWrite(53,HIGH);

int xret=0;

boolean isdhcp=((NETip[0]&NETip[1]&NETip[2]&NETip[3])==255);

if (isdhcp)
  {
#if (modSerialLog == 1)
XLog("DHCP?");
#endif
xret=Ethernet.begin(NETmac);
  }
else
  {
  Ethernet.begin(NETmac, NETip, NETdns, NETgw, NETmask);
  xret=1;
  delay(1000);
  }

W5100.setRetransmissionTime(0x07FF);
W5100.setRetransmissionCount(2); 

if ( (xret == 0) && (isdhcp) ) {
#if modSerialLog == 1
  XLog("No dhcp.");
#endif
  FlashError(4);
  }
else
  {
#if modSerialLog == 1
  char xtext[100];
  char cf[]="%d.%d.%d.%d";
  IPAddress xaddr;
  XLog("IP,MASK,GW,DNS");
  xaddr=Ethernet.localIP();
  for (byte i=0;i<4;i++) {
    if (i==1) xaddr=Ethernet.subnetMask();else
    if (i==2) xaddr=Ethernet.gatewayIP();else
    if (i==3) xaddr=Ethernet.dnsServerIP();   
    sprintf(xtext,cf,xaddr[0],xaddr[1],xaddr[2],xaddr[3]);
    XLog(xtext);
    }
#endif
  }

#if modWebServer == 1
server.begin();
#if modSerialLog == 1
char c[30]="";sprintf(c,"+www @ %d",modWebServerPort);XLog(c);
#endif
#endif

SamplerReset();

#if modSerialLog == 1
XLog("Done.");
#endif
}



// -- [LOOP] ----------------------------------------------

void loop()
{
FlashLiveLed();
CalculateSeconds();
#if modMyPowerLog == 1
SendDataToMyPower();
#endif
#if modWebServer == 1
WebServer();
#endif
#if (modDisplay == 1)
#if (modDisplayType == dLCDKeyPad1602)
DisplayDetectButton();
#endif
#endif
SamplerRun();
#if modExtBus == 1
 modExtBusRead();
#endif
}

void softReset() { asm volatile ("  jmp 0"); } 

#if modSerialLog == 1
void XLog(char * xmsg) { Serial.println(xmsg); }
void XLogRam() { Serial.print("> RAM:"); Serial.println(freeRam()); }
void XLog2(char * xmsg,char * xpre) { Serial.print(xpre);Serial.println(xmsg); }
#endif

#if (modDisplay == 1)
void CalcDisplayConfEpromSpace(int * xa,int * la) {
* xa=(sizeof(tPinConf)*NUM_ANALOG_INPUTS)+sizeof(tDeviceConf);
* la=2047; }
#endif


void SetLiveLed(byte xs) { 
#if modStatusLed==1
digitalWrite(modStatusLedPin,xs);
#endif
}

void FlashLiveLed() {
unsigned long i=((millis()/100)%10);
byte qlxLed=( ((i==1)) || ((lxHttpOk==2) && ( (i==3) || (i==5) )) )?HIGH:LOW;
if (qlxLed!=lxLed) { SetLiveLed(qlxLed); lxLed=qlxLed; }
}

void CalculateSeconds() {
byte smx=0x7F; int qlxTime=((millis()/1000)&smx);
if (qlxTime!=lxTime) { OneSecondActions();
  lxSeconds+=(lxTime>qlxTime)?(qlxTime+smx+1-lxTime):(qlxTime-lxTime); lxTime=qlxTime; }
}

void OneSecondActions() {
#if (modDisplay == 1)
DrawDisplayPage();
#endif
}

#if modMyPowerLog==1
void SendDataToMyPower()
{ 
boolean canrun=true; 
for (int xp=0;xp<NUM_ANALOG_INPUTS;xp++)
  if (xSamplerStruct.tPinValues[xp].smpTime==0)
    { canrun=false;break; }
if (canrun) SendDataToMyPower_int(); 
}

void SendDataToMyPower_int()
{  
char xtext[200]; 
int xsi=sendInterval;
int xsiorig=xsi;
int xsr=sendRetry;
if (lxHttpOk==2) xsi=xsr;
if (xsi<30) xsi=30;
uint32_t qlxSendIndex=lxSeconds;
if ((lxSendIndex+xsi<=qlxSendIndex) || (lxHttpOk==0))
  {
  lxHttpOk=2;
  lxSendIndex=lxSeconds;
  EthernetClient client;  
  SetLiveLed(HIGH);
#if modSerialLog == 1
  XLogRam();XLog("> Connecting");
#endif
  if ( (client.connect(modMyPowerHost, 80)) && (client.connected()) )
    {
#if modSerialLog == 1
    XLog("> Connected");
#endif
    sprintf(xtext,"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: MPWmicrolog/%s\r\nCookie: fveid=%s; ",
      modMyPowerURL, modMyPowerHost,MPWMicroLogVersion,modMyPowerFVEID);
    client.write((byte*)xtext,strlen(xtext));  

    boolean islogin=(xMpwAccess.mpwlogintimeS==0);

if (islogin)
  {
  strcpy(xtext,"cdate=" __DATE__ " " __TIME__ ";");
  for (int i=0;i<strlen(xtext);i++)
    if (xtext[i]==' ') xtext[i]='_';
  strcat(xtext," ");
  client.write((byte*)xtext,strlen(xtext));
  client.write("login=yes; ");
  }
 
      sprintf(xtext,"utime=%lu/%lu; ",millis(),lxSeconds);
      client.write((byte*)xtext,strlen(xtext));
      sprintf(xtext,"AX=VSAMX; ");
      client.write((byte*)xtext,strlen(xtext));

    for (int xp=0;xp<NUM_ANALOG_INPUTS;xp++)
      {

      tPinValue * qPinValue;
      qPinValue=&xSamplerStruct.tPinValues[xp];
      sprintf(xtext,"A%d=%X,%X,%X,%X,%X; ",xp,
        qPinValue->smpVal,qPinValue->smpSqr,qPinValue->smpAvg,qPinValue->smpMin,qPinValue->smpMax);
      client.write((byte*)xtext,strlen(xtext));

if ( (islogin) || ((xMpwAccess.xComStatus&xComStatusASchanged)!=0) )
  {
      tPinConf pconf;            
      boolean isvalid=ReadPinConf(&pconf,xp);            
      if (isvalid)
        { 
        char smin10k[30]="";sprint10k(smin10k,pconf.xmin,pconf.xmin10000);
        char smax10k[30]="";sprint10k(smax10k,pconf.xmax,pconf.xmax10000);
//        sprintf(xtext,"AC%d=%X/%ld.%04d/%ld.%04d/",xp,pconf.xopt,pconf.xmin,pconf.xmin10000,pconf.xmax,pconf.xmax10000);
        sprintf(xtext,"AC%d=%X/%s/%s/",xp,pconf.xopt,smin10k,smax10k);
        client.write((byte*)xtext,strlen(xtext));
        xtext[0]=0;
        char xf[]="%02X";
        char xh[3];
        byte * p;
        p=(byte*)pconf.xname;
        for (int i=0;i<strlen(pconf.xname);i++) { sprintf(xh,xf,p[i]); strcat(xtext,xh); }
        client.write((byte*)xtext,strlen(xtext));
        strcpy(xtext,"/");
        p=(byte*)pconf.xunit;
        for (int i=0;i<strlen(pconf.xunit);i++) { sprintf(xh,xf,p[i]); strcat(xtext,xh); }
        client.write((byte*)xtext,strlen(xtext));
        client.write("; ");
        }              
  }
      }
  
#if modExtBus == 1
  for (int i=0;i<xExtBusValCount;i++)
    {
     // if(! xExtBusValue[i].changed) continue;
      xExtBusValue[i].changed = xtext[0] = 0;
      sprintf(xtext,"EXTBUS_%d_%d=", xExtBus[xExtBusValue[i].bus].pin, xExtBusValue[i].valOffset);
      ftoa(xtext + strlen(xtext), xExtBusValue[i].cur, 2);      
      client.write((byte*)xtext,strlen(xtext));
      client.write("; ");
    }
#endif
  
    xMpwAccess.xComStatus&=~xComStatusASchanged;
    
    client.write("\r\n\r\n");
      
    int lbufpos=0;
    boolean ishead=true;
    xtext[0]=0;
    unsigned long xmstart=millis();
    while (client.connected())
      {
      if (millis()-xmstart>3000) { break; }
      if (client.available()) 
        {
        byte qbuf[200];
        int qlen=client.readBytes((char*)qbuf,sizeof(qbuf)-2);
        for (int i=0;i<qlen;i++) {
          char c = qbuf[i];
          if (c=='\n') {
            if (xtext[0]=='\r') ishead=false; else
            if (!ishead) ParseHTTPResult(xtext);
            lbufpos=0;
            xtext[0]=0;
            }
          else {
            if (lbufpos<sizeof(xtext)-2) {
              xtext[lbufpos]=c; lbufpos++; xtext[lbufpos]=0;
              }
            }
          }
        }
      }
    if (lbufpos>0) ParseHTTPResult(xtext);
    client.stop();
    }
  if (lxHttpOk==1) 
    {
#if modSerialLog == 1
    XLog("> OK");
    sprintf(xtext,"> Waiting (%d seconds) ...",xsiorig);
    XLog(xtext);
#endif
    lxHttpErrors=0;
    }
  else 
    {
    lxHttpErrors++;
#if modSerialLog == 1
    sprintf(xtext,"> ERR (%d/%d)",lxHttpErrors,maxErrors);
    XLog(xtext);
#endif
    if (lxHttpErrors>=maxErrors) FlashError(3);
#if modSerialLog == 1
    else
    sprintf(xtext,"> Retry @ %d seconds ...",xsr);
    XLog(xtext);
#endif
    }
  SetLiveLed(LOW);
  }
}

void ParseHTTPResult(char * xtext) { 
#if modSerialLog == 1
XLog2(xtext,"# ");
#endif
if (strcmp(xtext,"result:OK")==0) lxHttpOk=1; else
if (strncmp(xtext,"qid:",4)==0) ParseQID(xtext+4);
}

void ParseQID(char * xtext) {
if (xMpwAccess.mpwlogintimeS==0) xMpwAccess.mpwlogintimeS=lxSeconds;
char * p=strchr(xtext,'.');
if (p!=NULL)
  {  
  p[0]=0;
  xMpwAccess.mpwDeviceId=atol(xtext);
  p++;
  char * px=strchr(p,'.');
  if (px!=NULL)
    {
    px[0]=0;
    px++;
    xMpwAccess.mpwAccessId=atol(p);
    if (strlen(px)==20)
      {
      char c[30]="";
      strncpy(c,px,4);c[4]=0;xMpwAccess.mpwremoteDateYear=atoi(c);px+=4;
      strncpy(c,px,2);c[2]=0;xMpwAccess.mpwremoteDateMonth=atoi(c);px+=2;
      strncpy(c,px,2);c[2]=0;xMpwAccess.mpwremoteDateDay=atoi(c);px+=2;
      strncpy(c,px,2);c[2]=0;signed long xhour=atoi(c);px+=2;
      strncpy(c,px,2);c[2]=0;signed long xminute=atoi(c);px+=2;
      strncpy(c,px,2);c[2]=0;signed long xsecond=atoi(c);px+=2;
      strncpy(c,px,2);c[2]=0;byte xdaysinmonth=atoi(c);px+=2;
      strncpy(c,px,1);c[1]=0;byte xisdst=atoi(c);px+=1;
      strncpy(c,px,1);c[1]=0;xMpwAccess.mpwremoteDateDayOfWeek=atoi(c);px+=1;
      strncpy(c,px,2);c[2]=0;xMpwAccess.mpwremoteDateWeek=atoi(c);px+=2;

      if (xisdst==1)
        xMpwAccess.xComStatus|=xComStatusIsDST;
      else
        xMpwAccess.xComStatus&=~xComStatusIsDST;

      if (xdaysinmonth==xMpwAccess.mpwremoteDateDay)
        xMpwAccess.xComStatus|=xComStatusIsLastDayInMonth;
      else
        xMpwAccess.xComStatus&=~xComStatusIsLastDayInMonth;
        
      xMpwAccess.xComStatus&=~xComStatusXXDaysMonthMask;
      if (xdaysinmonth==28) xMpwAccess.xComStatus|=xComStatus28DaysMonth;
      if (xdaysinmonth==29) xMpwAccess.xComStatus|=xComStatus29DaysMonth;
      if (xdaysinmonth==30) xMpwAccess.xComStatus|=xComStatus30DaysMonth;
      if (xdaysinmonth==31) xMpwAccess.xComStatus|=xComStatus31DaysMonth;
        
      xMpwAccess.mpwremoteTimeS=(xhour*3600)+(xminute*60)+(xsecond);
      xMpwAccess.mpwremoteCheckedAtS=lxSeconds;
      }
    }
  
  }

/*  
char xstr[50]="";
tMpwDateTime xMpwDateTime;
GetCurrentDateTime(&xMpwDateTime);
FormatDateTimeStr(xstr, "j.n.Y H:i:s",&xMpwDateTime,sizeof(xstr));
Serial.println(xstr);
*/
}
#endif



void FormatDateTimeStr(char * xstr, char * xfmt, struct tMpwDateTime * xMpwDateTime,int xmaxlen)
{
xstr[0]=0;
while (strlen(xfmt)>0)
  {
  char ch=xfmt[0];
  char cout[20]="";
  if (ch=='d') sprintf(cout,"%02d",xMpwDateTime->xday); else
  if (ch=='j') sprintf(cout,"%d",xMpwDateTime->xday); else
  if (ch=='m') sprintf(cout,"%02d",xMpwDateTime->xmonth); else
  if (ch=='n') sprintf(cout,"%d",xMpwDateTime->xmonth); else
  if (ch=='Y') sprintf(cout,"%d",xMpwDateTime->xyear); else
  if (ch=='y') sprintf(cout,"%d",xMpwDateTime->xyear%99); else
  if (ch=='g') sprintf(cout,"%d",((xMpwDateTime->xhour%12)+1)); else
  if (ch=='G') sprintf(cout,"%d",(xMpwDateTime->xhour)); else
  if (ch=='h') sprintf(cout,"%02d",((xMpwDateTime->xhour%12)+1)); else
  if (ch=='H') sprintf(cout,"%02d",(xMpwDateTime->xhour)); else
  if (ch=='i') sprintf(cout,"%02d",(xMpwDateTime->xminute)); else
  if (ch=='s') sprintf(cout,"%02d",(xMpwDateTime->xsecond)); else
    { cout[0]=ch;cout[1]=0; }
    
  xfmt++;
  if (strlen(xstr)+strlen(cout)<xmaxlen) strcat(xstr,cout); else break;
  }
}

int GetCurrentDateTime_DaysInMonth()
{
int xret=28;
int xst=xMpwAccess.xComStatus&xComStatusXXDaysMonthMask;
if (xst==xComStatus28DaysMonth) xret=28; else
if (xst==xComStatus29DaysMonth) xret=29; else
if (xst==xComStatus30DaysMonth) xret=30; else
if (xst==xComStatus31DaysMonth) xret=31;
return xret;
}
 
void GetCurrentDateTime(struct tMpwDateTime * xMpwDateTime)
{
if (xMpwAccess.mpwremoteCheckedAtS>0)
  {
  xMpwDateTime->xyear=xMpwAccess.mpwremoteDateYear;
  xMpwDateTime->xmonth=xMpwAccess.mpwremoteDateMonth;
  xMpwDateTime->xday=xMpwAccess.mpwremoteDateDay;
  uint32_t xtime=lxSeconds-xMpwAccess.mpwremoteCheckedAtS+xMpwAccess.mpwremoteTimeS;
  if (xtime>=86400)
    {
    int xdaysover=xtime/86400UL;
    int xtimeover=xtime%86400UL;

    if (xdaysover>25) // max 25 days without time sync
      {
      xMpwDateTime->xyear=0;
      xMpwDateTime->xmonth=0;
      xMpwDateTime->xday=0;
      }
    else
      { 
      int xdaysinmonth=GetCurrentDateTime_DaysInMonth();
      for (int i=0;i<xdaysover;i++)
        {
        if (xMpwDateTime->xday<xdaysinmonth)
          xMpwDateTime->xday++;  
        else
          {
          xMpwDateTime->xday=1;
          if (xMpwDateTime->xmonth<12)
            xMpwDateTime->xmonth++;
          else
            {
            xMpwDateTime->xmonth=1;
            xMpwDateTime->xyear++;
            }
          }
        }
      }
  
    xtime=xtimeover;
    }
  xMpwDateTime->xhour=(xtime/3600)%24;
  int xmod=(xtime%3600);
  xMpwDateTime->xminute=(xmod/60)%60;
  xMpwDateTime->xsecond=xmod%60;
  }
else
  {
  uint32_t xtime=lxSeconds;
  xMpwDateTime->xyear=0;
  xMpwDateTime->xmonth=0;
  xMpwDateTime->xday=(xtime/86400UL);
  xMpwDateTime->xhour=((xtime%86400UL)/3600)%24;
  signed long xmod=((xtime%86400L)%3600);
  xMpwDateTime->xminute=(xmod/60)%60;
  xMpwDateTime->xsecond=xmod%60;    
  }
}





void FlashError(byte xcount)
{
#if modSerialLog == 1
char xtext[50];
sprintf(xtext,"Error %d.",xcount);
XLog(xtext);
XLog("Flashing err (10x) & restarting.");
#endif
xcount*=2;
for (int i=0;i<10;i++) {
  byte x=0;
  for (int i=0;i<xcount;i++) {
    SetLiveLed((!((x++)&1))?HIGH:LOW);
    delay(250);
    }
  delay(1000);
  }
#if modSerialLog == 1
XLog("Reset");
XLog("---");
#endif
delay(1000);
softReset();
}

int freeRam() {
extern unsigned int __heap_start;
extern void *__brkval;
int free_memory; int stack_here;
if (__brkval == 0) free_memory = (int) &stack_here - (int) &__heap_start;
else free_memory = (int) &stack_here - (int) __brkval; 
return (free_memory);
}


byte calcPchsum(byte xpin, struct tPinConf * pconf) {
byte*xconf=(byte*)pconf; byte xqs=pconf->chsum;
pconf->chsum=0; byte chs=0; for (int i=0;i<sizeof(tPinConf);i++) chs+=(xpin+3)+xconf[i]+i;
pconf->chsum=xqs; return chs&0xFF; }

boolean ReadPinConf(struct tPinConf * pconf,int xpin)
{
boolean xret=false;
if (xpin<NUM_ANALOG_INPUTS)
  {
  byte * pcx=(byte*)pconf;
  int sp=sizeof(tPinConf);
//  int xs=(xpin*sp);
  int xs=GetEpromPinAddr(xpin);
  if (xs>=0)
    {
    for (int ax=0;ax<sp;ax++) pcx[ax]=EEPROM.read(ax+xs);
    xret=(calcPchsum(xpin, pconf)==pconf->chsum);
    }
  }
return xret;
}

int GetEpromPinAddr(int xpin)
{
int xret=-1;
if (xpin<NUM_ANALOG_INPUTS)
  xret=(xpin*sizeof(tPinConf));
return xret;
}


// -- [WEB SERVER] ----------------------------------------------


#if modWebServer == 1

#define xWEBhome   0x00
#define xWEBcfgp   0x01
#define xWEBcfg    0x02
#define xWEBmpw    0x03
#define xWEBpins   0x04

#define xCFGnone   0x00
#define xCFGk      0x10
#define xCFGv      0x20
#define xCFGs      0x30

// w = write
// [w]as = analog settings
// [w]ds = digital settings
// [w]dp = display
// [w]ep = eprom
// hw = hwinfo
// dd = display dump
// db = display button press emulation

#define xACTnone   0x00
#define xACThw     0x01
#define xACTas     0x02
#define xACTdp     0x03
#define xACTdd     0x04
#define xACTdb     0x05
#define xACTsp     0x06
#define xACTds     0x07
#define xACTep     0x08

#define xACTwds    0x0C
#define xACTwep    0x0D
#define xACTwas    0x0E
#define xACTwdp    0x0F

#define xKEYnone          0x00
#define xKEYcmd           0x10
#define xKEYpin           0x20
#define xKEYDispButt      0x30
#define xKEYDpin          0x40

#define xPinOptNone       0x00
#define xPinOptEnabled    0x01
#define xPinOptName       0x02
#define xPinOptMin        0x03
#define xPinOptMax        0x04
#define xPinOptUnit       0x05
#define xPinOptType       0x06
#define xPinOptGroup      0x07
#define xPinCmdWrite      0xF0

#define xDPinOptNone      0x00
#define xDPinOptName      0x02

#define xDPinOptSetStatus 0xA0

#define xDPinSetStatus    0xE0

void WebServer_pf(char * xstr,signed long * xsl,int * xsl10k) {
int im; *xsl=0;*xsl10k=0; char * p=xstr; char * p10k=p;
for (int i=0;i<strlen(xstr);i++) if (p10k[0]=='.') { p10k[0]=0;p10k++;break; } else p10k++;
boolean hm=(p[0]=='-'); if (hm) { p++; } im=strlen(p);
for (int i=0;i<im;i++) { *xsl*=10; if ((p[i]>='0') && (p[i]<='9')) *xsl+=(p[i]-'0'); }
im=strlen(p10k);
for (int i=0;i<4;i++) { *xsl10k*=10; if ((i<im) && ((p10k[i]>='0') && (p10k[i]<='9'))) 
  *xsl10k+=(p10k[i]-'0'); }
if (hm) *xsl=0-*xsl;
if (hm) *xsl10k=0-*xsl10k; // minus bugfix
}

void WebServer_wp(byte xpin, struct tPinConf * pconf) {
if (xpin<NUM_ANALOG_INPUTS) {
  xMpwAccess.xComStatus|=xComStatusASchanged;
  byte*xconf=(byte*)pconf; pconf->chsum=calcPchsum(xpin,pconf);
  int ax=GetEpromPinAddr(xpin);
  if (ax>=0)
    for (int ii=0;ii<sizeof(tPinConf);ii++)
      if (EEPROM.read(ax+ii) != xconf[ii])
        EEPROM.write(ax+ii,xconf[ii]);
  }
}

void WebServer_wdp(byte xdpin, struct tDPinConf * pconf) {
if (xdpin<16) {
  xMpwAccess.xComStatus|=xComStatusDSchanged;
  DPIN_epWriteConfName(xdpin,pconf->xname);
  }
}

byte WebServer_pb(char * xstr) {
byte cb=0; for (int i=0;i<strlen(xstr);i++) { cb*=10; char c=xstr[i];
  if ((c>='0') && (c<='9')) cb+=c-'0'; }
return cb;
}

word WebServer_c(char * lbuf,char c,word xmode, byte * datastruct, unsigned long * xparam) {
byte qmode=xmode&0x0F; byte qsubmode=xmode&0xF0; byte qsaction=(xmode>>8)&0x0F; byte wkey=(xmode>>8)&0xF0;
if (qmode==xWEBhome) {
  if (strncmp(lbuf,"GET ",4)==0) {
    char * p=lbuf+5;
    if (strcmp(p,"cfg")==0) qmode=xWEBcfgp; else
    if (strcmp(p,"pin")==0) qmode=xWEBpins; else
    if (strcmp(p,"mpw")==0) qmode=xWEBmpw;
    if (qmode!=xWEBhome) lbuf[0]=0;
    }
  }
else if (qmode==xWEBcfgp) {
  if (c=='?')
    { qsubmode=xCFGk; lbuf[0]=0; }
  else if ((c=='&') || (c==' '))
    { WebServer_p(lbuf,&qsubmode,&wkey,&qsaction,datastruct,xparam); qsubmode=xCFGk; lbuf[0]=0; } 
  else if (c=='=')
    { WebServer_p(lbuf,&qsubmode,&wkey,&qsaction,datastruct,xparam); 
      if (qsubmode!=xCFGs) { qsubmode=xCFGv; strcpy(lbuf,"\n"); } else  { lbuf[0]=0; }  }
  if (c==' ') { qmode=xWEBcfg; WebServer_e(&qsaction,xparam); } else
  if (qsubmode==xCFGv) WebServer_h(lbuf); else
  if (qsubmode==xCFGs) WebServer_s(lbuf,&qsaction,xparam);
  }
return ((qmode|qsubmode)|((qsaction|wkey)<<8));
}

void WebServer_e(byte * qsaction, unsigned long  * xparam)
{
#if modDisplay == 1
if (*qsaction==xACTwdp)
  {
  byte chsum=((*xparam)&0xFF);
  int xaddr=((*xparam>>8)&0xFFFF);
  int xa,la;CalcDisplayConfEpromSpace(&xa,&la);
  int raddr=xaddr+xa+1;
  if (raddr<la) if (EEPROM.read(raddr)!=0) EEPROM.write(raddr,0);   
  if (EEPROM.read(la)!=0) EEPROM.write(la,0);
  if (EEPROM.read(xa)!=chsum) EEPROM.write(xa,chsum);
  }
#endif
}

unsigned long WebServer_ph(char * lbuf)
{
unsigned long d=0;
for (byte ii=0;ii<8;ii++) {
  char c=lbuf[ii];
  if (c='\0') break;
  d=(d<<4)+(((c>='0') && (c<='9'))?(c-'0'):(((c>='A') && (c<='F'))?(c-'A'+10):0));
  }
return d;
}

void WebServer_s(char * lbuf,byte * qsaction,unsigned long * xparam)
{
#if modDisplay == 1
if (*qsaction==xACTwdp)
  {
  if (strlen(lbuf)==2)
    {
    byte d=0;
    for (byte ii=0;ii<2;ii++) {
      char c=lbuf[ii];
      d=(d<<4)+(((c>='0') && (c<='9'))?(c-'0'):(((c>='A') && (c<='F'))?(c-'A'+10):0));
      }
    lbuf[0]=0;
    if (d==0) d=32;
    byte chsum=((*xparam)&0xFF);
    int xaddr=((*xparam>>8)&0xFFFF);
    int xa,la;CalcDisplayConfEpromSpace(&xa,&la);
    int raddr=xaddr+xa+1;
    if (raddr<la)
      {
      if (EEPROM.read(raddr)!=d) EEPROM.write(raddr,d);
      chsum+=((d&0xFF)+(xaddr+1))&0xFF;     
      xaddr++;
      } 
    *xparam=(chsum|((unsigned long)xaddr<<8));
    }
  }
#endif
}

void WebServer_h(char * lbuf)
{
int cx=strlen(lbuf);
char * p=lbuf+cx;
while ((p>=lbuf) && (p[0]!='\n')) p--;
if ((p>=lbuf) && (lbuf+cx-p==3)) {
  byte d=0;
  for (byte ii=1;ii<3;ii++) {
    d=d<<4; char c=p[ii];
    if ((c>='0') && (c<='9')) d+=c-'0';
    if ((c>='A') && (c<='F')) d+=c-'A'+10;
    }
  byte pr='\\';
  if (d==pr) { p[0]=pr;p[1]=pr;p++; } 
  if (d==0) { p[0]=pr;p[1]='0';p++; } 
  else p[0]=d;
  p++;p[0]='\n';p[1]=0;
  }
}

void WebServer_p(char * xstr,byte * qsubmode,byte * wkey,byte * qsaction, byte * datastruct, 
  unsigned long * xparam)
{
int ix=strlen(xstr);
if (ix>0){ ix--;xstr[ix]=0; }
if (* qsubmode==xCFGk)
  {
    
#if modDisplay == 1   
  if (*qsaction==xACTdb)
    {
    if (strcmp(xstr,"bid")==0) { *wkey=xKEYDispButt; }
    }    
  else
  if (*qsaction==xACTwdp)
    {
    if (strcmp(xstr,"src")==0) { *qsubmode=xCFGs; *xparam=0; }
    }
  else
#endif
  if (*qsaction==xACTwds)
    {
    char * p=xstr;
    if (strcmp(xstr,"DX")==0)
      {
      byte xpinopt=xDPinSetStatus;
      *xparam&=0xFFFFFF00;
      *xparam|=xpinopt;
      }
    else
    if (p[0]=='s')
      {
      p++;
      int xpinindex=atoi(p)&0x0F;
      byte xpinopt=xDPinOptSetStatus;
      *xparam&=0xFFFF0000;
      *xparam|=xpinopt|(xpinindex<<8);
      }
    else
    if (p[0]=='D')
      {
      p++;
      char * px=p;
      for (int ii=1;ii<strlen(xstr);ii++)
        if (p[0]=='.') { p[0]=0;p++;break; } else p++;
      int xpinindex=atoi(px);
      byte xpinopt=xDPinOptNone;
      if (strcmp(p,"nam")==0) xpinopt=xDPinOptName;
      *xparam&=0xFFFF0000;
      *xparam|=xpinopt|(xpinindex<<8);
      }   
    *wkey=xKEYDpin;
    }
  else
  if (*qsaction==xACTwas)
    {
    char * p=xstr;
    if (p[0]=='A')
      {
      p++;
      char * px=p;
      for (int ii=1;ii<strlen(xstr);ii++)
        if (p[0]=='.') { p[0]=0;p++;break; } else p++;
      byte xpinopt=xPinOptNone;
      if (strcmp(p,"ena")==0) xpinopt=xPinOptEnabled; else
      if (strcmp(p,"nam")==0) xpinopt=xPinOptName; else
      if (strcmp(p,"min")==0) xpinopt=xPinOptMin; else
      if (strcmp(p,"max")==0) xpinopt=xPinOptMax; else
      if (strcmp(p,"uni")==0) xpinopt=xPinOptUnit; else
      if (strcmp(p,"typ")==0) xpinopt=xPinOptType; else
      if (strcmp(p,"grp")==0) xpinopt=xPinOptGroup; else
      if (strcmp(p,"~w")==0) xpinopt=xPinCmdWrite;
      *xparam&=0xFFFFFF00;
      *xparam|=xpinopt;
      }   
    *wkey=xKEYpin;
    }
  else
  if (strcmp(xstr,"cmd")==0) *wkey=xKEYcmd; 
  else
    *wkey=xKEYnone;
  }
else
if (* qsubmode==xCFGv)
  {
  if (ix>0){ ix--;xstr[ix]=0; }
  
#if modDisplay == 1   
  if (*wkey==xKEYDispButt)
    {
    DisplayPressButton(xstr);
    }
  else
#endif

// ---------------------------------------

  if (*wkey==xKEYDpin) 
    {
    byte xpk=((*xparam)&0xFF);
    tDPinConf * pconf;
    pconf=(tDPinConf*)datastruct;
    
if (xpk==xDPinOptName)
  {
  int xpinindex=(((*xparam)&0x0F00)>>8);
  strncpy(pconf->xname,xstr,sizeof(pconf->xname)-1);
  DPIN_epWriteConfName(xpinindex,pconf->xname);
  for (int ii=0;ii<sizeof(tDPinConf);ii++) datastruct[ii]=0;  
  }
else
if (xpk==xDPinSetStatus) {
  DPINstatus=(uint16_t)WebServer_ph(xstr);
  DPINUpdateHW();  
  }
else
if (xpk==xDPinOptSetStatus) {
  int xpinindex=(((*xparam)&0x0F00)>>8);
  uint16_t xpinstatus=(atoi(xstr))&1;
  DPINstatus&=(~(((uint16_t)1)<<xpinindex));
  DPINstatus|=(xpinstatus<<xpinindex);
  DPINUpdateHW();  
  }

  
    }   
else
// ---------------------------------------
  if (*wkey==xKEYpin) 
    {
    byte xpk=((*xparam)&0xFF);
    tPinConf * pconf;
    pconf=(tPinConf*)datastruct;
    
if (xpk==xPinOptEnabled) {
  if (strcmp(xstr,"1")==0)
    pconf->xopt|=xpinOPTenabled;
  }
else
if (xpk==xPinOptName)
  {
  strncpy(pconf->xname,xstr,sizeof(pconf->xname)-1);
  pconf->xname[sizeof(pconf->xname)-1]=0;
  }
else
if (xpk==xPinOptMin) {
  signed long xsl=0;int xsl10000=0;
  WebServer_pf(xstr,&xsl,&xsl10000);
  pconf->xmin=xsl;pconf->xmin10000=xsl10000;
  }
else if (xpk==xPinOptMax) {
  signed long xsl=0;int xsl10000=0;
  WebServer_pf(xstr,&xsl,&xsl10000);
  pconf->xmax=xsl;pconf->xmax10000=xsl10000;
  }
else
if (xpk==xPinOptUnit)
  strncpy(pconf->xunit,xstr,sizeof(pconf->xunit)-1);
else
if (xpk==xPinOptType)
  {
  char w=xstr[0];
  pconf->xopt&=~xpinOPTtypeMask;
  if (w=='s') pconf->xopt|=xpinOPTsample; else
  if (w=='a') pconf->xopt|=xpinOPTavg; else
  if (w=='e') pconf->xopt|=xpinOPTeffective; else
  if (w=='m') pconf->xopt|=xpinOPTmin; else
  if (w=='x') pconf->xopt|=xpinOPTmax;
  }
else
if (xpk==xPinOptGroup)
  {
  char w=xstr[0];
  pconf->xopt&=~xpinOPTgroupMask;
  if (w=='0') pconf->xopt|=xpinOPTsampleG0; else
  if (w=='1') pconf->xopt|=xpinOPTsampleG1; else
  if (w=='2') pconf->xopt|=xpinOPTsampleG2; else
  if (w=='3') pconf->xopt|=xpinOPTsampleG3;
  }
else
if (xpk==xPinCmdWrite) {
    WebServer_wp(WebServer_pb(xstr),(tPinConf*)datastruct);
    for (int ii=0;ii<sizeof(tPinConf);ii++) datastruct[ii]=0;
  }
  
    }   
 // --------------------------------------------
  else
  if (*wkey==xKEYcmd) 
    {
    if (strcmp(xstr,"wep")==0) *qsaction=xACTwep;else
    if (strcmp(xstr,"wds")==0) *qsaction=xACTwds;else
    if (strcmp(xstr,"was")==0) *qsaction=xACTwas;else
    if (strcmp(xstr,"wdp")==0) *qsaction=xACTwdp;else
    if (strcmp(xstr,"hw")==0) *qsaction=xACThw;else
    if (strcmp(xstr,"as")==0) *qsaction=xACTas;else
    if (strcmp(xstr,"ds")==0) *qsaction=xACTds;else
    if (strcmp(xstr,"dp")==0) *qsaction=xACTdp; else
    if (strcmp(xstr,"sp")==0) *qsaction=xACTsp; else
    if (strcmp(xstr,"dd")==0) *qsaction=xACTdd; else
    if (strcmp(xstr,"ep")==0) *qsaction=xACTep; else
    if (strcmp(xstr,"db")==0) *qsaction=xACTdb;    
    }
  }
}

void WebServer() {
  EthernetClient client = server.available();
  if (client) {
    boolean cl = true;
    char xln[200]="";
    word xmode=xWEBhome;
    unsigned long xparam=0;
    int cx=sizeof(tPinConf);
    int cxq=sizeof(tDPinConf);if (cx<cxq) cx=cxq;
    byte xDataStruct[cx];
    for (int ii=0;ii<cx;ii++) xDataStruct[ii]=0; 
    int ix=0;
    SetLiveLed(HIGH);
    while (client.connected()) {
      if (client.available()) 
        {
        char c=client.read();
        if (c=='\n') { xln[0]=0; }
        ix=strlen(xln);
        if (ix<sizeof(xln)) { xln[ix]=c;ix++;xln[ix]=0; }
        xmode=WebServer_c(xln,c,xmode,xDataStruct,&xparam);
        byte qmode=xmode&0x0F;
        byte qsubmode=xmode&0xF0; 
        byte qsaction=(xmode>>8)&0x0F; 
        if (c == '\n' && cl) {
          client.write("HTTP/1.1 200 OK\n");
          if (qsaction==xACTep)
            {
            client.write("Content-Disposition: attachment; filename=\"");
            sprintf(xln,"ulog_eprom_%08lX.hex",millis());
            client.write((byte*)xln,strlen(xln));
            client.write("\"\n");
            client.write("Content-Type: text/plain\nConnnection: close\n\n");              
            }
          else
            client.write("Content-Type: text/html\nConnnection: close\n\n");
          if (qmode==xWEBpins) {
            for (int ain = 0; ain < NUM_ANALOG_INPUTS; ain++) {
              if (ain>0) client.write(",");
              sprintf(xln,"A%d:%d",ain,SamplerAnalogRead(ain));
              client.write((byte*)xln,strlen(xln));
              }
            } 
          else
          if (qsaction==xACThw)
            {
            sprintf(xln,"~HW\nt:%lX\nc:%d.",millis(),NUM_ANALOG_INPUTS);
            client.write((byte*)xln,strlen(xln));
            char cq[]="%d.%d.%d.%d.%d.%d.%d.%d";
            sprintf(xln,cq,(int)modStatusLedPin,(int)sendInterval,(int)sendRetry,(int)maxErrors,
            (int)lxHttpErrors,(int)modSerialLog,(int)modDisplay,(modDisplay==1)?(int)modDisplayType:(int)0);
            client.write((byte*)xln,strlen(xln));
            sprintf(xln,".%lu\nf:%s\nh:%s\nx:%d\nr:",(unsigned long)lxSeconds,modMyPowerFVEID,modMyPowerHost,(int)modMyPowerLog);
            client.write((byte*)xln,strlen(xln));
            sprintf(xln,cq,(int)freeRam(),(int)ARDUINO,(int)RAMEND,(int)XRAMEND,
            (int)E2END,(int)FLASHEND,(int)SPM_PAGESIZE,(int)E2PAGESIZE);
            client.write((byte*)xln,strlen(xln));
            sprintf(xln,"\n\nv:%s\nm:%02X%02X%02X%02X%02X%02X\nn:",MPWMicroLogVersion,
            NETmac[0],NETmac[1],NETmac[2],NETmac[3],NETmac[4],NETmac[5],NETmac[6]);
            client.write((byte*)xln,strlen(xln));          
            char cf[]="%d.%d.%d.%d";
            IPAddress xaddr;
            xaddr=Ethernet.localIP();
            for (byte i=0;i<4;i++)
              {
              if (i==1) xaddr=Ethernet.subnetMask();else
              if (i==2) xaddr=Ethernet.gatewayIP();else
              if (i==3) xaddr=Ethernet.dnsServerIP();   
              sprintf(xln,cf,xaddr[0],xaddr[1],xaddr[2],xaddr[3]);
              if (i<3) strcat(xln,"/");
              client.write((byte*)xln,strlen(xln));          
              }
            sprintf(xln,"\nw:%d/%ld\nde:%d",(int)modWebServer,(long)modWebServerPort,(int)modDisplayEmulation);
            client.write((byte*)xln,strlen(xln));
            client.write("\nds:");
            for (int i=0;i<16;i++)
              {
              int qb=DPINmap[i];
              sprintf(xln,"%d",qb);
              if (i<16) strcat(xln,",");
              client.write((byte*)xln,strlen(xln));
              }
            sprintf(xln,"/%04X/%04X\n",DPINstatus,DPINlogicmask);
            client.write((byte*)xln,strlen(xln));          

char xstr[30]="";
tMpwDateTime xMpwDateTime;
GetCurrentDateTime(&xMpwDateTime);
FormatDateTimeStr(xstr, "j.n.Y@H:i:s",&xMpwDateTime,sizeof(xstr));


            sprintf(xln,"mpw:%s/%d.%d.%d.%ld/%lu/%lu/%lu/%lu/%d/%d/%d/%lu\n",xstr,(int)xMpwAccess.mpwremoteDateYear,
              (int)xMpwAccess.mpwremoteDateMonth,
              (int)xMpwAccess.mpwremoteDateDay,
              (signed long)xMpwAccess.mpwremoteTimeS,
              xMpwAccess.mpwremoteCheckedAtS,
              xMpwAccess.mpwlogintimeS,
              xMpwAccess.mpwDeviceId,
              xMpwAccess.mpwAccessId,
              (int)xMpwAccess.xComStatus,
              (int)xMpwAccess.mpwremoteDateWeek,
              (int)xMpwAccess.mpwremoteDateDayOfWeek,
              (unsigned long)(lxSeconds-xMpwAccess.mpwremoteCheckedAtS));
            client.write((byte*)xln,strlen(xln));          
            
            }
          else
// ----------------------------------
          if ((qsaction==xACTds) || (qsaction==xACTwds))
            {
            if (qsaction==xACTwds) client.write("~WDS\n"); else client.write("~DS\n");
            for (int xp=0;xp<16;xp++)
              {
              sprintf(xln,"D%d:%d:",xp,(int)DPINmap[xp]);
              client.write((byte*)xln,strlen(xln));
              char cname[30]="";
              DPIN_epReadConfName(xp,cname,sizeof(cname));
              xln[0]=0;
              char xf[]="%02X";
              char xh[3];
              for (int i=0;i<strlen(cname);i++) {
                int xi=(uint8_t)cname[i];
                sprintf(xh,xf,xi); 
                strcat(xln,xh);
                }
              strcat(xln,"\n");
              client.write((byte*)xln,strlen(xln));
              }
            sprintf(xln,"DX:%04X/%04X\n",DPINstatus,DPINlogicmask);
            client.write((byte*)xln,strlen(xln));
            }
          else
// ----------------------------------
          if ((qsaction==xACTas) || (qsaction==xACTwas))
            {
            if (qsaction==xACTwas) client.write("~WAS\n"); else client.write("~AS\n");
            for (int xp=0;xp<NUM_ANALOG_INPUTS;xp++)
              {
              tPinConf pconf;            
              boolean isvalid=ReadPinConf(&pconf,xp);            
              sprintf(xln,"A%d:",xp);
              client.write((byte*)xln,strlen(xln));
              if (!isvalid)
                strcat(xln,"-");
              else
                { 
                char smin10k[30]="";sprint10k(smin10k,pconf.xmin,pconf.xmin10000);
                char smax10k[30]="";sprint10k(smax10k,pconf.xmax,pconf.xmax10000);
                sprintf(xln,"%X/%s/%s/",pconf.xopt,smin10k,smax10k);
                client.write((byte*)xln,strlen(xln));
                xln[0]=0;
                char xf[]="%02X";
                char xh[3];
                byte * p;
                p=(byte*)pconf.xname;
                for (int i=0;i<strlen(pconf.xname);i++) {
                  sprintf(xh,xf,p[i]); strcat(xln,xh); }
                client.write((byte*)xln,strlen(xln));
                strcpy(xln,"/");
                p=(byte*)pconf.xunit;
                for (int i=0;i<strlen(pconf.xunit);i++) {
                  sprintf(xh,xf,p[i]); strcat(xln,xh); }
                strcat(xln,"/");
                client.write((byte*)xln,strlen(xln));                 
                sprintf(xln,"%d",SamplerAnalogRead(xp));
                }              
              strcat(xln,"\n");
              client.write((byte*)xln,strlen(xln));
              }
            }
          else
          if (qsaction==xACTsp)
            {
            client.write("~SP\n");
            byte qindex=0;
            while (true)
              {
              qindex++;
              byte qpins[4];
              SamplerGet4PinGroup(qpins,&qindex);
              SamplerExec(qpins,&xSamplerStruct,4);
              sprintf(xln,"MS:%lX\nL:%lX\nT:",millis(),xSamplerStruct.tPinMicros);
              client.write((byte*)xln,strlen(xln));
              for (int x=0;x<128;x++)
                {
                sprintf(xln,"%02X",(int)xSamplerStruct.tPinTimes[x]);
                client.write((byte*)xln,strlen(xln));
                }
              client.write("\n");
              for (int p=0;p<4;p++)
                {
                tSampledPin * qSampledPin=&xSamplerStruct.tPinSamples[p];
                int qpin=qSampledPin->pin;
                if (qpin<NUM_ANALOG_INPUTS)
                  {
                  sprintf(xln,"S%d:S=%lu,A=%lu,M=%d,X=%d,C=%d:",qpin,
                    qSampledPin->smpSqr,
                    qSampledPin->smpAvg,
                    qSampledPin->smpMin,
                    qSampledPin->smpMax,
                    qSampledPin->smpCount);
                  client.write((byte*)xln,strlen(xln));
                  for (int x=0;x<128;x++)
                    {
                    sprintf(xln,"%02X",(int)qSampledPin->smpHi8[x]);
                    client.write((byte*)xln,strlen(xln));
                    }
                  client.write("/");
                  for (int x=0;x<32;x++)
                    {
                    sprintf(xln,"%02X",(int)qSampledPin->smpLo2[x]);
                    client.write((byte*)xln,strlen(xln));
                    }
                  client.write("\n");
                  }
                }
              if (qindex==0) break;              
              }
            for (int x=0;x<NUM_ANALOG_INPUTS;x++)
              {
              tPinValue * qPinValue;
              qPinValue=&xSamplerStruct.tPinValues[x];
              sprintf(xln,"A%d:V=%d,S=%d,A=%d,M=%d,X=%d\n",x,
                qPinValue->smpVal,
                qPinValue->smpSqr,
                qPinValue->smpAvg,
                qPinValue->smpMin,
                qPinValue->smpMax);
              client.write((byte*)xln,strlen(xln));
              }
            }
          else
          if ((qsaction==xACTdp) || (qsaction==xACTwdp))
            {
            if (qsaction==xACTwdp)
              client.write("~WDP\n");
            else
              client.write("~DP\n");
#if modDisplay == 1
            sprintf(xln,"T:%d\n",(int)modDisplayType);
            client.write((byte*)xln,strlen(xln));
            int xa,la;
            CalcDisplayConfEpromSpace(&xa,&la);            
            int xs=la-xa;
            sprintf(xln,"B:%d\nD:",xs-1);
            client.write((byte*)xln,strlen(xln));
            byte chsum=0;
            byte rchsum=EEPROM.read(xa);
            for (int i=1;i<xs;i++) 
              {
              int xv=EEPROM.read(xa+i);
              if (xv==0) break;
              chsum+=((xv&0xFF)+i)&0xFF;
              sprintf(xln,"%02X",xv);
              if ((i-1)%10==9) strcat(xln,"\nD:");
              client.write((byte*)xln,strlen(xln));
              }      
            sprintf(xln,"\nR:%02X\nC:%02X",rchsum,chsum);
            client.write((byte*)xln,strlen(xln));
            if (EEPROM.read(la)!=0) EEPROM.write(la,0);
            client.write("\n");
#endif
            }
          else
          
#if ((modDisplay == 1) && (modDisplayType == dLCDKeyPad1602))
          if (qsaction==xACTdd)
            {
            char lcdlbuf0[17]="";
            char lcdlbuf1[17]="";
            _lcdweblbuf0=lcdlbuf0;
            _lcdweblbuf1=lcdlbuf1;
            tDispStats lcdwebdispstats;
            _lcdwebdispstats=&lcdwebdispstats;                     
            DrawDisplayPage();
            client.write("~DD\n");
            sprintf(xln,"S:%d,%d\n",(int)lcdwebdispstats.xvarscount,(int)lcdwebdispstats.xvarram);
            client.write((byte*)xln,strlen(xln));          
            sprintf(xln,"T:%d\nL1:%s\nL2:%s\n",(int)modDisplayType,lcdlbuf0,lcdlbuf1);
            client.write((byte*)xln,strlen(xln));          
            _lcdweblbuf0=NULL;
            _lcdweblbuf1=NULL;
            _lcdwebdispstats=NULL;
            }
          else
#endif
#if (modDisplay == 1)
          if (qsaction==xACTdb)
            {
            client.write("OK");              
            }
          else
#endif         
          if (qsaction==xACTep)
            {
            sprintf(xln,"uLog/%s/eprom.0-%d/@%08lX\n",MPWMicroLogVersion,E2END,millis());
            client.write((byte*)xln,strlen(xln));
            unsigned long xsum=0;
            char s[20]="";
            for (int i=0;i<=E2END;i++)
              {
              if ((i&15)==0) 
                {
                sprintf(xln,"%04X: ",i);
                client.write((byte*)xln,strlen(xln));
                }
              int v=EEPROM.read(i);
              sprintf(xln,"%02X",v);
              int xi=i&15;
              s[xi]=((v>32)&&(v<127))?v:'.';
              s[xi+1]=0;
              client.write((byte*)xln,strlen(xln));          
              xsum=(((xsum>>24)&0xFF)|(xsum<<8))+v+i;
              if ((i&15)==15) 
                {
                sprintf(xln," %s\n",s);
                client.write((byte*)xln,strlen(xln));
                s[0]=0;
                }
              else client.write(" ");
              }
            if (s[0]!=0)
              {
              sprintf(xln," %s\n",s);
              client.write((byte*)xln,strlen(xln));
              s[0]=0;
              }
            sprintf(xln,"S:%08lX",xsum);
            client.write((byte*)xln,strlen(xln));                      
            }
          else
          if (qmode==xWEBcfg)  
            {
            client.write("CFG");
            }
          else
            {
            client.write("<!DOCTYPE HTML>\n<html><head><meta http-equiv=\"Content-Type\" ");
            client.write("content=\"text/html; charset=UTF-8\"><style>");
            client.write("a {color:#FFC000;} body {font-family:sans;font-size:12px;background:#101010;color:white}");
            client.write("</style></head>");
            if (qmode==xWEBhome) client.write("<meta http-equiv=\"refresh\" content=\"5\">");
            client.write("<body>");
            client.write("<b><a href=\"./\">HOME</a> | ");
            client.write("<a href=\"mpw\">TOOLS</a> | ");
            client.write("<a target=\"_blank\" href=\"http://microlog.mypower.cz\">WIKI</a></b><hr>");
            if (qmode==xWEBhome) {
  

            for (int x=0;x<NUM_ANALOG_INPUTS;x++)
              {
              tPinValue * qPinValue;
              qPinValue=&xSamplerStruct.tPinValues[x];
              sprintf(xln,"A%d: V=%d, S=%d, A=%d, M=%d, X=%d <br>\n",x,
                qPinValue->smpVal,
                qPinValue->smpSqr,
                qPinValue->smpAvg,
                qPinValue->smpMin,
                qPinValue->smpMax);
              client.write((byte*)xln,strlen(xln));
              }
// show extbus values
#if modExtBus == 1
            xln[0] = 0;
            for (int i=0;i<xExtBusValCount;i++)
              {
              sprintf(xln,"EXTBUS_%d_%d=", xExtBus[xExtBusValue[i].bus].pin, xExtBusValue[i].valOffset);
              ftoa(xln + strlen(xln), xExtBusValue[i].cur, 2);
              sprintf(xln + strlen(xln), " <br>\n");
              client.write((byte*)xln, strlen(xln));
              }
#endif

/*              
              
              for (int ain = 0; ain < NUM_ANALOG_INPUTS; ain++) {
                sprintf(xln,"<b>A%d:</b> %d<br>",ain,SamplerAnalogRead(ain));
                client.write((byte*)xln,strlen(xln));
                }

*/
              } 
            else
            if (qmode==xWEBmpw) {
              client.write("<script language=\"javascript\" src=\"http://");
              client.write(modMyPowerHost);
              client.write("/microlog-tools?");
              client.write(MPWMicroLogVersion);
              client.write("\"></script>");
              client.write("<div x=\"n\">Connecting to ");
              client.write(modMyPowerHost);
              client.write("...</div>");
              }
            client.write("<hr><font color=gray size=2><b>powered by ");
            client.write("<a href=\"http://mypower.cz\">MyPower.CZ</a> Microlog</b></font>");
            client.write("</body></html>\n");
            }
          break;
        }
        if (c == '\n') cl = true; else if (c != '\r') cl=false;
        }
      }
    SetLiveLed(LOW);
    delay(1);
    client.stop();
  }
}
#endif




// -- [DISPLAY] ----------------------------------------------


#if (modDisplay == 1)

#if (modDisplayType == dLCDKeyPad1602)

#define lcdPageSelect   0xFA
#define lcdPageLeft     0xFB
#define lcdPageRight    0xFC
#define lcdPageNone     0xFF

struct tDispVar {
char * xname;
signed long xval;
int xval10000;
char * xunit;
};

char lcdLastButton=0;
unsigned long lcdLastButtonTime=0;

void DisplayDetectButton()
{
char qbutt=0;
int xb=analogRead(0);
if (xb<50) qbutt='R'; else
if (xb<200) qbutt='U'; else
if (xb<400) qbutt='D'; else
if (xb<550) qbutt='L'; else
if (xb<800) qbutt='S';
if ((qbutt!=lcdLastButton) && ((millis()-lcdLastButtonTime)>200))
  {
  if ((lcdLastButton==0) && (qbutt!=0))
    DisplayPressButton(&qbutt);
  lcdLastButton=qbutt;
  lcdLastButtonTime=millis();
  }

}

void DisplayPressButton(char * xbutt)
{
char p=xbutt[0];
if (p=='X') { delay(1000); softReset(); }
else if (p=='U') { if (lcdCurrentPage==0) lcdCurrentPage=lcdMaxPage; else lcdCurrentPage--; }
else if (p=='D') { if (lcdCurrentPage>=lcdMaxPage) lcdCurrentPage=0; else lcdCurrentPage++; }
else if (p=='R') lcdCurrentPage=lcdPageRight;
else if (p=='L') lcdCurrentPage=lcdPageLeft;
else if (p=='S') lcdCurrentPage=lcdPageSelect;
DrawDisplayPage();
if (lcdCurrentPage>lcdMaxPage) { lcdCurrentPage=0; DrawDisplayPage(); }
}

int DisplayGetConfigVarNames(char * xnamebuf,int xbuflen,int xa,int xs)
{
boolean hasvar=false;
char cvn[30]="\n";
strcpy(xnamebuf,cvn);
int xret=0;
for (int i=1;i<=xs;i++)
  {
  char c=EEPROM.read(xa+i);
  if (hasvar)
    {
    if ((c=='.') && ((i<=xs)-2)) { i++;c=EEPROM.read(xa+i);
      if ((c>='0') && (c<='4')) i++;
      c='#';  }
    if ( (DisplayParseVariables_IsVarChar(c)) && (c!='#') )
      {
      int xq=strlen(cvn);
      if (xq<sizeof(cvn)-2) { cvn[xq]=c;xq++;cvn[xq]=0; }
      continue;
      }
    else
      {
      if (cvn[1]!='@')
        {
        strcat(cvn,"\n");
        if (strstr(xnamebuf,cvn)==NULL)
          {
          int qx=(strlen(cvn)-1);
          if (((strlen(xnamebuf)+qx)<(xbuflen-1)) && (qx>1))
            {
            char *p=cvn+1;
            strcat(xnamebuf,p);
            xret++;
            }
          }
        }
      strcpy(cvn,"\n");
      if (c=='#')
        {
        hasvar=false;
        continue;
        }
      }
    }
  if (c==0) break;
  hasvar=(c=='$');
  }
return (xret<1?1:xret);
}

struct tDispVar * DisplayFindVarByName(char * xname,byte * xvars, int xmaxvars) {
tDispVar * xret=NULL;
tDispVar * xvar=(tDispVar *)xvars;
for (int i=0;i<xmaxvars;i++) {
  if (strcmp(xvar->xname,xname)==0) { xret=xvar; break; }
  xvar++; }
return xret;
}

void DisplayInitConfigVars(char * xnamebuf,byte * xvars, int xmaxvars)
{
char * p=xnamebuf;
if (p[0]=='\n')
  {
  p++;
  tDispVar * xvar=(tDispVar *)xvars;
  char * q=p;
  int ix=strlen(p);
  int qcvars=0;
  for (int i=0;i<ix;i++)
    if (p[i]=='\n') {
      p[i]=0;
      if (q[0]!=0)
        {
        xvar->xname=q;
        xvar->xval=0;
        xvar->xval10000=0;
        xvar->xunit=NULL;    
        qcvars++;
        if (qcvars<xmaxvars) xvar++; else break;
        }
      q=p+1+i;
      }
  }
}

void DisplayParseConfig(byte * qWpage)
{
int xa,la;
CalcDisplayConfEpromSpace(&xa,&la);
int xs=la-xa;
byte chsum=0;
byte rchsum=EEPROM.read(xa);
for (int i=1;i<xs;i++) { int xv=EEPROM.read(xa+i);
  if (xv==0) break; chsum+=((xv&0xFF)+i)&0xFF; }      
if (rchsum==chsum) {
  byte xpage=0xFF;
  byte xpageline=0xFF;
  char xln[60]="";
  char xnamebuf[200]="\n";
  int xmaxvars=DisplayGetConfigVarNames(xnamebuf,sizeof(xnamebuf),xa,xs);
  tDispVar xvars[xmaxvars];
  if (_lcdwebdispstats!=NULL) {
    _lcdwebdispstats->xvarscount=xmaxvars;
    _lcdwebdispstats->xvarram=sizeof(xvars);
    }
  DisplayInitConfigVars(xnamebuf,(byte*)xvars, xmaxvars);
  word xflag=lcdPageNone;
  for (int i=1;i<xs;i++) {
    int xv=EEPROM.read(xa+i);
    if ((xv==0) || (xv=='\n')) {
      char * p=xln;
      while ((p[0]!=0) && (p[0]==32)) p++;
      for (int ii=0;p[ii]!=0;ii++) if ((p[ii]=='/') && (p[ii+1]=='/')) { p[ii]=0;break; }
      for (int ii=strlen(p)-1;ii>=0;ii--) if (p[ii]==32) p[ii]=0; else break;
      if (p[0]!=0) DisplayParseConfigL(p,&xpage,&xpageline,qWpage,(byte*)xvars,xmaxvars,&xflag);
      xln[0]=0;
      }
    else 
      { int xls=strlen(xln); if (xls<sizeof(xln)-1) { xln[xls]=xv;xln[xls+1]=0; } }
    if (xv==0) break;
    }
  if (xpage!=0xFF) lcdMaxPage=xpage;
  }
}

void DisplayParseConfigL(char * xln,byte * xpage,byte * xpageline,byte * qWpage, byte * xvars, int xmaxvars,word * xflag)
{
if (xln[0]=='#')
  {
  *xflag=lcdPageNone;
  if (xln[1]=='R') *xflag=lcdPageRight; else
  if (xln[1]=='L') *xflag=lcdPageLeft; else
  if (xln[1]=='S') *xflag=lcdPageSelect;
  (*xpage)++;
  (*xpageline)=0xFF;
  }
else
if ((*xpage)==0xFF)
  DisplayParseConfigCmdL(xln, xvars, xmaxvars, xflag);
else
if ((*xpage)!=0xFF)
  {
  if ((*xpageline)==0xFF) (*xpageline)=0; else (*xpageline)++; 
  if (((*qWpage)==(*xpage)) || ((*xflag)==(*qWpage)))
    {
    *qWpage=(*xpage);
    *xflag=lcdPageNone;
    if (*xpageline<2)
      {
      lcdClearLine();
      char * p=xln;
      for (int i=0;i<strlen(xln);i++) if (p[i]=='|') { p+=i;p[0]=0;p++;break; }
      char px[20];
      if (p==xln)
        {       
        DisplayParseVariables(px,xln,sizeof(px),xvars,xmaxvars);
        lcdPrintCenter(px);
        }
      else
        {
        DisplayParseVariables(px,xln,sizeof(px),xvars,xmaxvars);
        lcdPrintLeft(px);
        px[0]=0;
        DisplayParseVariables(px,p,sizeof(px),xvars,xmaxvars);
        lcdPrintRight(px);
        }
      lcdPrintLine(*xpageline);
      }
    }
  }
}

#define xVarOpNONE  0x00
#define xVarOpADD   0x01
#define xVarOpSUB   0x02
#define xVarOpMUL   0x03
#define xVarOpDIV   0x04
#define xVarOpEQ    0x05

void DisplayParseConfigCmdL(char * xln, byte * xvars, int xmaxvars, word * xflag)
{
byte xop=xVarOpNONE;
tDispVar constVar;
tDispVar * xr=NULL;
tDispVar * xvar=NULL;
int xconstdecs=-1;
boolean hasvar=false;
boolean hasconst=false;
char cvarname[20]="";
for (int i=0;i<=strlen(xln);i++)
  {
  char c=xln[i];
  if ((hasvar) && (!hasconst))
    {
    if (c=='.') { c=xln[i+1]; if ((c>='0') && (c<='9')) { i++; } c='#';  }
    if ( (DisplayParseVariables_IsVarChar(c)) && (c!='#'))
      {
      int xq=strlen(cvarname);
      if (xq<sizeof(cvarname)-1) { cvarname[xq]=c;xq++;cvarname[xq]=0; }
      continue;
      }
    else
      {
      char qunit[10]="";
      constVar.xname=cvarname;
      constVar.xunit=qunit;
      constVar.xval=0;
      constVar.xval10000=0;
      xvar=&constVar;
      if (cvarname[0]=='@')
        {
        char r=cvarname[1];
        char *p=cvarname+2;
        int xpin=0;
        for (int ic=0;ic<strlen(p);ic++)
          { xpin*=10; char e=p[ic]; if ((e>='0') || (e<='9')) xpin+=e-'0'; }
        xvar->xname=cvarname;
        xvar->xunit=qunit;
        xvar->xval=0;
        xvar->xval10000=0;
        
        if ( (xvar->xname[0]=='@') && (xvar->xname[1]=='T') )
          DisplayFillVarByTimeVar(xvar);
        else
          DisplayFillVarByPinValue(xpin,xvar,r);
        
        }
      else
        xvar = DisplayFindVarByName(cvarname,xvars,xmaxvars);

      if ((xvar!=NULL) && (xr!=NULL) && (xop!=xVarOpNONE)) {          
        if (xop==xVarOpEQ)
          { xr->xval=xvar->xval; xr->xval10000=xvar->xval10000; }
        else
          DisplayDoVarOp(xr,xvar,xop);
        xop=xVarOpNONE;
        }     
      cvarname[0]=0;
      if (c=='#') { hasvar=false; continue; }
      }
    }
  else
  if ((!hasvar) && (hasconst))
    {
    boolean isnum=((c>='0') && (c<='9'));
    if ((c=='.') && (xconstdecs<0))
      {
      xconstdecs=0;
      continue;
      }
    else
    if ((isnum) && (xvar!=NULL))
      {
      int cn=c-'0';
      if (xconstdecs<0)
        {
        if (xconstdecs>=-9)
          {
          xconstdecs--;
          xvar->xval*=10;
          xvar->xval+=(signed long)cn;
          }
        }
      else
      if (xconstdecs<4)
        {
        xconstdecs++;
        xvar->xval10000*=10;
        xvar->xval10000+=cn;
        }
      continue;
      }
    else
      {
       
      if ((xvar!=NULL) && (xr!=NULL) && (xop!=xVarOpNONE)) {     
        if (xconstdecs>=0) while (xconstdecs<4) { xconstdecs++; xvar->xval10000*=10; }
        if (xop==xVarOpEQ)
          { xr->xval=xvar->xval; xr->xval10000=xvar->xval10000; }
        else
          DisplayDoVarOp(xr,xvar,xop);
        xop=xVarOpNONE;
        }     
        
      xvar=NULL;
      hasconst=false;
      xconstdecs=-1;
      }
    }
  
  if (c==0) break;
  hasvar=(c=='$');
  if (!hasvar) { 
     
    hasconst=((c>='0') && (c<='9'));
    if (hasconst)
      {
      constVar.xval=0;
      constVar.xval10000=0;
      xvar=&constVar;
      i--;
      continue;
      }

    if (c!=' ') 
      {      
      byte xlastop=xop;
      if (c=='=') xop=xVarOpEQ; else
      if (c=='+') xop=xVarOpADD; else
      if (c=='-') xop=xVarOpSUB; else
      if (c=='*') xop=xVarOpMUL; else
      if (c=='/') xop=xVarOpDIV;
      if ((xlastop==xVarOpNONE) && (xop==xVarOpEQ) && (xvar!=NULL) && (xr==NULL)) 
        { xr=xvar; xvar=NULL; }
      }
    else
      hasconst=false;
    }
  }
}


void DisplayDoVarOp(struct tDispVar * xr,struct tDispVar * xvar,byte xop)
{ 
int64 xq1,xq2,xqr;
if (xop==xVarOpMUL) {
  i64x10k2int10kH(&xq1, xr->xval,xr->xval10000);
  i64x10k2int10kH(&xq2, xvar->xval,xvar->xval10000);
} else {
  i64x10k2int10k(&xq1, xr->xval,xr->xval10000);
  i64x10k2int10k(&xq2, xvar->xval,xvar->xval10000);
  }
if (xop==xVarOpADD) i64addS(&xqr,&xq1,&xq2); else
if (xop==xVarOpSUB) i64subS(&xqr,&xq1,&xq2); else
if (xop==xVarOpDIV) { word xmod; i64divW(&xqr, &xmod, &xq1, xvar->xval); } else
if (xop==xVarOpMUL) { int64 xretH; i64mul64(&xretH, &xqr, &xq1, &xq2); }
i64int10k2x10k(&xr->xval,&xr->xval10000,&xqr);
}

void PrintComputedPinValue(int xpin,char * xret,int maxunitsize,char xpintype)
{
tDispVar xvar;
DisplayFillVarByPinValue(xpin,&xvar,xpintype);
//signed long xval=xvar.xval;
//int xval10k=xvar.xval10000;
sprint10k(xret,xvar.xval,xvar.xval10000); //minus bugfix
//sprintf(xret,"%ld.%04d",xval,xval10k);
}

void DisplayFillVarByPinValue(int xpin,struct tDispVar * xvar,char xpintype)
{
if ((xpin<NUM_ANALOG_INPUTS) && (xvar!=NULL))
  {
  tPinConf pconf;
  byte qpintype=xpinOPTdefault; 
  if ((xpintype=='A') || (xpintype=='a')) qpintype=xpinOPTavg; else
  if ((xpintype=='R') || (xpintype=='r')) qpintype=xpinOPTeffective; else
  if ((xpintype=='S') || (xpintype=='s')) qpintype=xpinOPTsample; else
  if ((xpintype=='M') || (xpintype=='m')) qpintype=xpinOPTmin; else
  if ((xpintype=='X') || (xpintype=='x')) qpintype=xpinOPTmax; 
  int pinval=SamplerAnalogReadType(xpin,qpintype);
  xvar->xval=pinval;
  xvar->xval10000=0;
  if ((xpintype>='A') && (xpintype<='Z'))
    {
    boolean isvalid = ReadPinConf(&pconf,xpin);
    if (isvalid)
      {
      int qxval10000;
      signed long qxval;
      DisplayMapPinValue(pinval,&qxval,&qxval10000,
        pconf.xmin,pconf.xmin10000,pconf.xmax,pconf.xmax10000);
      xvar->xval=qxval;
      xvar->xval10000=qxval10000;
      }
    }
  }
}

void DisplayMapPinValue(int pinval,signed long * qxval, int * qxval10k,
      signed long xmin, int xmin10k, signed long xmax, int xmax10k)
{
*qxval10k=0;*qxval=0;int64 qmin,qdif,qx,qml,qmx;i64nul(&qmin);i64nul(&qdif);i64nul(&qx);
i64x10k2int10k(&qmin,xmin,xmin10k);i64x10k2int10k(&qx,xmax,xmax10k);
boolean msf=qmin.msf;i64subS(&qdif,&qx,&qmin);qmin.msf=msf;i64nul(&qx);qx.ll=pinval;
msf=qdif.msf;qdif.msf=false;i64mul64(&qmx,&qml,&qx,&qdif);word xmod;
i64divW(&qmx,&xmod,&qml,1023);qmx.msf=msf;i64addS(&qml,&qmx,&qmin);
i64int10k2x10k(qxval, qxval10k, &qml);
}


boolean DisplayParseVariables_IsVarChar(char c) {
return ( ((c>='a') && (c<='z')) || ((c>='A') && (c<='Z')) || (c=='_') || 
  (c=='@') || (c=='#') || (c=='.') || ((c>='0') && (c<='9')) ); }

void DisplayParseVariables(char * xdst,char * xsrc, int xmaxlen, byte * xvars,int xmaxvars)
{
int ix=0;
boolean hasvar=false;
char cvarname[20]="";
byte xdecs=0xFF;
for (int i=0;i<=strlen(xsrc);i++)
  {
  char c=xsrc[i];
  if (hasvar)
    {
    if (c=='.') { c=xsrc[i+1];
      if (c=='R') { xdecs=0xFE; i++; }  else
      if ((c>='0') && (c<='9')) { xdecs=c-'0';i++; }
      c='#';  }
    if ( (DisplayParseVariables_IsVarChar(c)) && (c!='#'))
      {
      int xq=strlen(cvarname);
      if (xq<sizeof(cvarname)-1) { cvarname[xq]=c;xq++;cvarname[xq]=0; }
      continue;
      }
    else
      {
      char qunit[10]="";
      tDispVar pinVar;
      pinVar.xname=cvarname;
      pinVar.xunit=qunit;
      pinVar.xval=0;
      pinVar.xval10000=0;
      tDispVar * xvar=&pinVar;
      if (cvarname[0]=='@')
        {
        char r=cvarname[1];
        char *p=cvarname+2;
        int xpin=0;
        for (int ic=0;ic<strlen(p);ic++)
          { xpin*=10; char e=p[ic]; if ((e>='0') || (e<='9')) xpin+=e-'0'; }
        xvar->xname=cvarname;
        xvar->xunit=qunit;
        xvar->xval=0;
        xvar->xval10000=0;       
        DisplayFillVarByPinValue(xpin,xvar,r);
        }
      else
        xvar = DisplayFindVarByName(cvarname,xvars,xmaxvars);

      if (xvar!=NULL)
        {
        char pxval[50]="";
        if ( (xvar->xname[0]=='@') && (xvar->xname[1]=='T') )
          {
          DisplayGetValueByTimeVar(pxval,xvar->xname,sizeof(pxval));
          }
        else
          {
          char qsc[]=" kMGT";
          char *xsc=qsc;
          signed long xval=xvar->xval;
          int xval10k=xvar->xval10000;

          if (xdecs==0xFE)
            {
            sprintf(pxval,"%ld",xval);
            }
          else
            {
            while (xval>1500)
              {
              if (xsc[1]==0) break;
              int64 xq,xr; word xmod;
              i64x10k2int10k(&xq, xval,xval10k);
              i64divW(&xr, &xmod, &xq, 1000);
              i64int10k2x10k(&xval,&xval10k,&xr);
              xsc++;
              }
            sprint10k(pxval,xval,xval10k); // minus bugfix
    //        sprintf(pxval,"%ld.%04d",xval,xval10k);
            DisplayCorrectDecs(pxval,xdecs);
            if ((xsc[0]!=' ') && (xsc[0]!=0)) { xsc[1]=0; strcat(pxval,xsc); }
            }
          }
        for (int cc=0;cc<strlen(pxval);cc++)
          if (ix<xmaxlen-1) { xdst[ix]=pxval[cc];ix++;xdst[ix]=0; }
        }
      cvarname[0]=0;
      if (c=='#') 
        {
        hasvar=false;
        continue;
        }
      }
    }
  if (c==0) break;
  hasvar=(c=='$');
  if ((!hasvar) && (ix<xmaxlen-1)) { xdst[ix]=c;ix++;xdst[ix]=0; }
  }
}


void DisplayFillVarByTimeVar(struct tDispVar * xvar)
{
char px[30]="";
DisplayGetValueByTimeVar(px,xvar->xname,sizeof(px));
xvar->xval=atol(px);
xvar->xval10000=0;
}


void DisplayGetValueByTimeVar(char * xbuf,char * xvarname,int xbufmaxsize)
{
xbuf[0]=0;
if ((xvarname[0]=='@') && (xvarname[1]=='T'))
  {
  if (strlen(xvarname)>=3)
    {
char xfmt[5]="";xfmt[0]=xvarname[2];xfmt[1]=0;
tMpwDateTime xMpwDateTime;GetCurrentDateTime(&xMpwDateTime);
FormatDateTimeStr(xbuf,xfmt,&xMpwDateTime,xbufmaxsize);
    }
  }
}

void DisplayCorrectDecs(char * pxval, byte xdecs)
{
int xl=strlen(pxval);
if (xl>0) {
  if (xdecs==0xFF) { for (;;) { xl=strlen(pxval); if ((xl==0) || (pxval[xl-1]!='0')) break; pxval[xl-1]=0; } }
  else { if (xdecs>4) xdecs=4; byte qdecs=4-xdecs;
    for (byte xx=0;xx<qdecs;xx++) { xl=strlen(pxval);if (xl==0) break; pxval[xl-1]=0; } }
  xl=strlen(pxval); if (xl>0) if (pxval[xl-1]=='.') pxval[xl-1]=0;
  }
}

void DrawDisplayPage()
{
if (lcdCurrentPage==0xFF)
  {
  lcdClear();
  lcdCurrentPage=0;
  }
DisplayParseConfig(&lcdCurrentPage);
}

#endif

#endif














// -- [SAMPLER] ----------------------------------------------

int SamplerAnalogRead(byte xpin) 
{
return SamplerAnalogReadType(xpin,xpinOPTdefault); 
}


void SamplerReset() { 
  for (int i=0;i<NUM_ANALOG_INPUTS;i++) 
    xSamplerStruct.tPinValues[i].smpTime=0; 
  }

int SamplerAnalogReadType(byte xpin,byte forcextype)
{
int xret=0;
if (xpin<NUM_ANALOG_INPUTS)
  {
  tPinValue * xpinvalue;
  xpinvalue=&xSamplerStruct.tPinValues[xpin];
  byte xtype=SamplerGetPinType(xpin);
  if (forcextype!=xpinOPTdefault) xtype=forcextype;
  if (xtype==xpinOPTavg) xret=xpinvalue->smpAvg;else 
  if (xtype==xpinOPTeffective) xret=xpinvalue->smpSqr;else 
  if (xtype==xpinOPTmin) xret=xpinvalue->smpMin;else 
  if (xtype==xpinOPTmax) xret=xpinvalue->smpMax;else
    xret=xpinvalue->smpVal;
  }
return xret;
}

void SamplerRun()
{
byte qpins[4];
SamplerGetNext4PinGroup(qpins); 
SamplerExec(qpins,&xSamplerStruct,4);
}

byte SamplerGroupindex=0;
unsigned long Stime=0;

void SamplerGetNext4PinGroup(byte * qpins)
{
SamplerGroupindex++;
SamplerGet4PinGroup(qpins,&SamplerGroupindex);
}

byte SamplerGetPinType(byte xpin)
{
byte xret=xpinOPTsample;tPinConf pconf;
if (ReadPinConf(&pconf,xpin)) xret=pconf.xopt&xpinOPTtypeMask;
return xret;
}

byte SamplerGetPinGroup(byte xpin)
{
byte xret=0;
byte qret=0;
tPinConf pconf;
if (ReadPinConf(&pconf,xpin)) {
  qret=pconf.xopt&xpinOPTgroupMask;
  if (qret==xpinOPTsampleG0) xret=0; else
  if (qret==xpinOPTsampleG1) xret=1; else
  if (qret==xpinOPTsampleG2) xret=2; else
  if (qret==xpinOPTsampleG3) xret=3;
  }
return xret;
}

void SamplerGet4PinGroup(byte * qpins,byte * qgroupindex)
{
char pgroups[4][NUM_ANALOG_INPUTS+1];
for (int x=0;x<4;x++) strcpy(pgroups[x],"");
char qch[2]="";qch[1]=0;
for (int x=0;x<NUM_ANALOG_INPUTS;x++) { qch[0]='A'+x; strcat(pgroups[SamplerGetPinGroup(x)],qch); }
char pgroups4[NUM_ANALOG_INPUTS][5];
for (int x=0;x<NUM_ANALOG_INPUTS;x++) strcpy(pgroups4[x],"");
int p4index=0;
for (int x=0;x<4;x++) {
  while (strlen(pgroups[x])>0) {
    strncpy(pgroups4[p4index],pgroups[x],4);
    pgroups4[p4index][4]=0;
    if (strlen(pgroups[x])>4) {
      char * p=(pgroups[x])+4;
      strcpy(pgroups[x],p);
      }
    else
      strcpy(pgroups[x],"");
    p4index++;
    }
  }
for (int p4s=0;p4s<NUM_ANALOG_INPUTS;p4s++) {
  int xl=strlen(pgroups4[p4s]);
  if (xl==4) continue; 
  else {
    int xr=4-xl;
    boolean xfound=false;
    while (!xfound) {
      for (int x=p4s+1;x<NUM_ANALOG_INPUTS;x++)
        if (strlen(pgroups4[x])==xr) {
          strcat(pgroups4[p4s],pgroups4[x]);
          strcpy(pgroups4[x],"");
          xfound=true;
          break;
          }
      if (xfound) break; else if (!xfound) { if (xr>0) xr--; else break; }
      }
    }
  }
byte p4count=0;
for (int x=0;x<NUM_ANALOG_INPUTS;x++) if (strlen(pgroups4[x])==0) { p4count=x;break; }
for (int x=0;x<4;x++) qpins[x]=0xFF;
byte idx=0;
if (p4count>0) {
  idx=*qgroupindex%p4count;
  *qgroupindex=idx;
  char *p=pgroups4[idx];
  int xl=strlen(p);
  if (xl>4) xl=4;
  if (xl!=0)
    for (int x=0;x<xl;x++)
      qpins[x]=(p[x])-'A';
  }
}

void SamplerResetPinStruct(int xpin,struct tSampledPin * xSampledPin)
{
xSampledPin->pin=xpin;
for (int x=0;x<sizeof(xSampledPin->smpHi8);x++) xSampledPin->smpHi8[x]=0;
for (int x=0;x<sizeof(xSampledPin->smpLo2);x++) xSampledPin->smpLo2[x]=0;
xSampledPin->smpSqr=0;
xSampledPin->smpAvg=0;
xSampledPin->smpMin=0x3FF;
xSampledPin->smpMax=0x000;
xSampledPin->smpCount=0;
}


void SamplerExec(byte * pins, struct tSamplerStruct * qSamplerStruct,byte xpinscount)
{
byte * xtimes=qSamplerStruct->tPinTimes;
tSampledPin * xPinSamples=qSamplerStruct->tPinSamples;
unsigned long wqms,wqme;
for (int x=0;x<xpinscount;x++) SamplerResetPinStruct(pins[x],&xPinSamples[x]);
for (int x=0;x<128;x++) xtimes[x]=0;
int qscount=sizeof(xPinSamples[0].smpHi8);
int smpCount=0;
unsigned long xxs,xms,xm1;
unsigned long qms,qm1;
int qsample=0;
int inputpin=0;
tSampledPin * qSampledPin;
int xs3=0;
int xs2=0;
xxs=micros();
xms=0;
qms=0;
while (true) { xm1=micros()-xxs; if ((xm1>>9)!=qms) break; }
xms=xm1;

wqms=micros();
for (int x=0;x<qscount;x++)
  {
  xs2=x>>2;
  xs3=(x&0x03)<<1;
  for (int p=0;p<xpinscount;p++)
    {
    qSampledPin=&xPinSamples[p];
    if (qSampledPin->pin<NUM_ANALOG_INPUTS) qsample=analogRead(qSampledPin->pin); else qsample=0;
    qSampledPin->smpHi8[x]=(byte)(qsample>>2);
    qSampledPin->smpLo2[xs2]|=(byte)((qsample&0x03)<<xs3);
    }
  smpCount++;  
  qms=xms>>9;
  while (true) { xm1=micros()-xxs; if ((xm1>>9)!=qms) break; }
  xtimes[x]=(byte)( (xm1-xms) - 400 );
  xms=xm1;
  }

wqme=micros()-wqms;
qSamplerStruct->tPinMicros=wqme;

for (int x=0;x<qscount;x++)
  {
  xs2=x>>2;
  xs3=(x&0x03)<<1;
  for (int p=0;p<xpinscount;p++)
    {
    qSampledPin=&xPinSamples[p];
    qsample=(((int)qSampledPin->smpHi8[x])<<2)|((qSampledPin->smpLo2[xs2]>>xs3)&0x03);
    qSampledPin->smpAvg+=qsample;
    if (qSampledPin->smpMax<qsample) qSampledPin->smpMax=qsample;
    if (qSampledPin->smpMin>qsample) qSampledPin->smpMin=qsample;
    unsigned long qs=qsample;qs*=qs;
    qSampledPin->smpSqr+=qs;
    }
  }

byte qpoint=64;
for (int p=0;p<xpinscount;p++)
  {
  qSampledPin=&xPinSamples[p];
  byte qxpin=qSampledPin->pin;
  if (qxpin<NUM_ANALOG_INPUTS)
    {
    qSampledPin->smpSqr=sqrt(qSampledPin->smpSqr/smpCount);
    qSampledPin->smpAvg=qSampledPin->smpAvg/smpCount;
    qSampledPin->smpCount=smpCount;
    tPinValue * xpinvalue=&qSamplerStruct->tPinValues[qxpin];
    xs3=(qpoint&0x03)<<1;
    xpinvalue->smpVal=(((int)qSampledPin->smpHi8[qpoint])<<2)|((qSampledPin->smpLo2[qpoint>>2]>>xs3)&0x03);;
    xpinvalue->smpSqr=qSampledPin->smpSqr;
    xpinvalue->smpAvg=qSampledPin->smpAvg;
    xpinvalue->smpMin=qSampledPin->smpMin;
    xpinvalue->smpMax=qSampledPin->smpMax;
    xpinvalue->smpTime=millis();
    }
  }
}




// -- [MATH10k] -------------------------------------------------

boolean isnegative10k(signed long xval, int x10k) { 
  boolean xret=(xval<0);if (xval==0) xret=(x10k<0);return xret; 
}

void sprint10k(char * xret, signed long xval, int x10k) {
char xsgn[5]="";if (isnegative10k(xval,x10k)) strcpy(xsgn,"-");
if (xval<0) xval=-xval; if (x10k<0) x10k=-x10k;
sprintf(xret,"%s%ld.%04d",xsgn,xval,x10k);
}

// -- [MATH64] -------------------------------------------------

void i64print(struct int64 * xval,char * xstr) { 
  char x[200]="";char xm[5]="";if (xval->msf) strcpy(xm,"-");sprintf(x,"--[ %s ] --\n%s%04lX%04lX%04lX%04lX",xstr,xm,
  (long)xval->hh,(long)xval->hl,(long)xval->lh,(long)xval->ll);Serial.println(x); }

void i64cpy(struct int64 * xdst,struct int64 * xsrc) { xdst->hh=xsrc->hh;xdst->hl=xsrc->hl;
  xdst->lh=xsrc->lh;xdst->ll=xsrc->ll;xdst->msf=xsrc->msf; }
void i64nul(struct int64 * q1) { q1->hh=0;q1->hl=0;q1->lh=0;q1->ll=0;q1->msf=false; }
void i64shl16(struct int64 * xret) { xret->hh=xret->hl;xret->hl=xret->lh;xret->lh=xret->ll;xret->ll=0;}
int i64cmpS(struct int64 * q1, struct int64 * q2) { for (byte i=0;i<4;i++) {
  word x1=((i==0)?(q1->hh):(i==1?(q1->hl):(i==2?(q1->lh):(i==3?(q1->ll):0))));
  word x2=((i==0)?(q2->hh):(i==1?(q2->hl):(i==2?(q2->lh):(i==3?(q2->ll):0))));
  signed long s1=(signed long)x1; signed long s2=(signed long)x2;
  if (q1->msf) s1=-s1;if (q2->msf) s2=-s2; if (s1<s2) return -1;else if (s1>s2) return 1; }
return 0; }

void i64addS(struct int64 * xret, struct int64 * q1, struct int64 * q2) {
i64nul(xret);signed long xx; if ( ((!q1->msf) && (!q2->msf)) || ((q1->msf) && (q2->msf)) ) {
  xx=(signed long)q1->ll+(signed long)q2->ll;xret->ll=(word)(xx&0xFFFF);
  xx=(signed long)q1->lh+(signed long)q2->lh+(signed long)(xx>>16);xret->lh=(word)(xx&0xFFFF);
  xx=(signed long)q1->hl+(signed long)q2->hl+(signed long)(xx>>16);xret->hl=(word)(xx&0xFFFF);
  xx=(signed long)q1->hh+(signed long)q2->hh+(signed long)(xx>>16);xret->hh=(word)(xx&0xFFFF);
  xret->msf=((q1->msf) && (q2->msf)); }
else if ((q1->msf) && (!q2->msf)) { q1->msf=false; i64subS(xret, q2, q1); }
else if ((!q1->msf) && (q2->msf)) { q2->msf=false; i64subS(xret, q1, q2); }
}

void i64subS(struct int64 * xret, struct int64 * q1, struct int64 * q2) { 
i64nul(xret); if ((!q1->msf) && (q2->msf)) { q2->msf=false; i64addS(xret, q1, q2); } else
if ((q1->msf) && (!q2->msf)) { q1->msf=false; i64addS(xret, q1, q2); xret->msf=true; } else
if ((q1->msf) && (q2->msf)) { q1->msf=false;q2->msf=false; i64subS(xret, q2, q1); } else 
if ((!q1->msf) && (!q2->msf)) { int xcmp=i64cmpS(q1,q2);
  if (xcmp==0) return; else if (xcmp==-1) { i64subS(xret, q2, q1); xret->msf=true; }
  else { signed long xx,xh,xl;xx=(signed long)q1->ll-(signed long)q2->ll;
    xh=(xx>=0)?0:1;xret->ll=(word)xx;xx=(signed long)q1->lh-(signed long)q2->lh-xh;
    xh=(xx>=0)?0:1;xret->lh=(word)xx;xx=(signed long)q1->hl-(signed long)q2->hl-xh;
    xh=(xx>=0)?0:1;xret->hl=(word)xx;xx=(signed long)q1->hh-(signed long)q2->hh-xh;
    xret->hh=(word)xx; } }
}

boolean i64divW(struct int64 * xret, word * xmod, struct int64 * q1, word q2) {
i64nul(xret);*xmod=0; if (q2==0) return false;
else { unsigned long ql;ql=(unsigned long)q1->hh;xret->ll=ql/q2;*xmod=ql%q2;i64shl16(xret);
  ql=(((unsigned long)(*xmod))<<16)+(unsigned long)q1->hl;xret->ll=ql/q2;*xmod=ql%q2;i64shl16(xret);
  ql=(((unsigned long)(*xmod))<<16)+(unsigned long)q1->lh;xret->ll=ql/q2;*xmod=ql%q2;i64shl16(xret);
  ql=(((unsigned long)(*xmod))<<16)+(unsigned long)q1->ll;xret->ll=ql/q2;*xmod=ql%q2;
  return true; } 
}

unsigned long i64lo(struct int64 * xint) { return (((unsigned long)xint->lh)<<16)+((unsigned long)xint->ll);}
unsigned long i64hi(struct int64 * xint) { return (((unsigned long)xint->hh)<<16)+((unsigned long)xint->hl);}

void i64mul64(struct int64 * xretH, struct int64 * xretL, struct int64 * q1, struct int64 * q2) 
{ i64nul(xretH);i64nul(xretL);unsigned long q1l,q1h,q2l,q2h;
q1l=i64lo(q1);q1h=i64hi(q1);q2l=i64lo(q2);q2h=i64hi(q2);int64 xr1,xr2,xr3,xr4;
i64mul(&xr4, q1h, q2h);i64mul(&xr3, q1h, q2l);i64mul(&xr2, q1l, q2h); i64mul(&xr1, q1l, q2l); 
xretL->ll=xr1.ll;xretL->lh=xr1.lh;unsigned long xr2h=i64hi(&xr2);unsigned long xr3h=i64hi(&xr3);
xr2.hh=0;xr2.hl=0;xr3.hh=0;xr3.hl=0;int64 xr1x;i64nul(&xr1x);
xr1x.lh=xr1.hh;xr1x.ll=xr1.hl;int64 xr23sumx;i64addS(&xr23sumx,&xr2,&xr3);int64 xr23sumL;
i64addS(&xr23sumL,&xr23sumx,&xr1x);xretL->hl=xr23sumL.ll;xretL->hh=xr23sumL.lh;
int64 xr23o;i64nul(&xr23o);xr2.lh=(word)(xr2h>>16);xr2.ll=(word)(xr2h&0xFFFF);
xr3.lh=(word)(xr3h>>16);xr3.ll=(word)(xr3h&0xFFFF);xr23o.ll=xr23sumL.hl;xr23o.lh=xr23sumL.hh;
i64addS(&xr1,&xr4,&xr2);i64addS(&xr4,&xr1,&xr3);i64addS(xretH,&xr4,&xr23o);
}

void i64mul(struct int64 * xret, unsigned long q1, unsigned long q2) { 
unsigned long x1,x2,qp,qp1;i64nul(xret);
word all=(word)q1;word alh=(word)(q1>>16);word bll=(word)q2;word blh=(word)(q2>>16);
x1=(unsigned long)all*(unsigned long)bll;xret->ll=(word)(x1);qp=(x1>>16);
x1=(unsigned long)alh*(unsigned long)bll;qp1=(x1>>16);x1&=0xFFFF;
x2=(unsigned long)all*(unsigned long)blh;qp1+=(x2>>16);x2&=0xFFFF;x1+=x2+qp;qp=(x1>>16)+qp1;
xret->lh=(word)(x1);x1=((unsigned long)alh*(unsigned long)blh);
x1+=qp;xret->hl=(word)(x1);xret->hh=(word)(x1>>16);
}

void i64int10k2x10k(signed long * xval, int * x10k, struct int64 * xint64) {
boolean msf=xint64->msf;xint64->msf=false;int64 qret;i64nul(&qret);
word xmod=0;i64divW(&qret, &xmod, xint64, 10000);*x10k=xmod;
signed long qval=(((unsigned long)qret.lh)<<16)+(unsigned long)qret.ll;
*xval=msf?(-qval):qval; if (msf) *x10k=-(*x10k); // minus bugfix
xint64->msf=msf;
}

void i64x10k2int10k(struct int64 * xret, signed long xval, int x10k) {
int64 q1;int64 q2;i64nul(xret);i64nul(&q1);i64nul(&q2);
/* boolean msf=((xval)<0); */ boolean msf=isnegative10k(xval,x10k); // minus bugfix
if (msf) xval=-(xval);if (x10k<0) x10k=-(x10k);
q2.ll=(x10k)&0xFFFF;i64mul(&q1, (unsigned long)(xval), 10000UL);
i64addS(xret, &q1, &q2);xret->msf=msf;
}

void i64x10k2int10kH(struct int64 * xret, signed long xval, int x10k) {
int64 q1;int64 q2;i64nul(xret);i64nul(&q1);i64nul(&q2);
/* boolean msf=((xval)<0); */ boolean msf=isnegative10k(xval,x10k); // minus bugfix
if (msf) xval=-(xval);if (x10k<0) x10k=-(x10k);
q2.ll=(x10k/100)&0xFFFF;i64mul(&q1, (unsigned long)(xval), 100UL);
i64addS(xret, &q1, &q2);xret->msf=msf;
}


// ---[ Digital Pin Output ]----------------------------------------

int DPIN_epGetConfNameAddr(byte xpinindex) { 
return (xpinindex<16)?(2048+(xpinindex*20)):-1;
}

void DPIN_epWriteConfName(byte xpinindex,char * xnamebuf ) {
int xaddr=DPIN_epGetConfNameAddr(xpinindex);
if (xaddr>0) 
  {
  byte chsum=(xaddr&0xFF)+xpinindex;
  for (int i=1;i<20;i++) {
    byte ch=xnamebuf[i-1]; 
    chsum+=ch+i+(ch&(0x0F<<2));
    if (EEPROM.read(xaddr+i)!=ch) EEPROM.write(xaddr+i,ch);    
    if (ch==0) break;
    }
  if (EEPROM.read(xaddr)!=chsum) EEPROM.write(xaddr,chsum);    
  }
}

void DPIN_epReadConfName(byte xpinindex,char * xnamebuf, int xbufsize ) {
char xbuf[20]="";
int xaddr=DPIN_epGetConfNameAddr(xpinindex);
byte chsum=(xaddr&0xFF)+xpinindex;
if (xaddr>0) 
  {
  for (int i=1;i<20;i++) {
    byte ch=EEPROM.read(xaddr+i);
    chsum+=ch+i+(ch&(0x0F<<2));
    if (ch==0) break;
    xbuf[i-1]=ch;
    xbuf[i]=0;
    }
  if (EEPROM.read(xaddr)==chsum)
    {
    strncpy(xnamebuf,xbuf,xbufsize-1);
    xnamebuf[xbufsize-1]=0;
    }
  else
    xnamebuf[0]=0;
  }
}

void DPINUpdateHW() {
uint16_t dstat=DPINstatus^DPINlogicmask;
for (int i=0;i<16;i++) { byte xdpin=DPINmap[i];
  if ((xdpin>=22) && (xdpin<=49)) digitalWrite(xdpin, ((dstat&(1<<i))==0)?LOW:HIGH);
  }
}

void DPINResetHW() {
for (int i=0;i<16;i++) { byte xdpin=DPINmap[i];
  if ((xdpin>=22) && (xdpin<=49)) { pinMode(xdpin, OUTPUT); digitalWrite(xdpin, LOW); }
  }
DPINUpdateHW();
}
