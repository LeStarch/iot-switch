/*
 * main.cpp
 *
 *  Created on: Dec 31, 2016
 *      Author: starchmd
 */

#include <Arduino.h>
#include "machine.h"
#include "private.h" //Create this with SSID and PASSWORD #defs to configure the switch

//Setup the serial port used for USB logging
#define SerialTTY Serial
//Setup the serial port used for the ESP
#define SerialESP Serial1
//Setup chip enable
#define ESP_CE 2
//Setup output PIN
#define LED 13
#define SWITCH 6
/**
 * Process tokens received
 * @param token - string received from ESP
 */
void process(String token)
{
    static MachineState MACH_STATE = MODE_SETUP;
    static ProtocolState PROTO_STATE = WAIT_READY;

    static MachineState LAST_MACH_STATE = MODE_SETUP;
    static ProtocolState LAST_PROTO_STATE = WAIT_READY;


    static String user = "";
    static String payload = "";

    String command = "";
    MachineState next = MODE_SETUP;

    //Nothing changed, don't do anything
    if (token == "" && LAST_PROTO_STATE == PROTO_STATE && LAST_MACH_STATE == MACH_STATE)
    {
        return;
    }
    LAST_MACH_STATE = MACH_STATE;
    LAST_PROTO_STATE = PROTO_STATE;
    //Print out state information
    SerialTTY.print("M");
    SerialTTY.print(MACH_STATE);
    SerialTTY.print(":P");
    SerialTTY.print(PROTO_STATE);
    if (token != "")
    {
        SerialTTY.print(" Resp: '");
        SerialTTY.print(token);
        SerialTTY.print("'");
    }
    SerialTTY.println();
    //Error handling, flash 3 times and reset
    if (token == "ERROR")
    {
        SerialTTY.println("[ERROR] Resetting and retrying");
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(LED,HIGH);
            delay(500);
            digitalWrite(LED,LOW);
            delay(500);
        }
        MACH_STATE = MODE_SETUP;
        PROTO_STATE = WAIT_READY;
        SerialESP.print("AT+RST\r\n");
        return;
    }
    //Machine state machine
    switch (MACH_STATE)
    {
        //Setup to be client mode to existing AP
        case MODE_SETUP:
            command = "AT+CWMODE=1";
            next = JOIN_AP;
            break;
        //Joing the local network
        case JOIN_AP:
            command = "AT+CWJAP=\"";
            command = command + SSID;
            command = command + "\",\"";
            command = command + PASSWORD;
            command = command + "\"";
            next = IP_LIST;
            break;
        //List the IP for debugging purposes
        case IP_LIST:
            command = "AT+CIFSR";
            next = CIP_MUX;;
            break;
        //Allow many clients
        case CIP_MUX:
            command = "AT+CIPMUX=1";
            next = START_SERVER;
            break;
        //Start the IP-based serial server
        case START_SERVER:
            command = "AT+CIPSERVER=1";
            next = ON_OFF;
            break;
        //State which loops handling clients sending ON/OFF
        case ON_OFF:
            //Harvest user to respond to
            if (token.startsWith("+IPD,"))
            {
                user = token.substring(5, token.indexOf(',', 5));
            }
            //Check for ON/OFF commands
            if (token.endsWith("ON"))
            {
                digitalWrite(SWITCH,HIGH);
                digitalWrite(LED,HIGH);
                //Force protocol "OK" as ON/OFF don't emit "OK"
                next = RESPOND_SETUP;
                PROTO_STATE = WAIT_OK;
                token = "OK";
            }
            else if (token.endsWith("OFF"))
            {
                digitalWrite(SWITCH,LOW);
                digitalWrite(LED,LOW);
                //Force protocol "OK" as ON/OFF don't emit "OK"
                next = RESPOND_SETUP;
                PROTO_STATE = WAIT_OK;
                token = "OK";
            }
            break;
        //Acks back to the requester
        case RESPOND_SETUP:
            payload = "HTTP/1.1 200 OK\r\n\r\nACK";
            command = "AT+CIPSEND=";
            command = command + user;
            command = command + ",";
            command = command + payload.length();
            next = RESPOND_CLOSE;
            break;
        //Acks back to the requester
        case RESPOND_CLOSE:
            command = "AT+CIPCLOSE=";
            command = command + user;
            next = ON_OFF;
            break;
        //Fail-safe state
        default:
            next = MACH_NONE;
    }
    //Protocol state machine
    switch (PROTO_STATE)
    {
        //Wait for ESP to report ready on boot or RESET
        case WAIT_READY:
            if (token != "ready")
            {
                break;
            }
        //Setup sends the command out, and defers to the ESP for an ACK
        case SETUP:
            if (command != "")
            {
                SerialTTY.print("      Cmd:  '");
                SerialTTY.print(command);
                SerialTTY.println("'");
                SerialESP.print(command+"\r\n");
            }
            PROTO_STATE = WAIT_ACK;
            if (payload != "")
            {
                PROTO_STATE = PAYLOAD;
            }
            break;
        //Send the payload
        case PAYLOAD:
            delay(200);
            if (payload != "")
            {
                SerialTTY.print("      Pay:  '");
                SerialTTY.print(payload);
                SerialTTY.println("'");
                SerialESP.print(payload);
                payload = "";
            }
            PROTO_STATE = WAIT_ACK;
            break;
        //Wait for the ESP to ACK the command
        case WAIT_ACK:
            payload = "";
            SerialTTY.print("      Wait: '");
            SerialTTY.print(command);
            SerialTTY.print("' == '");
            SerialTTY.print(token);
            SerialTTY.println("'");
            if (token == command)
            {
                PROTO_STATE = WAIT_OK;
            }
            break;
        //Wait for an OK response, and then move to the next command
        //OK doesn't usually preceed more text, so a [RECALL] forces another run through the machine
        case WAIT_OK:
            payload = "";
            if (token == "OK")
            {
                PROTO_STATE = SETUP;
                MACH_STATE = next;
            }
            break;
    }
}
/**
 * Sets up the serial port for use as a serial server.
 */
void setup()
{
    SerialTTY.begin(9600);
    SerialESP.begin(115200,SERIAL_8N1);
    pinMode(ESP_CE, OUTPUT);
    pinMode(SWITCH, OUTPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(ESP_CE,HIGH);
    digitalWrite(SWITCH,LOW);
    digitalWrite(LED,LOW);
    //Sleep to allow serial ports to catch-up
    delay(1000);
    SerialTTY.println("Serial Ports Started");

}
/**
 * Program loop that recieves a response and runs the state machine
 */
void loop()
{
    int from = 0;
    int index = 0;
    //Read a complete response
    static String response = "";
    String command = "[RECALL]";
    while (Serial1.available() > 0)
    {
        response = response + SerialESP.readString();
    }
    //Loop through gotten responses
    while ((index = response.indexOf("\r\n",from)) != -1)
    {
        //Update loop variables for next token
        String token = response.substring(from,(index >= 1 && response[index-1] != '\r')?index:index-1);
        from = index + 2;
        //Process this token running the state machine. A recall command output re-runs the state-machine (expected null response from ESP)
        process(token);
    }
    response = response.substring(from);
    process("");
}
