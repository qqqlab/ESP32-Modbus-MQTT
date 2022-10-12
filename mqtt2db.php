<?php
//composer php-mqtt/client": "^1.1

/*
CREATE TABLE `sensor` (
 `ts` timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
 `n` varchar(16) COLLATE latin1_general_ci NOT NULL DEFAULT 'temp1',
 `t` float DEFAULT NULL,
 `h` float DEFAULT NULL,
 `p` float DEFAULT NULL,
 `v` float DEFAULT NULL,
 `l` float DEFAULT NULL,
 `Pout` float DEFAULT NULL,
 `Etot` float DEFAULT NULL,
 `HrTot` float DEFAULT NULL,
 `Eday` float DEFAULT NULL,
 `HrDay` float DEFAULT NULL,
 `V1in` float DEFAULT NULL,
 `Val3` float DEFAULT NULL,
 `Val37` float DEFAULT NULL,
 PRIMARY KEY (`ts`,`n`) USING BTREE,
 KEY `ts` (`ts`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci
*/

require('vendor/autoload.php');

use \PhpMqtt\Client\MqttClient;
use \PhpMqtt\Client\ConnectionSettings;

$server   = 'localhost';
$port     = 1883;
$clientId = rand(5, 15);
$username = null;
$password = null;
$clean_session = false;

$zigbee_topic = 'zigbee2mqtt/+';

$connectionSettings  = new ConnectionSettings();
$connectionSettings
  ->setUsername($username)
  ->setPassword($password)
  ->setKeepAliveInterval(60);
/*
  ->setLastWillTopic('emqx/test/last-will')
  ->setLastWillMessage('client disconnect')
  ->setLastWillQualityOfService(1);
 */

$mqtt = new MqttClient($server, $port, $clientId);

$mqtt->connect($connectionSettings, $clean_session);
slog("======CONNECTED to $server   topic: $zigbee_topic ======");

$mqtt->subscribe($zigbee_topic, function($t,$m) {mqtt_process($t,$m);}, 0);

$mqtt->loop(true);




function slog($s) {echo date("Y-m-d H:i:s") . " " . $s . "\n";}

function mqtt_process($topic, $msg){
  $prop = [
'temperature'=>'t',
'linkquality'=>'l',
'voltage'=>'v',
'humidity'=>'h',
'pressure'=>'p',
'Pout'=>'Pout',
'V1in'=>'V1in',
'Eday'=>'Eday',
'Etot'=>'Etot',
'HrTot'=>'HrTot',
'HrDay'=>'HrDay',
'Val3'=>'Val3',
'Val37'=>'Val37'
];

  $db = null;
  try {
    slog("MQTT $topic $msg");

    $out = [];
    $a = json_decode($msg, true); //as array

    foreach($prop as $k=>$v) {
      if(isset($a[$k])) $out[$v] = $a[$k];
    }
    $out['n'] = explode('/',$topic)[1]; //sensor name

    $sql = "REPLACE INTO sensor("
      . implode(',',array_keys($out))
      . ") VALUES ('"
      . implode("','",array_values($out))
      . "')";

    $db = new mysqli("127.0.0.1", "<<MYSQL_USER>>", "<<MYSQL_PASSWD>>", "<<MYSQL_DB>>");
    $res = $db->query($sql);
    $db->close();

    slog("POST res=$res sql=$sql");
    if(!$res) slog("DB_ERR ".$db->error());

  } catch (Exception $e) {
    echo 'ERROR: ',  $e->getMessage(), "\n";
  }
  unset($db);
}
