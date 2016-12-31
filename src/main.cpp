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
 * @return: command to send back to ESP or [NONE] or [RECALL]
 */
String process(String token)
{
    static MachineState MACH_STATE = MODE_SETUP;
    static ProtocolState PROTO_STATE = WAIT_READY;
    String ret = "[NONE]";
    String command = "";
    MachineState next = MODE_SETUP;

    //Print states for debugging purposes
    SerialTTY.print("Machine State: ");
    SerialTTY.print(MACH_STATE);
    SerialTTY.print(" Protocol State: ");
    SerialTTY.print(PROTO_STATE);
    SerialTTY.print(" Token: ");
    SerialTTY.println(token);
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
        return "AT+RST";
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
            command = "AT+CWJAP=\""+SSID+\",\""+PASSWORD+"\"";
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
            if (token.endsWith("ON"))
            {
                digitalWrite(SWITCH,HIGH);
                digitalWrite(LED,HIGH);
            }
            else if (token.endsWith("OFF"))
            {
                digitalWrite(SWITCH,LOW);
                digitalWrite(LED,LOW);
            }
            command = "[NONE]";
            next = ON_OFF;
            break;
        //Fail-safe state
        default:
            command = "[NONE]";
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
            ret = command;
            PROTO_STATE = WAIT_ACK;
            break;
        //Wait for the ESP to ACK the command
        case WAIT_ACK:
            SerialTTY.print("Waiting for: '");
            SerialTTY.print(command);
            SerialTTY.print("' but got '");
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
            if (token == "OK")
            {
                PROTO_STATE = SETUP;
                MACH_STATE = next;
                ret = "[RECALL]";
            }
            break;
    }
    return ret;
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
    //Read a complete response
    String response = "";
    String command = "[RECALL]";
    while (response[response.length()-1] != '\n' || response[response.length()-2] != '\r')
    {
        response += SerialESP.readString();
    }
    int from = 0;
    int index = 0;
    //Loop through gotten responses
    while ((index = response.indexOf("\r\n",from)) != -1)
    {
        //Update loop variables for next token
        String token = response.substring(from,(response[index-1] != '\r')?index:index-1);
        from = index + 2;
        //Process this token running the state machine. A recall command output re-runs the state-machine (expected null response from ESP)
        while ((command = process(token)) == "[RECALL]")
        {}
        //Send a command if it is valid
        if (command != "[NONE]" && command != "")
        {
            SerialTTY.print("Command:  ");
            SerialTTY.println(command);
            SerialESP.print(command+"\r\n");
        }
    }
}
