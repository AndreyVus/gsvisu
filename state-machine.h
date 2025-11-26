#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <UserCAPI.h>
#include <stdbool.h>
#include "ton.h"
#include "can_device.h"

#define sm_takt 1000
#define tTimeOut 5000

typedef enum
{
    OFFLINE,
    SET_OPER,
    ONLINE,
    GET_ID,
    ANTWORT
} State_t;

typedef enum
{
    EV_NONE = 0,
    EV_ANSWER_700 = (1 << 0),
    EV_ANSWER_180 = (1 << 1),
    EV_ANSWER_580 = (1 << 2),
    EV_KEY_180 = (1 << 3)
} EventBits_t;

typedef struct
{
    State_t curState;
    EventBits_t pending;
    bool online, takt;
    int32_t key;
    T_TON anfrageTON;
} StateMachine_t;

StateMachine_t sm = {0};

void setOnline()
{
    sm.online = true;
    HourCounterStart(2); // start online timer
    HourCounterStop(3);  // stop offline timer
    visu1();
}

void setOffline()
{
    sm.curState = OFFLINE;
    sm.online = false;
    sm.pending = EV_NONE;
    HourCounterStop(2);  // stop online timer
    HourCounterStart(3); // start offline timer
    visu1();
}

void StateMachine_Init(void)
{
    sm.curState = OFFLINE;
    sm.pending = EV_NONE;
    sm.online = false;
}

void StateMachine_SetEvent(EventBits_t event)
{
    sm.pending |= event;
}

void StateMachine_Update(void)
{
    switch (sm.curState)
    {
    case OFFLINE:
        CanDevice_SendReset(CAN_DEVICE_ID);
        sm.curState = SET_OPER;
        TON(&sm.anfrageTON, false, tTimeOut);
        break;

    case SET_OPER:
        if (sm.pending & EV_ANSWER_700)
        {
            sm.pending = EV_NONE;
            CanDevice_SendSetOperational(CAN_DEVICE_ID);
            sm.curState = ONLINE;
        }
        if (TON(&sm.anfrageTON, true, tTimeOut))
        {
            setOffline();
        }
        break;

    case ONLINE:
        if (sm.pending & EV_ANSWER_180)
        {
            sm.pending &= ~EV_ANSWER_180;
            setOnline();
            sm.curState = GET_ID;
        }
        if (TON(&sm.anfrageTON, true, tTimeOut))
        {
            setOffline();
        }
        break;

    case GET_ID:
        CanDevice_SendGetProductID(CAN_DEVICE_ID);
        TON(&sm.anfrageTON, false, tTimeOut);
        sm.curState = ANTWORT;
        break;

    case ANTWORT:
        if (sm.pending & EV_ANSWER_580)
        {
            sm.pending &= ~EV_ANSWER_580;
            sm.curState = GET_ID;
        }
        if (TON(&sm.anfrageTON, true, tTimeOut))
        {
            setOffline();
        }
        break;
    }
}

#endif
