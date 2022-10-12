//TODO: cannot access wifi page from home


//================================================
// CONF
//================================================ 
#define OTA_VERSION 1
#define REVISION 0
#define ENV "PV-EHE"
  
#define CONF_MQTT "192.168.0.31"
#define CONF_MQTT_TOPIC "pv"

#define MB_INTERVAL 60000 //Modbus polling interval

//===========================================================================
// config (Preferences)
//===========================================================================
#include <Preferences.h>
typedef struct {
  String ssid;
  String pw;
  String mqtt_server;
  uint16_t mqtt_port;
  String mqtt_topic;  
} config_t;
config_t config;
#define CONF_NAME "qqq"

void config_setup() {
  Preferences prefs;
  prefs.begin(CONF_NAME, true); //readonly
  config.ssid        = prefs.getString("ssid", "");
  config.pw          = prefs.getString("pw", "");  
  config.mqtt_server = prefs.getString("mqtt_server", CONF_MQTT);
  config.mqtt_port   = prefs.getUShort("mqtt_port",   1883);
  config.mqtt_topic  = prefs.getString("mqtt_topic", CONF_MQTT_TOPIC);
  prefs.end();  
}

void config_save() {
  Preferences prefs;
  prefs.begin(CONF_NAME, false); //readwrite
  prefs.putString("ssid",        config.ssid);
  prefs.putString("pw",          config.pw);  
  prefs.putString("mqtt_server", config.mqtt_server);
  prefs.putUShort("mqtt_port",   config.mqtt_port);
  prefs.putString("mqtt_topic",  config.mqtt_topic);
  prefs.end();
}



//===========================================================================
// WiFi Manager (captive portal)
// set HTTP_MAX_DATA_WAIT 500 in C:\Users\[USERNAME]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\WebServer\src\WebServer.h
// otherwise 5 second delay between connecting to captive portal AP and display or web page
//===========================================================================
#define WM_CONNECT_TIMEOUT  20 //seconds to wait on boot to establish wifi connection
#define WM_PORTAL_TIMEOUT  300 //seconds to keep captive portal alive before rebooting
#define WM_DROPPED_TIMEOUT 300 //seconds to wait for dropped wifi reconnection before rebooting
//XXX #include <WiFi.h>

#include <DNSServer.h>
IPAddress wm_apIP(192, 168, 1, 1);
DNSServer wm_dnsServer;

#include <WebServer.h>
WebServer wm_webserver(80);

bool wm_saved = false;
String wm_hostname = "";
String wm_ssid = "";
String wm_pw = "";

//(re)connect wifi, returns true on success
//blocks for up to WM_CONNECT_TIMEOUT seconds
bool wm_connect() {
  //start wifi
  WiFi.begin(wm_ssid.c_str(), wm_pw.c_str());
  if(wm_hostname!="") WiFi.setHostname(wm_hostname.c_str());

  //wait for connect
  if(wm_ssid=="") {
    Serial.println("wm_connect: SSID is empty");
    return false;
  }else{
    Serial.print("wm_connect: Connecting to " + wm_ssid + " ");
    int timeout = WM_CONNECT_TIMEOUT;
    while (timeout && WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
      timeout--;      
    }
    if(WiFi.status() == WL_CONNECTED) {
      Serial.print(" ");
      Serial.print(WiFi.localIP());
      Serial.println(" CONNECTED");
      return true;
    }else{
      Serial.println(" FAILED");
      return false;
    }
  }   
}

//returns true if connected to ssid
//returns false with newly saved ssid + pw
//blocks (does not return/reboots) if can't connect to ssid
bool wm_setup(String &ssid, String &pw, String portal_ssid, String portal_pw) {
  wm_hostname = portal_ssid;
  wm_ssid = ssid;
  wm_pw = pw;

  //start wifi, exit on success
  if(wm_connect()) return true;
    
  //================================
  //captive portal
//XXX  led_set_status(LED_STATUS_CAPTIVE); //TODO add event, remove led_set_status from here
  Serial.println("Starting captive portal ...");
  
  //wifi 
  //persistent    - default true
  //autoConnect   - default true [depreciated]
  //autoReconnect - default true    
  WiFi.persistent(false); //needed?? - default true
  WiFi.setAutoConnect(false); //needed??  AutoConnect=true station mode will connect to AP automatically when it is powered on.
  WiFi.mode(WIFI_AP); 
  delay(2000); //needed, otherwise crash on client connect
  WiFi.softAP(portal_ssid.c_str(),portal_pw.c_str());
  WiFi.softAPConfig(wm_apIP, wm_apIP, IPAddress(255, 255, 255, 0));
  Serial.print(" - AP started, IP address: ");
  Serial.println(WiFi.softAPIP());

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  wm_dnsServer.setErrorReplyCode(DNSReplyCode::NoError); //needed??
  wm_dnsServer.start(53, "*", wm_apIP);
  Serial.println(" - DNS server started");

  //setup webserver
  wm_webserver.onNotFound(wm_handleNotFound);
  wm_webserver.begin();
  Serial.println(" - Webserver started");
  Serial.println("Captive portal started");

  uint32_t portal_millis = millis();
  while(millis() - portal_millis <= ((WM_PORTAL_TIMEOUT)*1000)) {
    if(WiFi.softAPgetStationNum() > 0) portal_millis = millis();
    wm_dnsServer.processNextRequest();
    wm_webserver.handleClient();
    if(wm_saved) {
      ssid = wm_ssid;
      pw = wm_pw;
      return false; 
    }
  }
  wm_reboot("captive portal timeout");
  return true; //keep compiler happy (does not get here)
}

void wm_loop() {
  static uint32_t dropped_millis;
  if(WiFi.status() == WL_CONNECTED) {
    //connected
//XXX    if(led_get_status()==LED_STATUS_BOOT) led_set_status(LED_STATUS_OK);
    dropped_millis = millis();
    return;
  }else{
    //connection lost
//XXX    led_set_status(LED_STATUS_BOOT); 
    wm_connect();
    if(millis() - dropped_millis > ((WM_DROPPED_TIMEOUT)*1000)){
      wm_reboot("Connection dropped");
    }
  }
}

String wm_scan()
{
  String s = "";
  
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  if (n == 0) {
    s += "no networks found";
  } else {
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      s += "<button onclick=\"upd('" + WiFi.SSID(i) + "')\">" 
      //+ String(i + 1) + ": " 
      + WiFi.SSID(i) + " [" 
      + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open " : "") 
      + String(WiFi.RSSI(i)) + "dB"
      + "]</button><br />";
      delay(10);
    }
    s += "<script>function upd(s){document.getElementById('ssid').value=s;var pw=document.getElementById('pw');pw.value='';pw.focus();}</script>";
  }
  return s;
}

void wm_handleNotFound() {  
  String m = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><style>input{padding:5px;font-size:1em;width:95%;} body{text-align:center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:95%;}</style>";
  String ssid = wm_webserver.arg("ssid");
  String pw = wm_webserver.arg("pw");
  String action = wm_webserver.arg("a");

  //handle actions
  if(action == "reboot") {
    wm_webserver.send(200, "text/html", m + "Reconnecting, please wait ...");
    wm_reboot("Portal request");
  }  
  if(ssid != "") {
    wm_ssid = ssid;
    wm_pw = pw;
    wm_saved = true;
    wm_webserver.send(200, "text/html", m + "Settings Saved - Rebooting");
    return;
  }

  //return web page
  if(wm_ssid != "") m += "Current network is <b>" + wm_ssid + "</b> - <a href=\"/?a=reboot\">Reconnect</a>";
  m += "<h1>WiFi Setup</h1>";
  m += "<form>Network<br /><input id=\"ssid\" name=\"ssid\"><br />Password<br /><input id=\"pw\" name=\"pw\" type=\"password\"><br /><button type=\"submit\">Save</button></form>";
  m += "<h1>Available Networks</h1><a href=\"/\">Refresh</a><br />" + wm_scan();
  m += "<h1>Gateway Info</h1>";
  m += "<pre>" + gw_top() + "</pre>";
  m += "</body></html>";
  wm_webserver.send(200, "text/html", m);
}

void wm_reboot(String msg) {
  Serial.println(msg + " --> rebooting...");
  Serial.flush();
  yield();
  delay(1000); 
  ESP.restart();
  while(1) {delay(1000); yield();}  
}


//===========================================================================
// MQTT
//===========================================================================
long mqtt_attempt_cnt = 0; //number of connect attempts
long mqtt_connect_cnt = 0; //number of connects
long mqtt_subscribe_cnt = 0; //number of subs
long mqtt_publish_cnt = 0; //number of pubs
long mqtt_publish_fail_cnt = 0; //number of failed pubs

#include <PubSubClient.h>
WiFiClient mqtt_WiFiClient;
PubSubClient mqtt_Client(mqtt_WiFiClient);

void mqtt_connect() {
  static uint32_t lastReconnectAttempt = -999999;
  uint32_t now = millis();
  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;
    mqtt_attempt_cnt++;
    String clientid = "QLgw-" + String(random(0xffff), HEX) + String(random(0xffff), HEX);
    //Serial.print(clientid);
    Serial.print("MQTT Connecting to " + config.mqtt_server + ":" + String(config.mqtt_port) + " ... ");
    if (mqtt_Client.connect(clientid.c_str())) {
      mqtt_connect_cnt++;
      mqtt_Client.subscribe(gw_mac().c_str());
      Serial.println("OK");
      //set to reconnect in 1 second if connection is lost        
      lastReconnectAttempt = now - 4000; 
    }else{
      Serial.println("FAILED");      
    }
  }
}

bool mqtt_publish(String topic, String msg) {
  mqtt_publish_cnt++;
  //bool ok = mqtt_Client.publish(topic.c_str(),msg.c_str(),true); //retained
  bool ok = mqtt_Client.publish(topic.c_str(),msg.c_str());
  if(!ok) mqtt_publish_fail_cnt++;
  return ok;
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  mqtt_subscribe_cnt++;
  Serial.print("MQTT Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqtt_setup() {
  mqtt_Client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
  mqtt_Client.setCallback(mqtt_callback);
  mqtt_Client.setBufferSize(1024);
}

void mqtt_loop() {
  if (!mqtt_Client.connected()) {
    mqtt_connect();  
  }else{
    mqtt_Client.loop();
  }
}

//===========================================================================
// ntp time
//===========================================================================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

#include "time.h"
void time_setup() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String time_local()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return String("Failed to obtain time");
  }
  char buf[100];
  //strftime(buf,sizeof(buf),"%A, %B %d %Y %H:%M:%S",&timeinfo);
  strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&timeinfo);
  return String(buf);
}

String time_gmt()
{
  time_t now;
  struct tm timeinfo;
  time(&now);
  gmtime_r(&now, &timeinfo);
  char buf[100];
  //strftime(buf,sizeof(buf),"%A, %B %d %Y %H:%M:%S",&timeinfo);
  strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&timeinfo);
  return String(buf);
}


//===========================================================================
// GW gateway functions
//===========================================================================
String gw_status = "";

String gw_version() {
  char buf[20];
  snprintf(buf,sizeof(buf),ENV "-%d-%d", OTA_VERSION, REVISION);
  return String(buf);
}

String gw_mac() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[20];
  snprintf(buf,sizeof(buf),"%02X%02X%02X%02X%02X%02X", (int)((mac>>0)&0xFF), (int)((mac>>8)&0xFF), (int)((mac>>16)&0xFF), (int)((mac>>24)&0xFF), (int)((mac>>32)&0xFF), (int)((mac>>40)&0xFF));
  return String(buf);
}

//millis64 keeps track of time if called at least once every 49 days
uint64_t gw_millis64() {
    static uint32_t low32, high32;
    uint32_t new_low32 = millis();
    if (new_low32 < low32) high32++;
    low32 = new_low32;
    return (uint64_t) high32 << 32 | low32;
}

//human readable uptime
String gw_uptime() {
    uint32_t sec = gw_millis64()/1000;
    char buf[100];
    snprintf(buf,sizeof(buf),"%d days %d:%02d:%02d",sec/86400, (sec/3600)%24, (sec/60)%60, sec%60); 
    return String(buf); 
}

String gw_top() {
    uint64_t mac      = ESP.getEfuseMac();
    uint32_t heapsize = ESP.getHeapSize();
    uint32_t freeheap = ESP.getFreeHeap();
    uint32_t minheap  = ESP.getMinFreeHeap();
    uint32_t sketsize = ESP.getSketchSize();
    uint32_t freesket = ESP.getFreeSketchSpace();
    char buf[100];
    String s;

    //current time
    s = "local:" + time_local() + " GMT:" + time_gmt() + "\n";  
    //ESP + network stats 
    snprintf(buf,sizeof(buf),"UP: %s  Version: %s\n", gw_uptime().c_str(), gw_version().c_str());
    s += buf;
    snprintf(buf,sizeof(buf),"IP: %s  SSID: %s  RSSI: %ddB  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI(), (int)((mac>>0)&0xFF), (int)((mac>>8)&0xFF), (int)((mac>>16)&0xFF), (int)((mac>>24)&0xFF), (int)((mac>>32)&0xFF), (int)((mac>>40)&0xFF));
    s += buf;
    snprintf(buf,sizeof(buf),"Mem:%8d total,%8d %2d%% free,%8d %2d%% used,%8d %2d%% min free\n",heapsize+freeheap,freeheap,100*freeheap/(heapsize+freeheap),heapsize,100*heapsize/(heapsize+freeheap),minheap,100*minheap/(heapsize+freeheap));
    s += buf;
    snprintf(buf,sizeof(buf),"Prg:%8d total,%8d %2d%% free,%8d %2d%% used\n",sketsize+freesket,freesket,100*freesket/(sketsize+freesket),sketsize,100*sketsize/(sketsize+freesket)); 
    s += buf;
    snprintf(buf,sizeof(buf),"CPU: rev%d %dMHz, Hall: %d\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), hallRead());
    s += buf;
    
    //MQTT stats
    snprintf(buf,sizeof(buf),"MQTT connect:   %ld total, %ld failed\n", mqtt_attempt_cnt, mqtt_attempt_cnt-mqtt_connect_cnt); 
    s += buf;
    snprintf(buf,sizeof(buf),"MQTT subscribe: %ld received\n", mqtt_subscribe_cnt); 
    s += buf;
    snprintf(buf,sizeof(buf),"MQTT publish:   %ld total, %ld failed\n", mqtt_publish_cnt, mqtt_publish_fail_cnt); 
    s += buf;

    //GW stats
    s += gw_status;
 
    return s;
}

//===========================================================================
// webserver
//===========================================================================
#include <WebServer.h>
WebServer web_server(80);
void web_PageRoot() {
  String msg;
  msg += "<h1>Sensor Gateway " + gw_mac().substring(8) + "</h1>";
  msg += "<pre>uptime: ";
  msg += millis();
  msg += "\n\n" + gw_top() + "</pre>";
  msg += "<a href=\"/_ac\">Wifi Config</a> | ";
  msg += "<a href=\"/setup\">Server Setup</a>";
  web_server.send(200, "text/html", msg);
}

void web_handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += web_server.uri();
  message += "\nMethod: ";
  message += (web_server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += web_server.args();
  message += "\n";
  for (uint8_t i = 0; i < web_server.args(); i++) {
    message += " " + web_server.argName(i) + ": " + web_server.arg(i) + "\n";
  }
  web_server.send(404, "text/plain", message);
}

void web_PageSetup() {  
  String msg =
"<h1>Setup</h1>"
"<form method=\"POST\">"
"MQTT Server<input name=\"mqtt_server\" value=\"" + config.mqtt_server + "\"><br />"
"MQTT Port<input name=\"mqtt_port\" value=\"" + String(config.mqtt_port) + "\"><br />"
"MQTT Topic<input name=\"mqtt_topic\" value=\"" + String(config.mqtt_topic) + "\"><br />"
"<input type=\"submit\" value=\"Save\">"
"<a href=\"/\">Home</a>";
"</form>";

  web_server.send(200, "text/html", msg);  
}

void web_PageSetupPost() {
//  if(web_server.hasArg("myinflux"))    config.myinflux    = web_server.arg("myinflux");
  if(web_server.hasArg("mqtt_server")) config.mqtt_server = web_server.arg("mqtt_server");
  if(web_server.hasArg("mqtt_port"))   {
    uint16_t port = web_server.arg("mqtt_port").toInt(); 
    if(port>0) config.mqtt_port = port;
  }
  if(web_server.hasArg("mqtt_topic")) config.mqtt_topic = web_server.arg("mqtt_topic");
  if(web_server.args()>0) {
    config_save();
  }

  web_server.sendHeader("Location", "/setup");
  web_server.send(303, "text/html", "");  
}

void web_setup() {
  web_server.on("/", web_PageRoot);

  web_server.on("/setup", HTTP_GET, web_PageSetup);
  web_server.on("/setup", HTTP_POST, web_PageSetupPost);
  web_server.onNotFound(web_handleNotFound);
  web_server.begin();
}

void web_loop() {
  web_server.handleClient();
}



//================================================
// MODBUS
//================================================ 
 
/*
UART  RX IO   TX IO   CTS   RTS
UART0   GPIO3   GPIO1   N/A   N/A
UART1   GPIO9   GPIO10  GPIO6   GPIO11
UART2   GPIO16  GPIO17  GPIO8   GPIO7
 */

#define RXD2 16  //RO
#define TXD2 17  //DI
#define REDE 18  //!RE/DE
#define MB_BAUD 9600
#define TIMEOUT 200 //answer in 32 ms

#define MB_SLAVEID 10

volatile uint32_t mb_ts = 0;

uint16_t mb_addr = 0;
uint8_t mb_addrcnt = 39;

struct mb_struct{
  int idx;
  String lbl;
  float factor; //scale factor for uom
  String uom; //unit of measurement
  int32_t regval; //normal register value, register not reported when this value is returned
};

const mb_struct mb_regs[] = {
  {0,"VerInv",1,"",4044},   //Inverter Version
  {1,"VerLCD",1,"",275},    //LCD Version
  {2,"Val2",1,"",19},       //Unknown - always 19
  {3,"Val3",1,"",-1},       //Unknown
  {4,"null4",1,"",0},
  {5,"null5",1,"",0},
  {6,"Vgrid",1,"V",-1},     //Grid voltage
  {7,"null7",1,"",0},
  {8,"null8",1,"",0},
  {9,"I1in",0.1,"A",-1},    //PV chain 1 current
  {10,"V1in",1,"V",-1},     //PV chain 1 voltage
  {11,"I2in",0.1,"A",-1},   //PV chain 2 current
  {12,"V2in",1,"V",-1},     //PV chain 2 voltage
  {13,"Temp",1,"C",-1},     //Inverter temperature
  {14,"Fgrid",0.01,"Hz",-1},//Grid frequency
  {15,"PoutMSB",1,"",0},    //MSB (?)
  {16,"Pout",0.1,"W",-1},   //Output power
  {17,"P1inMSB",1,"",0},    //MSB (?)
  {18,"P1in",0.1,"W",-1},   //PV chain 1 power
  {19,"P2inMSB",1,"",0},    //MSB (?)
  {20,"P2in",0.1,"W",-1},   //PV chain 2 power
  {21,"null21",1,"",0},
  {22,"null22",1,"",0},
  {23,"null23",1,"",0},
  {24,"null24",1,"",0},
  {25,"null25",1,"",0},
  {26,"null26",1,"",0},
  {27,"EdayMSB",1,"",0},    //MSB (?)
  {28,"Eday",0.1,"kWh",-1}, //Day energy
  {29,"EtotMSB",1,"",0},    //MSB (?)
  {30,"Etot",1,"kWh",-1},   //Total energy
  {31,"Val31",1,"",1},      //Unknown - always 1
  {32,"SN1",1,"",0},        //Serial number MSB (?)
  {33,"SN2",1,"",1555},     //Serial number
  {34,"HrTotMSB",1,"",0},   //MSB (?)
  {35,"HrTot",1,"hr",-1},   //Total hours
  {36,"HrDay",1.0/60.0,"hr",-1}, //Day hours
  {37,"Val37",1,"",-1},     //Unknown
  {38,"Riso",1,"MOhm",20}   //Riso  
};

#include "ModbusRtu.h"

// data array for modbus network sharing
uint16_t mb_data[256];

bool mb_rxwait = false;

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  port : serial port
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */
Modbus master(0,Serial2,REDE); // this is master and RS-232 or USB-FTDI

void mb_setup() {
  Serial2.begin(MB_BAUD, SERIAL_8N1, RXD2, TXD2);

  master.start();
  master.setTimeOut( TIMEOUT ); 

  mb_ts = millis() - MB_INTERVAL;
}

//returns true if transmitted
bool mb_tx() {
  if(mb_rxwait) return false;

  modbus_t telegram;
  telegram.u8id = MB_SLAVEID; // slave address
  telegram.u8fct = 3; // function code (this one is registers read)
  telegram.u16RegAdd = mb_addr; // start address in slave
  telegram.u16CoilsNo = mb_addrcnt; // number of elements (coils or registers) to read
  telegram.au16reg = mb_data; // pointer to a memory array in the Arduino

  master.query( telegram ); // send query (only once)
  mb_rxwait = true;
    
  return true;
}

//returns 0 if no data, negative if communication error, >4 if correct query processed
int8_t mb_rx() {
  if(!mb_rxwait) return 0;
      
  int8_t rv = master.poll(); // check incoming messages
  if (master.getState() == COM_IDLE) {
    mb_rxwait = false;
    return rv;
  }
  return 0;
}

void mb_loop() {
  if(millis() - mb_ts >= MB_INTERVAL) {
    mb_ts += MB_INTERVAL;
    mb_tx();
  }

  int8_t rv = mb_rx();
  if(rv>0) {
    gw_status = "<br><br>Status at " + time_local() + "<br>";
    Serial.print("Time: " + time_local() + "\n");
    String json = "{";
    for(uint8_t i=0;i<mb_addrcnt;i++) {
      if( mb_data[i] != mb_regs[i].regval) {
        float val = mb_data[i] * mb_regs[i].factor;
        char s[80];
        //serial
        Serial.printf("%s: %g %s\n", mb_regs[i].lbl.c_str(), val , mb_regs[i].uom.c_str());
        //json
        snprintf(s, 80, "\"%s\":\"%g\",", mb_regs[i].lbl.c_str(), val);
        json += s;
        //gw_status
        snprintf(s, 80,"%s: %g %s<br>",mb_regs[i].lbl.c_str(), val , mb_regs[i].uom.c_str());
        gw_status += s;
      }
    }
    json += "\"ts\":\"" + time_gmt() + "\"}";
    Serial.println();    

    mqtt_publish(config.mqtt_topic,json);
  }else if(rv<0) {
    char s[80]; 
    snprintf(s, 80,"modbus error %d",rv);
    Serial.println(s);
    //mqtt_publish(config.mqtt_topic, String("{\"log\":\"") + s + "\"}");
  }
}

//================================================
// MAIN
//================================================ 
void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println(gw_mac());
  Serial.println(gw_version());
  delay(1000);
  config_setup();

  if(!wm_setup(config.ssid, config.pw, String("qqqlab") + "-" + gw_mac().substring(8), "12345678")) {
    config_save();
    wm_reboot("Storing new ssid="+config.ssid);
  }

  web_setup();
  time_setup(); //NOTE without WIFI setup this crashes with: ".../components/freertos/queue.c:1442 (xQueueGenericReceive)- assert failed!"
  mqtt_setup();
  
  mb_setup();

  Serial.printf("\nMODBUS %d\n",MB_BAUD);
}

void loop() {
  web_loop();
  mqtt_loop();
  wm_loop();
  mb_loop();
}
