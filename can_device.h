#ifndef CAN_DEVICE_H
#define CAN_DEVICE_H

#include <UserCAPI.h>
#include <stdbool.h>

#define CAN_DEVICE_ID 126
#define CAN_INTERFACE GS_CAN_0

#include "state_machine.h"

// CAN-Rx-Callback
int32_t CanDevice_RxCallback(tCanFrame *frame, void *user)
{
    uint32_t nodeID = frame->mId & 0x7F;
    uint32_t cob = frame->mId & 0x780;

    switch (cob)
    {
    case 0x180:
        sm.key = frame->mData.u16[0];
        StateMachine_SetEvent(EV_KEY_180);
        StateMachine_SetEvent(EV_ANSWER_180);
        break;
    case 0x580:
        StateMachine_SetEvent(EV_ANSWER_580);
        break;
    case 0x700:
        StateMachine_SetEvent(EV_ANSWER_700);
        break;
    }
    return 0;
}

// Init CAN-Rx-Filter
void CanDevice_Init(void)
{
    // empfang keycode
    CANAddRxFilterCallBack(CAN_INTERFACE, 0x180 + CAN_DEVICE_ID,
                           GS_CAN_STD_ID_MSK, GS_CAN_FLAG_STD_ID,
                           CanDevice_RxCallback, 0);
    // empfang antwort
    CANAddRxFilterCallBack(CAN_INTERFACE, 0x580 + CAN_DEVICE_ID,
                           GS_CAN_STD_ID_MSK, GS_CAN_FLAG_STD_ID,
                           CanDevice_RxCallback, 0);
    // empfang pong
    CANAddRxFilterCallBack(CAN_INTERFACE, 0x700 + CAN_DEVICE_ID,
                           GS_CAN_STD_ID_MSK, GS_CAN_FLAG_STD_ID,
                           CanDevice_RxCallback, 0);
}

// Sende-Funktionen
void CanDevice_SendReset(uint8_t nodeID)
{
    tCanFrame frm = {
        .mChannel = CAN_INTERFACE,
        .mFlags = GS_CAN_FLAG_STD_ID,
        .mId = 0,
        .mLen = 2};
    frm.mData.s8[0] = 0x81;
    frm.mData.s8[1] = nodeID;
    CANSendFrame(&frm);
}

void CanDevice_SendSetOperational(uint8_t nodeID)
{
    tCanFrame frm = {
        .mChannel = CAN_INTERFACE,
        .mFlags = GS_CAN_FLAG_STD_ID,
        .mId = 0,
        .mLen = 2};
    frm.mData.s8[0] = 1;
    frm.mData.s8[1] = nodeID;
    CANSendFrame(&frm);
}

void CanDevice_SendGetProductID(uint8_t nodeID)
{
    tCanFrame frm = {
        .mChannel = CAN_INTERFACE,
        .mFlags = GS_CAN_FLAG_STD_ID,
        .mLen = 8};
    frm.mId = 0x600 | nodeID;
    frm.mData.u32[0] = 0x02101840;
    frm.mData.u32[1] = 0;
    CANSendFrame(&frm);
}

#endif
