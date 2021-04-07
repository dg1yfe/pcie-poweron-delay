#include <stdint.h>
#include <avr/pgmspace.h>

#define PCIE_NSHUTDOWN 8
#define POST_LED1 9
#define POST_LED2 10
#define POST_LED3 11
#define POST_LED4 12

struct timer_S {
  uint32_t tick;
  uint32_t overflow;
};

enum pciState_E{ PCIE_DEACTIVATE, POST, WAIT_DELAY, PCIE_ACTIVATE, WAIT_POST_DONE, RUNNING };
enum tempMeasState_E { TIDLE, TMEAS, TWAIT };


const char pciStr_deact[] PROGMEM = "PCIe Power deactivated";
const char pciStr_post[] PROGMEM = "Waiting for POST to commence";
const char pciStr_delay[] PROGMEM = "Triggered, waiting some more";
const char pciStr_act[] PROGMEM = "Activating PCIe Power";
const char pciStr_postdone[] PROGMEM = "Waiting for POST to be done";
const char pciStr_running[] PROGMEM = "Running...";

enum tempMeasState_E measState;
enum pciState_E pciState;

struct timer_S meas;
struct timer_S milli;
struct timer_S postTimer;
struct timer_S tt;

uint8_t postState;
char pbuf[128];

uint8_t op;
uint8_t override;
uint8_t printPt;


const char * const pciStateStr[] = {
  pciStr_deact,
  pciStr_post,
  pciStr_delay,
  pciStr_act,
  pciStr_postdone,
  pciStr_running
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  pinMode(POST_LED1, INPUT);
  pinMode(POST_LED2, INPUT);
  pinMode(POST_LED3, INPUT);
  pinMode(POST_LED4, INPUT);
  
  pinMode(PCIE_NSHUTDOWN, OUTPUT);
  digitalWrite(PCIE_NSHUTDOWN, 0);

  measState = TIDLE;
  pciState = RUNNING;
  initTimer(&milli);
  initTimer(&meas);
  initTimer(&postTimer);
  initTimer(&tt);
  postState = -1;

  override = 0;
  printPt = 0;
  printPciState();
}


uint8_t readPostState(){
  uint8_t s;
  s  = digitalRead(POST_LED1) ? 0 : 8;
  s |= digitalRead(POST_LED2) ? 0 : 4;
  s |= digitalRead(POST_LED3) ? 0 : 2;
  s |= digitalRead(POST_LED4) ? 0 : 1;
  if(override){
    s = op;
  }

  return s;
}


uint8_t checkExpired(struct timer_S * chk){
  if(chk->overflow <= milli.overflow){
    if(chk->tick <= milli.tick){
      return 1;
    }
  }
  return 0;
}


void initTimer(struct timer_S * tmr){
  tmr->tick = tmr->overflow = 0;
}


void updateTimer(struct timer_S * tmr){
  uint32_t t;
  t = millis();

  if(t < tmr->tick){
    tmr->overflow++;
  }
  tmr->tick = t;  
}


void addToTimer(struct timer_S * tmr, uint32_t t){
  uint32_t newTick;
  newTick = tmr->tick + t;
  if(newTick < tmr->tick){
    tmr->overflow++;
  }
  tmr->tick = newTick;
}


void printPciState(){
    snprintf_P(pbuf, sizeof(pbuf), PSTR("(%7lu) PCIe State: %1u, %S"),millis(), (uint8_t) pciState, pciStateStr[(int) pciState]);
    Serial.println(pbuf);
}


void printPostTimer(){
  if(printPt){
    snprintf_P(pbuf, sizeof(pbuf), PSTR("(%7lu) Post Timer: %lu, %lu"),millis(), postTimer.tick, postTimer.overflow);
    Serial.println(pbuf);
  }
}


void loop() {
  uint8_t p;
  enum pciState_E newPciState;

  p = readPostState();
  updateTimer(&milli);

  if(p != postState){
    postState = p;
    snprintf_P(pbuf, sizeof(pbuf), PSTR("(%7lu) POST: %d %d %d %d"), millis(), p & 1, p & 2, p & 4 ? 3 : 0, p & 8 ? 4 : 0);
    Serial.println(pbuf);
    printPostTimer();
  }

  // put your main code here, to run repeatedly:
  switch(measState){
    case TIDLE:{
      break;
    }
    case TMEAS:{
      int a[6];
      uint32_t newTick;
      
      a[0] = analogRead(A0);
      a[1] = analogRead(A1);
      a[2] = analogRead(A2);
      a[3] = analogRead(A3);
      a[4] = analogRead(A4);
      a[5] = analogRead(A5);
    
      for(int i=0;i<6;i++){
        Serial.print(i);
        Serial.print(":");
        Serial.println(a[i],DEC);
      }
      measState = TWAIT;
      addToTimer(&meas, 2000);
      break;
    }
    default:
    case TWAIT:{
      if(checkExpired(&meas)){
          measState = TMEAS;        
      }
      break;
    }
  }

  newPciState = pciState;
  switch(pciState){
    case PCIE_DEACTIVATE:
      digitalWrite(PCIE_NSHUTDOWN, 0);
      newPciState = POST;
    break;
    case POST:
      // 0 2 0 4
      if(postState == (0x2 + 0x8)){
        postTimer = milli;
        addToTimer(&postTimer, 2000);
        newPciState = WAIT_DELAY;
      }
    break;
    case WAIT_DELAY:
      if(checkExpired(&postTimer)){
        newPciState = PCIE_ACTIVATE;
      }
    break;
    case PCIE_ACTIVATE:{
      digitalWrite(PCIE_NSHUTDOWN, 1);
      newPciState = WAIT_POST_DONE;
      break;
    }
    case WAIT_POST_DONE:{
      if(postState != 0){
        // re-trigger timer for post states != 0
        postTimer = milli;
        addToTimer(&postTimer, 2000);         
      }
      if((postState == 0) && checkExpired(&postTimer)){
        newPciState = RUNNING;
      }
      break;
    }
    default:
    case RUNNING:{
      if(postState == 0){
        // re-trigger timer for post state == 0
        postTimer = milli;
        addToTimer(&postTimer, 10);         
      }
      if((postState != 0) && checkExpired(&postTimer)){
        // deactivate PCIe power if post state != 0 for >= 10 ms
        newPciState = PCIE_DEACTIVATE;
      }
      break;
    }
  }

  if(pciState != newPciState){
    pciState = newPciState;
    printPciState();
    printPostTimer();
  }
  
  if (Serial.available()){
    char c;
    c = (char) Serial.read();
    switch(c){
      case 't':
        if(measState == TIDLE){
          measState = TMEAS;
        }
        break;
      case ' ':
        printPciState();
        printPostTimer();
        break;
      case 'o':
        override = 1;
        break;
      case 'p':
        printPt ^= 1;
        break;
      case '1':
        op ^= 1;
        break;
      case '2':
        op ^= 2;
        break;
      case '3':
        op ^= 4;
        break;
      case '4':
        op ^= 8;
        break;
    }
  }
}

/*
 * 
T3500 Post Ablauf
00:30:03.278 -> POST: 1 2 3 4
00:30:03.278 -> POST: 0 0 0 0
00:30:03.315 -> POST: 1 0 0 0
00:30:03.315 -> POST: 1 0 3 0
00:30:03.690 -> POST: 0 0 3 0
00:30:03.690 -> POST: 0 0 0 0
00:30:04.153 -> POST: 1 0 0 0
00:30:04.153 -> POST: 1 0 3 4
00:30:04.153 -> POST: 0 0 3 4
00:30:04.297 -> POST: 1 0 3 4
00:30:04.297 -> POST: 0 0 3 4
00:30:04.297 -> POST: 0 2 0 0
00:30:04.297 -> POST: 0 2 3 4
00:30:04.331 -> POST: 0 0 3 4
00:30:04.368 -> POST: 0 0 0 4
00:30:04.368 -> POST: 0 0 3 4
00:30:04.401 -> POST: 0 0 0 4
00:30:04.401 -> POST: 0 0 0 0
00:30:04.436 -> POST: 0 0 0 4
00:30:04.436 -> POST: 0 0 0 0
00:30:04.436 -> POST: 0 0 0 4
00:30:04.436 -> POST: 0 0 0 0
00:30:04.544 -> POST: 1 0 3 4
00:30:04.578 -> POST: 0 0 3 4
--
00:30:12.326 -> POST: 0 2 0 4
00:30:14.570 -> POST: 1 2 3 4
00:30:14.570 -> POST: 1 2 3 0
00:30:15.114 -> POST: 0 0 3 0
00:30:15.182 -> POST: 0 0 3 4
00:30:15.363 -> POST: 1 0 0 4
00:30:18.449 -> POST: 1 2 3 0
00:30:18.449 -> POST: 0 2 3 0
00:30:18.449 -> POST: 0 2 3 4
00:30:18.984 -> POST: 1 2 3 0
00:30:18.984 -> POST: 0 2 3 0
00:30:19.021 -> POST: 1 2 3 0
00:30:19.414 -> POST: 1 2 0 0
00:30:19.451 -> POST: 1 2 3 0
00:30:19.451 -> POST: 0 2 3 0
00:30:19.451 -> POST: 1 0 0 4
00:30:20.537 -> POST: 0 2 3 0
00:30:20.537 -> POST: 1 2 3 0
00:30:20.714 -> POST: 1 2 3 4
00:30:20.787 -> POST: 1 2 0 4
00:30:20.787 -> POST: 1 2 0 0
00:30:23.215 -> POST: 1 2 3 0
00:30:23.215 -> POST: 0 2 3 0
00:30:23.215 -> POST: 0 0 3 0
00:30:23.283 -> POST: 1 0 3 0
00:30:23.283 -> POST: 1 2 3 0
00:30:23.926 -> POST: 0 0 0 0
00:30:26.211 -> POST: 0 2 0 0
00:30:26.211 -> POST: 0 0 0 0

 */
