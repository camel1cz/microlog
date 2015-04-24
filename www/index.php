<?php
// jde o ukázkový kód, tak to tak berte a pouze z něj vycházejte při vlastní
// tvorbě.

// ID vašeho arduina nastavené při kompilaci
$fveid = $_COOKIE['fveid'];

// analogové hodnoty jsou v cookies A0 až A15
$analog = array();
for($i=0;$i<16;$i++) {
  $analog[$i] = $_COOKIE['A'.$i];
}
// hodnoty z externích sběrnic jsou v cookies EXTBUS_<PIN>_<ID>
$extbus = array();
foreach($_COOKIE as $key => $val) {
  if(substr($key, 0, 6) == 'EXTBUS') {
    list($dummy, $pin, $id) = explode('_', $key, 3);
    if(! isset($extbus[$pin])) $extbus[$pin] = array();
    $extbus[$pin][$id] = $val;
  }
}

# takto pochopi arduino, že server data úspěšně přijal a zpracoval
echo 'result:OK' . PHP_EOL;
