/*
 * machine.h
 *
 *  Created on: Dec 31, 2016
 *      Author: starchmd
 */

#ifndef SRC_MACHINE_H_
#define SRC_MACHINE_H_
/**
 * Machine states used to connect to AP, and listen
 * for ON and OFF commands
 */
enum MachineState
{
    MODE_SETUP,
    JOIN_AP,
    IP_LIST,
    CIP_MUX,
    START_SERVER,
    ON_OFF,
    MACH_NONE
};
/**
 * ESP protocol handler:
 *  0. on startup/ reset wait for "ready"
 *  1. send command
 *  2. ESP will reply with command
 *  3. wait for "OK" to continue
 */
enum ProtocolState
{
    WAIT_READY,
    SETUP,
    WAIT_ACK,
    WAIT_OK
};
#endif /* SRC_MACHINE_H_ */
