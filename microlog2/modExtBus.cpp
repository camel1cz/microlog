#include <Arduino.h> //needed for Serial.println
#include <string.h> //needed for memcpy
#include <OneWire.h>
#include <DallasTemperature.h>
#include "modExtBus.h"

// definice sběrnic
struct tExtBus *xExtBus;
byte xExtBusCount;
// definice datových hodnot
struct tExtBusValue *xExtBusValue;
byte xExtBusValCount;

unsigned long laststamp;

// cidlo
dht11 DHT11;
//OneWire **ds;

// nastavi datove struktury
void modExtBusSetup() {
  laststamp = millis();
  // naalokuje struktury podle modExtBusPINmap a modExtBusPINproto
  // 1) zjistime pocet sbernic a naalokujeme struktury
  xExtBusCount = 0;
  while((modExtBusPINproto[xExtBusCount] == 1) || (modExtBusPINproto[xExtBusCount] == 2)) xExtBusCount++;
  // alokace
  xExtBus = (tExtBus *)operator new(sizeof(tExtBus)*xExtBusCount);

  // 2) projdeme sbernice a inicialuzujeme struktury
  // pocet hodnot, ktere cteme ze sbernice
  xExtBusValCount = 0;
  for(byte i=0;i<xExtBusCount;i++) {
    // pin a protokol z konfigurace
    xExtBus[i].pin      =  modExtBusPINmap[i];
    xExtBus[i].protocol =  modExtBusPINproto[i];

    switch(modExtBusPINproto[i]) {
      case modExtBusDHT11: {
        // DHT11 ma dve hodnoty
        xExtBusValCount += 2;
        xExtBus[i].valCnt = 2;
        break;
      }
      case modExtBus1Wire: {
        // scan 1Wire sbernice
        OneWire *oneWire = new OneWire(xExtBus[i].pin);
        DallasTemperature *dt = new DallasTemperature(oneWire);
        // Start up the library
        dt->begin();
        dt->setResolution(12);
        xExtBus[i].obj = (void *)dt;
        xExtBus[i].valCnt = dt->getDeviceCount();
        xExtBusValCount += xExtBus[i].valCnt;
        break;
      }
    }
  }
  
  // 3) oscanujeme sbernice a naalokujeme struktury pro hodnoty
  modExtBusScan();
}

// odesle/ulozi data
void modExtBusFlush() {
}

// nacte data ze senzoru
void modExtBusRead() {
  for(byte i=0;i<xExtBusValCount;i++) {
    if((xExtBusValue[i].timestamp == 0) || (millis() < laststamp) || (millis() > (xExtBusValue[i].refresh*1000 + xExtBusValue[i].timestamp))) {
      // cti
      laststamp = millis();
      
      switch(xExtBus[xExtBusValue[i].bus].protocol) {
        case modExtBusDHT11: {
          int chk = DHT11.read(xExtBus[xExtBusValue[i].bus].pin);
          if(chk == DHTLIB_OK) {
            switch(xExtBusValue[i].valOffset) {
              case modExtBusDHT11Tmp:
                xExtBusValue[i].cur = DHT11.temperature;
              break;
              case modExtBusDHT11Hum:
                xExtBusValue[i].cur = DHT11.humidity;
              break;
            }
            xExtBusValue[i].timestamp = laststamp;
            xExtBusValue[i].changed = 1;
          } else {
            XLog("DHT11: read ERR");
          }
          break;
        }
        case modExtBus1Wire: {
          DallasTemperature *dt = (DallasTemperature *)xExtBus[xExtBusValue[i].bus].obj;

          dt->requestTemperatures();

          float temp = dt->getTempC(xExtBusValue[i].id);
          if(temp != DEVICE_DISCONNECTED_C) {
            xExtBusValue[i].cur = temp;
            xExtBusValue[i].timestamp = laststamp;
            xExtBusValue[i].changed = 1;
            break;
          } else {
            XLog("1Wire: read ERR");
          }
          break;
        }
      }
    }
  }
}

// funkce prohleda sbernici a naalokuje struktury pro hodnoty
// zatim alokuje vždy znovu a neuvolnuje data / pocita se s volanim jen pri startu
void modExtBusScan() {
  //xExtBusValue = (tExtBusValue *)operator new(sizeof(tExtBusValue) * xExtBusValCount);
  xExtBusValue = new tExtBusValue[xExtBusValCount];
  byte pos = 0; // pozice v poli hodnot
  for(byte i=0;i<xExtBusCount;i++) {
    switch(xExtBus[i].protocol) {
      case modExtBusDHT11: {
        // teplota
        xExtBusValue[pos].bus = i;
        xExtBusValue[pos].valOffset = modExtBusDHT11Tmp; // teplota
        xExtBusValue[pos].cur = 0;
        xExtBusValue[pos].timestamp = 0;
        xExtBusValue[pos].refresh = modExtBusDHT11TmpRefresh;
        pos++;
        // vlhkost
        xExtBusValue[pos].bus = i;
        xExtBusValue[pos].valOffset = modExtBusDHT11Hum; // vlhkost
        xExtBusValue[pos].cur = 0;
        xExtBusValue[pos].timestamp = 0;
        xExtBusValue[pos].refresh = modExtBusDHT11HumRefresh;
        pos++;
        break;
      }
      case modExtBus1Wire: {
        DallasTemperature *dt = (DallasTemperature *)xExtBus[i].obj;
        DeviceAddress dsAddress={0};
        for(byte j=0;j<xExtBus[i].valCnt;j++) {
          if(dt->getAddress(dsAddress, j)) {
            xExtBusValue[pos].bus = i;
            xExtBusValue[pos].valOffset = j;
            memcpy(xExtBusValue[pos].id, dsAddress, 8);
            xExtBusValue[pos].type = xExtBusValue[pos].id[0];
            xExtBusValue[pos].cur = 0;
            xExtBusValue[pos].timestamp = 0;
            xExtBusValue[pos].refresh = modExtBus1WireTmpRefresh;
            pos++;
          }
        }
        break;
      }
    }
  }
  return;
}

char *ftoa(char *a, double f, int precision)
{
  long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
  
  char *ret = a;
  long heiltal = (long)f;
  while (*a != '\0') a++;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  return ret;
}


