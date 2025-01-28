/****************************************************************************
 *
 * Project:   name
 *
 * @file      filename.cpp
 * @author    author
 * @date      [Creation date in format %02d.%02d.20%02d]
 *
 * @brief     description
 *
 ****************************************************************************/
#include <stdint.h>
#include <UserCEvents.h>  /* Events send to the Cycle function              */
#include <UserCAPI.h>     /* API-Function declarations                      */

#include "vartab.h"       /* Variable Table definitions:                    */
                          /* include this file in every C-Source to access  */
                          /* the variable table of your project.            */
                          /* vartab.h is generated automatically.           */

#include "objtab.h"       /* Object ID definitions:                         */
                          /* include this file in every C-Source to access  */
                          /* the object ID's of the visual objects.         */
                          /* objtab.h is generated automatically.           */

#include "DataPanel.hpp"

extern "C"
{
  int32_t UserCPPCANCallbackIO(tCanFrame *msg, void *user);
  int32_t UserCPPCANCallbackHeartBeat(tCanFrame *msg, void *user);
}

// ... enter your cpp file content here ...

/****************************************************************************
 * @brief  Constructor of DataPanel class
 *
 * @param  evtc Number of events available (Event count)
 * @param  evtv Pointer to event value structures
 *              (see UserC-Guide docu in Help menu)
 ****************************************************************************/
DataPanel::DataPanel() //uint32_t CanId, uint32_t NodeId) : CanId(CanId), NodeId(NodeId)
{

};

/****************************************************************************
 * @brief  Destructor of DataPanel class
 *
 * @param  evtc Number of events available (Event count)
 * @param  evtv Pointer to event value structures
 *              (see UserC-Guide docu in Help menu)
 ****************************************************************************/
DataPanel::~DataPanel()
{
};


void DataPanel::init()
{
  CANAddRxFilterCallBack(mChannel, 0x180+mNodeId, 0x7ff, GS_CAN_FLAG_STD_ID, UserCPPCANCallbackIO, NULL);
  CANAddRxFilterCallBack(mChannel, 0x700+mNodeId, 0x7ff, GS_CAN_FLAG_STD_ID, UserCPPCANCallbackHeartBeat, NULL);

  SetStatusLedColor(0, 0xffff, 0, 0);
}

void DataPanel::send1b(uint32_t cobId, uint16_t ind, uint8_t subind, int8_t value)
{
  tCanFrame msg_out;
  msg_out.mId = cobId;
  msg_out.mFlags = GS_CAN_FLAG_STD_ID;//11-Bit
  msg_out.mChannel = mChannel;
  msg_out.mLen = 5;//Data lenght
  msg_out.mData.u8[0] = 0x2f;
  msg_out.mData.u8[1] = ind & 0x00ff;
  msg_out.mData.u8[2] = ind >> 8;
  msg_out.mData.u8[3] = subind;
  msg_out.mData.u8[4] = value;
  CANSendFrame(&msg_out);
}
void DataPanel::send2b(uint32_t cobId, uint16_t ind, uint8_t subind, int16_t value)
{
  tCanFrame msg_out;
  msg_out.mId = cobId;
  msg_out.mFlags = GS_CAN_FLAG_STD_ID;//11-Bit
  msg_out.mChannel = mChannel;
  msg_out.mLen = 6;//Data lenght
  msg_out.mData.u8[0] = 0x2b;
  msg_out.mData.u8[1] = ind & 0x00ff;
  msg_out.mData.u8[2] = ind >> 8;
  msg_out.mData.u8[3] = subind;
  msg_out.mData.u16[2] = value;
  CANSendFrame(&msg_out);
}
void DataPanel::config_rio()
{
  uint32_t cobId = 0x600+mNodeId;
  send1b(cobId, 0x2000, 0,    1);//set param 24v
  send1b(cobId, 0x2001, 4,    4);//set param 4a in pwm
  send1b(cobId, 0x2001, 5,    0x21);//set param 5a 5b
  send1b(cobId, 0x2002, 0xd,  100);//set param kp=1,00
  send1b(cobId, 0x2002, 0xe,  100);//set param ki=1,00
  send2b(cobId, 0x3000, 0,    100);//100Hz
}
void DataPanel::set_rio_oper()
{
  tCanFrame msg_out;
  msg_out.mId = 0;
  msg_out.mFlags = GS_CAN_FLAG_STD_ID;//11-Bit
  msg_out.mChannel = mChannel;
  msg_out.mLen = 2;//Data lenght
  msg_out.mData.u8[0] = 1;
  msg_out.mData.u8[1] = mNodeId;
  CANSendFrame(&msg_out);
}

int32_t DataPanel::rio_digital(tCanFrame *msg, void *user)//get byte
{
  if(1 == msg->mLen)
  {
    mIN[8] = TestBit(msg->mData.u8[0], 0);
    mIN[9] = TestBit(msg->mData.u8[0], 1);
  }
}

int32_t DataPanel::heartBeat(tCanFrame *msg, void *user)
{
  if(1 == msg->mLen)
  {
    switch (msg->mData.u8[0])
    {
    case 0x05://operational
      mOnline=true;
      break;
    case 0x7f://pre
      mOnline=false;
      config_rio();
      set_rio_oper();
      break;
    default:
      mOnline=false;
    }
  }
}
