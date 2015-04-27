#ifndef _MODEXTBUS_H
#define _MODEXTBUS_H

#include <Arduino.h> //needed for Serial.println
#include <string.h> //needed for memcpy
#include <dht11.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/*** DHT11 ***/
#define modExtBusDHT11    1
// DHT11 teplota
#define modExtBusDHT11Tmp 1
// DHT11 vlhkost
#define modExtBusDHT11Hum 2
// refresh teploty 5 minut
#define modExtBusDHT11TmpRefresh  300
// refresh vlhkosti 5 minut
#define modExtBusDHT11HumRefresh  300


// 1 wire
#define modExtBus1Wire    2
// refresh teploty 1Wire 30s
#define modExtBus1WireTmpRefresh  30

// popis jednotlivých sběrnic/pinů
typedef struct tExtBus {
 byte pin; // číslo PINu a zároveň ID sběrnice pro uLog
 byte protocol; // protokol, kterým povídáme po tom pinu - aktuálně jen DHT/1wire - nechal bych prostor klidně byte
 void *obj; // objekt ovladajici sbernici
 byte valCnt;  // pocet hodnot
};

// popis vyčítaných hodnot
typedef struct tExtBusValue {
 byte bus; // ID sběrnice na které sedí (index v xExtBus)
 byte type; // typ zařízení (u 1wire prvních 8 bitů ID)
 byte id[8]; // ID zařízení pro sběrnicové systémy jako 1wire, 64bitů  (1wire adresa)
 byte valOffset; // pořadové číslo hodnoty (v případě více hodnot na jedné adrese/pinu), 4 bity?
 //values[64]; // historie hodnot, každá 16 bitů (kvůli ADC)
 //val; min; max; avg; // předpočítané hodnoty (jestli to má smysl, jinak počítat online)
 float cur; // aktualni hodnota
 unsigned long timestamp; // poslední aktualizace
 unsigned long refresh; // požadovaná rychlost samplování (předpokládá se vysoká hodnota... velké vteřiny/minuty) + hodnota 0 na požádání (to pro třeba ty RAM/ROM, kde nechceme číst periodicky)
 byte changed; // nactena nova hodnota?
};

// definice sběrnic
extern tExtBus *xExtBus;
// pocet sběrnic
extern byte xExtBusCount;
// definice datových hodnot
extern tExtBusValue *xExtBusValue;
// počet čtených datových hodnot
extern byte xExtBusValCount;

// mapovani pinu sbernic
extern byte modExtBusPINmap[];
// protokoly sbernic
extern byte modExtBusPINproto[];

// detekce timestamp wraparound
extern unsigned long laststamp;

// cidlo
extern dht11 DHT11;

// API
// nastavi datove struktury
void modExtBusSetup();

// odesle/ulozi data
void modExtBusFlush();

// nacte data ze senzoru
void modExtBusRead();

// prohleda sbernice a prida nova cidla, stara necha
void modExtBusScan();

// logovani
extern void XLog(char *xmsg);

// vypis floatu
extern char *ftoa(char *a, double f, int precision);

#endif


