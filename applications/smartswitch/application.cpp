// define whetere we are a buttonPad or behind a gira
#define __buttonPad
//#define __gira

//#include "spark_wiring.h"

// hardware libraries
#include "lib/WS2812B.h"
#include "lib/MCP23017.h"

// software libraries
#include "SmartSwitchConfig.h"
#include "lib/SparkIntervalTimer.h"
#include "lib/QueueList.h"

// Insert firearm metaphor here
/* The Spark Core's manual mode puts everything in your hands. 
This mode gives you a lot of rope to hang yourself with, so tread cautiously. */
//SYSTEM_MODE(SEMI_AUTOMATIC);

#ifdef __buttonPad
WS2812B leds;
#endif /* buttonPad */


// to debug via serial console uncomment the following line:
#define SERIAL_DEBUG

Adafruit_MCP23017 mcp;

IntervalTimer myTimer;

SMARTSWITCHConfig myConfig;


#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_T_HOLD 1000
#define BTN_T_DOUBLE 200
#define BTN_COUNT 6
#define MAX_QUEUE_LENGTH 3


// millis of the last btn up
unsigned long btn_last_up [BTN_COUNT] = {0UL};

// millis of the last btn down
unsigned long btn_last_down [BTN_COUNT] = {0UL};

// Interrupts from the MCP will be handled by this PIN
byte SparkIntPIN = D3;

volatile int pressCount = 0;

volatile boolean inInterrupt = false;

volatile int lastLedAction = 0;

// create a queue of t_btn_events.
QueueList <t_btn_event> btn_event_queue;

void stopLEDs(){
    leds.setColor(0, 0, 0, 0);
    leds.setColor(1, 0, 0, 0);
    leds.setColor(2, 0, 0, 0);
    leds.setColor(3, 0, 0, 0);
    leds.show();
    lastLedAction = 0;
}


// This function gets called whenever there is a matching API request
// the command string format is <led number>,<state>
// for example: 1,HIGH or 1,LOW
//              2,HIGH or 2,LOW

int ledControl(String command) {

    int state = 0;
    int mcpPin = -1;
    int ledNumber = -1;
    char * params = new char[command.length() + 1];

    strcpy(params, command.c_str());
    char * param1 = strtok(params, ",");
    char * param2 = strtok(NULL, ",");


    if (param1 != NULL && param2 != NULL) {
        ledNumber = atoi(param1);

        /* Check for a valid digital pin */
        if (ledNumber < 0 || ledNumber > 5) return -1;

        /* find out the state of the led */
        if (!strcmp(param1, "HIGH")) state = 1;
        else if (!strcmp(param1, "LOW")) state = 0;
        else return -1;

        // write to the appropriate pin on the mcp
        switch (ledNumber) {
            case 0:
                mcpPin = BTN_LED_0;
                break;
            case 1:
                mcpPin = BTN_LED_1;
                break;
            case 2:
                mcpPin = BTN_LED_2;
                break;
            case 3:
                mcpPin = BTN_LED_3;
                break;
            case 4:
                mcpPin = BTN_LED_4;
                break;
            case 5:
                mcpPin = BTN_LED_5;
                break;
        }
        if (mcpPin >= 0) {
            mcp.digitalWrite(mcpPin, state);
            return 0;
        }
    }
    return -1;
}


// This function gets called whenever there is a matching API request
// the command string format is <led number>,<Red>,<Green>,<Blue>
// for example: 1,000,000,000

int ledControlRGB(String command) {

    int ledNumber = -1;
    int red = 0;
    int blue = 0;
    int green = 0;

    char * params = new char[command.length() + 1];

    strcpy(params, command.c_str());
    char * param1 = strtok(params, ",");
    char * param2 = strtok(NULL, ",");
    char * param3 = strtok(NULL, ",");
    char * param4 = strtok(NULL, ",");


    if (param1 != NULL && param2 != NULL && param3 != NULL && param4 != NULL) {
        ledNumber = atoi(param1);

        /* Check for a valid digital pin */
        if (ledNumber < 0 || ledNumber > 4) return -1;

        red = atoi(param2);
        blue = atoi(param3);
        green = atoi(param4);

        if (red < 0 || red > 255) return -1;
        if (blue < 0 || blue > 255) return -1;
        if (green < 0 || green > 255) return -1;

        leds.setColor(ledNumber, red, green, blue);
        leds.show();
        lastLedAction = millis();
        return 0;
    }
    return -1;
}

void handleButtonINT() {
    noInterrupts();
    inInterrupt = true;

}

void processButtonINT() {

    uint8_t pin = mcp.getLastInterruptPin();
    uint8_t val = mcp.getLastInterruptPinValue();

#ifdef SERIAL_DEBUG
    Serial.println("Interrupt detected!");
    Serial.println(pin);
    Serial.println(val);
#endif /* SERIAL_DEBUG */

    pressCount = pressCount + 1;
    
    //this will clear the MCP interrupt
    mcp.readGPIOAB();

    // relevant PIN
    // TODO check if queue has enough free space
    if (pin >= 1 && pin <= 16) {
        // current time, so we don't have to call millis() all the time
        unsigned long now = millis();
        // index in array starts at zero
        int idx = pin - 1;

        // button release
        // we do not process events while the queue is full
        if (val == BTN_UP && (btn_event_queue.count() < MAX_QUEUE_LENGTH)) {
            // hold condition
            // if (now - btn_last_down[idx] >= BTN_T_HOLD) {
            //                 t_btn_event _btn_event;
            //                 _btn_event.btn = pin;
            //                 _btn_event.event = BTN_HOLD;
            //                 btn_event_queue.push(_btn_event);
            //             }// double click condition
            //else 
            if ((now - btn_last_up[idx] < BTN_T_DOUBLE) && (now - btn_last_down[idx] < BTN_T_HOLD)) {
                t_btn_event _btn_event;
                _btn_event.btn = pin;
                _btn_event.event = BTN_DOUBLE;
                btn_event_queue.push(_btn_event);
            }// otherwise we have seen a single click
            else {
                t_btn_event _btn_event;
                _btn_event.btn = pin;
                _btn_event.event = BTN_SINGLE;
                btn_event_queue.push(_btn_event);
            }
            btn_last_up[idx] = millis();
        }// button press	
        else if (val == BTN_DOWN) {
            btn_last_down[idx] = millis();
        }// unrecognized button event
        else {
            //DEBUG
        }
    }
    

}

void setup() {

    delay(1000);

#ifdef SERIAL_DEBUG
    Serial.begin(9600);

    while (!Serial.available()) { // Wait here until the user presses ENTER 
        SPARK_WLAN_Loop(); // in the Serial Terminal. Call the BG Tasks
    }
#endif /* SERIAL_DEBUG */

    // connect to the WiFi
    WiFi.connect();
    // wait until it is actually connected
    while (!WiFi.ready()) SPARK_WLAN_Loop();

    // initialize configuration
    myConfig.setup();

    //Register our Spark function here
    Spark.function("led", ledControl);
    Spark.function("ledrgb", ledControlRGB);

    pinMode(SparkIntPIN, INPUT);

#ifdef __buttonPad
    leds.setup(4);
    leds.setColor(0, 255, 255, 0);
    leds.setColor(1, 0, 255, 0);
    leds.setColor(2, 0, 0, 255);
    leds.setColor(3, 255, 0, 0);
    leds.show();
    lastLedAction = millis();
#endif /* __buttonPad */


    mcp.begin(); // use default address 0

    mcp.pinMode(BTN_0, INPUT);
    mcp.pullUp(BTN_0, HIGH);

    mcp.pinMode(BTN_1, INPUT);
    mcp.pullUp(BTN_1, HIGH);

    mcp.pinMode(BTN_2, INPUT);
    mcp.pullUp(BTN_2, HIGH);

    mcp.pinMode(BTN_3, INPUT);
    mcp.pullUp(BTN_3, HIGH);

    mcp.pinMode(BTN_4, INPUT);
    mcp.pullUp(BTN_4, HIGH);

    mcp.pinMode(BTN_5, INPUT);
    mcp.pullUp(BTN_5, HIGH);

    mcp.pinMode(BTN_6, INPUT);
    mcp.pullUp(BTN_6, HIGH);

    mcp.pinMode(BTN_7, INPUT);
    mcp.pullUp(BTN_7, HIGH);

#ifdef __gira
    mcp.pinMode(BTN_LED_0, OUTPUT);
    mcp.pinMode(BTN_LED_1, OUTPUT);
    mcp.pinMode(BTN_LED_2, OUTPUT);
    mcp.pinMode(BTN_LED_3, OUTPUT);
    mcp.pinMode(BTN_LED_4, OUTPUT);
    mcp.pinMode(BTN_LED_5, OUTPUT);

    mcp.digitalWrite(BTN_LED_0, HIGH);
    mcp.digitalWrite(BTN_LED_1, HIGH);
    mcp.digitalWrite(BTN_LED_2, HIGH);
    mcp.digitalWrite(BTN_LED_3, HIGH);
    mcp.digitalWrite(BTN_LED_4, HIGH);
    mcp.digitalWrite(BTN_LED_5, HIGH);

#endif /* __gira */   

    // AUTO allocate printQcount to run every 1000ms (2000 * .5ms period)
    // myTimer.begin(printQcount, 3000, hmSec);


    // first boolean expression mirros interrupts of the two banks of the MCP
    // it seems not to interfere with our use case, but we don't need it 
    // so it is set to false
    mcp.setupInterrupts(false, false, LOW);
    // TODO only initialize interrupts needed ?
    //      source out to SMARTSWITCHConfig.setup() ?
    mcp.setupInterruptPin(BTN_0, CHANGE);
    mcp.setupInterruptPin(BTN_1, CHANGE);
    mcp.setupInterruptPin(BTN_2, CHANGE);
    mcp.setupInterruptPin(BTN_3, CHANGE);
    mcp.setupInterruptPin(BTN_4, CHANGE);
    mcp.setupInterruptPin(BTN_5, CHANGE);
    mcp.setupInterruptPin(BTN_6, CHANGE);
    mcp.setupInterruptPin(BTN_7, CHANGE);

    mcp.readGPIOAB();

    // Spark Interupt 
    attachInterrupt(SparkIntPIN, handleButtonINT, FALLING);

#ifdef SERIAL_DEBUG
    Serial.println("Hello :)");
#endif /* SERIAL_DEBUG */
}

/**
 * main routine
 */
void loop() {

    if (inInterrupt) {
        processButtonINT();
        inInterrupt = false;
        interrupts();
    }

#ifdef __buttonPad
    /* stop LEDs after 10 seconds */
    if(lastLedAction > 0 && millis() - lastLedAction > 10000){
        stopLEDs();
    }
#endif /* __buttonPad */
    

#ifdef SERIAL_DEBUG
    if (pressCount > 50) {
        Serial.println("50 interrupts...");
        Serial.println(pressCount);
        pressCount = 0;
    }
#endif /* SERIAL DEBUG */

    if (!btn_event_queue.isEmpty()) {
        t_btn_event _btn_event = btn_event_queue.pop();

        if (_btn_event.event == BTN_SINGLE) {
            // warten double click time
            delay(BTN_T_DOUBLE);
            // we waited for some time, is there something new in the queue?
            if (!btn_event_queue.isEmpty()) {
                t_btn_event _btn_next_event = btn_event_queue.peek();
                // does the new event concern the same button as the current ?
                // is is the old current event a single click ? This is necessairy so we
                // don't throw away two following double clicks or something like that
                if (_btn_event.btn == _btn_next_event.btn && _btn_event.event == BTN_SINGLE) {
                    // we assume the current event was only the first part of a double click
                    // so we skip it and replace it with the new one
                    // the new one will be popped from the queue
                    _btn_event = btn_event_queue.pop();
                }
            }
        }

        // interpret the event and fire desired action
        myConfig.process(&_btn_event);
    }

}


