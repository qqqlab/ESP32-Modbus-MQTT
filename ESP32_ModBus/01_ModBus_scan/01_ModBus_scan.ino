/**
 *  Modbus master example 1:
 *  The purpose of this example is to query an array of data
 *  from an external Modbus slave device. 
 *  The link media can be USB or RS232.
 *
 *  Recommended Modbus slave: 
 *  diagslave http://www.modbusdriver.com/diagslave.html
 *
 *  In a Linux box, run 
 *  "./diagslave /dev/ttyUSB0 -b 19200 -d 8 -s 1 -p none -m rtu -a 1"
 * 	This is:
 * 		serial port /dev/ttyUSB0 at 19200 baud 8N1
 *		RTU mode and address @1
 */
/*
UART  RX IO   TX IO   CTS   RTS
UART0   GPIO3   GPIO1   N/A   N/A
UART1   GPIO9   GPIO10  GPIO6   GPIO11
UART2   GPIO16  GPIO17  GPIO8   GPIO7
 */

#define RXD2 16  //RO
#define TXD2 17  //DI
#define REDE 18  //!RE/DE
#define BAUD 9600
#define TIMEOUT 200 //answer in 32 ms

uint16_t addr = 0;
uint8_t addrstep = 0;
uint8_t addrcnt = 39;
 
#include "ModbusRtu.h"

// data array for modbus network sharing
uint16_t au16data[256];
uint8_t u8state;

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  port : serial port
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */
Modbus master(0,Serial2,REDE); // this is master and RS-232 or USB-FTDI

/**
 * This is an structe which contains a query to an slave device
 */
modbus_t telegram;

unsigned long u32wait;

void setup() {
  Serial.begin(115200);

  Serial2.begin(BAUD, SERIAL_8N1, RXD2, TXD2);

  
  master.start();
  master.setTimeOut( TIMEOUT ); // if there is no answer in 2000 ms, roll over
  u32wait = millis() + 1000;
  u8state = 0; 

  delay(500);
  Serial.printf("\nMODBUS %d\n",BAUD);
}

volatile uint32_t ts;

const String lbl[39]={
  "VerInv", //0
  "VerLCD", //1
  "=19", //2
  "???", //3
  "0", //4
  "0", //5
  "Vgrid [V]", //6
  "0", //7
  "0", //8  
  "I1in [A/10]", //9
  "V1in [V]", //10
  "I2in [A/10]", //11
  "V2in [V]", //12
  "Temp [C]", //13
  "Fgrid [Hz/100]", //14
  "MSB", //15
  "Pout [W/10]", //16
  "MSB", //17
  "P1in [W/10]", //18 
  "MSB", //19 
  "P2in [W/10]", //20 
  "0", //21 
  "0", //22 
  "0", //23 
  "0", //24 
  "0", //25 
  "0", //26 
  "MSB", //27 
  "Eday [kWh/10]", //28 
  "MSB", //29 
  "Etot [kWh]", //30 
  "???", //31 
  "SN1", //32 
  "SN2", //33 
  "MSB", //34 
  "RunT-T [hr]", //35 
  "RunT-D [min]", //36 
  "???", //37 
  "???" //38   
};

const int32_t regval[39]={
  4044, //"VerInv", //0
  275,  //"VerLCD", //1
  19,   //"=19", //2
  -1,   //"???", //3
  0,    //"0", //4
  0,    //"0", //5
  -1,   //"Vgrid [V]", //6
  0,    //"0", //7
  0,    //"0", //8  
  -1,   //"I1in [A/10]", //9
  -1,   //"V1in [V]", //10
  -1,   //"I2in [A/10]", //11
  -1,   //"V2in [V]", //12
  -1,   //"Temp [C]", //13
  -1,   //"Fgrid [Hz/100]", //14
   0,   //"MSB", //15
  -1,    //"Pout [W/10]", //16
   0,   //"MSB", //17
  -1,    //"P1in [W/10]", //18 
   0,   //"MSB", //19 
  -1,   //"P2in [W/10]", //20 
  0,   //"0", //21 
  0,//"0", //22 
  0,//"0", //23 
  0,//"0", //24 
  0,//"0", //25 
  0,//"0", //26 
  0,   //"MSB", //27 
  -1,  //"Eday [kWh/10]", //28 
  0,  //"MSB", //29 
  -1,  //"Etot [kWh]", //30 
  1,   //"???", //31 
  0,   //"SN1", //32 
  1555,//"SN2", //33 
  0,   //"MSB", //34 
  -1,  //"RunT-T [hr]", //35 
  -1,  //"RunT-D [min]", //36 
  -1,  // "???", //37 
  20   //"Riso [MOhm]" //38   
};

void loop() {
  switch( u8state ) {
  case 0: 
    if (millis() > u32wait) u8state++; // wait state
    break;
  case 1: 
    telegram.u8id = 10; // slave address
    telegram.u8fct = 3; // function code (this one is registers read)
    telegram.u16RegAdd = addr; // start address in slave
    telegram.u16CoilsNo = addrcnt; // number of elements (coils or registers) to read
    telegram.au16reg = au16data; // pointer to a memory array in the Arduino

    master.query( telegram ); // send query (only once)
    ts = millis();
    u8state++;
    //Serial.printf("%04X:",addr);
    break;
  case 2:
    //@return 0 if no query, -1..-4 if communication error, >4 if correct query processed
    int8_t rv = master.poll(); // check incoming messages
    if (master.getState() == COM_IDLE) {
      volatile uint32_t dt = millis() - ts;
      //if(rv>=0) for(uint8_t i=0;i<addrcnt;i++) Serial.printf(" %04X", au16data[i]);
      if(rv>=0) {
        Serial.printf("millis:%ld\n",millis());
        //for(uint8_t i=0;i<addrcnt;i++) Serial.printf(" %6d", au16data[i]);   
        for(uint8_t i=0;i<addrcnt;i++) {
          if( au16data[i] != regval[i]) Serial.printf("%2d %s: %d\n", i, lbl[i].c_str(), au16data[i]);
        }
      }else{
        Serial.printf(" RV%d", rv);
        Serial.printf(" dt=%d", dt);       
      }
      Serial.printf("\n");
      u8state = 0;
      u32wait = millis() + 30000; 
      addr += addrstep;
    }
    break;
  }
}
