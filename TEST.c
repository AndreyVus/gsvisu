#include <UserCEvents.h>
#include <UserCAPI.h>
#include "vartab.h"
#include "objtab.h"
int UserCPPInit(uint32_t initFlags);
void UserCPPCycle(uint32_t evtc, tUserCEvt *evtv);
void UserCPPTimer(void);
void UserCPPDeInit(void);

int UserCInit(uint32_t initFlags)
{
	/* The return value is the interval of UserC-Timer in steps of 10 ms */
	/* Returning 0 disables the function UserCTimer */
	return UserCPPInit(initFlags);
}
void UserCCycle(uint32_t evtc, tUserCEvt *evtv)
{
	UserCPPCycle(evtc, evtv);
}
void UserCTimer(void)
{
	UserCPPTimer();
}