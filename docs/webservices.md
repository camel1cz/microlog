# webové služby modulu extbus

## čtení aktuálních hodnot ze sběrnice

```
http://192.168.1.188/cfg?cmd=6562
```

Výstup:
```
~EXTBUS
V:2:1:0.0
V:2:2:0.0

```

Význam:
- první řádek je identifikátor datového souboru,
- druhý řádek je první datová hodnota, následuje další...

Formát řádku:
```
V:<PIN>:<ID>:<HODNOTA>
```
<PIN> ... pin arduina, ze kterého pochází hodnota,
<ID> ... pořadové ID hodnoty (unikátní v rámci pinu),
<HODNOTA> ... aktuální hodnota.
