# modul extbus

Rozšíření jsem pojal formou modulu extbus, který již podle názvu zajišťuje komunikaci s externími sběrnicemi.
Funguje to tak, že v konfiguraci vyberete, který digitální pin arduina chcete použít a jaká směrnice na něm sídlí - o zbytek se již postaráno automaticky.

## příklad

Chcete použít čidlo DHT11 pro měření venkovní teploty a dále několik 1wire senzorů pro měření teploty nějaké technologie uvnitř technické místnosti.

V souboru microlog2.ino

Povolte modul extbus:
```
 #define modExtBus          1    // 1=enabled
```

Nakonfigurujte sběrnice:
```
 // null terminated array of protocols used
 byte modExtBusPINproto[] = {modExtBus1Wire, modExtBusDHT11, 0};
 // pins for corresponding protocols above
 byte modExtBusPINmap[] = {3,2};

```

V proměnné modExtBusPINproto je nastaveno, že první sběrnice je typu 1wire, druhá je DHT11. Pozor! Nula na konci je nutná a nechte ji tam!
V proměnné modExtBusPINmap říkáte, že první sběrnice sídlí na digitálním pinu 3 (jak již víme z předchozího řádku, je to 1wire) a druhá sběrnice je na pinu 2 (jde o DHT11 čidlo).

## reporting dat

aktuálně lze pouze přečíst naměřené hodnoty na hlavní stránce webového rozhraní micrologu a nebo je vyčíst externím dotazem na [webové služby](webservices.md) a tyto hodnoty jsou také posílány na server. Hlavní server mypower.cz ale jejich zaznamenávání nepodporuje.
Já jsem to vyřešil tak, že loguju do vlastního serveru - je to velice jednoduché a příklad naleznete v adresáři www
