#include <UserCEvents.h>
#include <UserCAPI.h>
#include <gsAudio.h>
#include "vartab.h"
#include "objtab.h"
#include "gstovisu.h"

#include "..\..\lib.cpp"
IPFunctions ipFunctions;

#define TasteAnzahl 3
#define TasteInhalt 105
#define dimmTimer 60000

class Maske0 : public Maske
{
private:
public:
	Maske0() {}
	void cycle(uint32_t evtc, tUserCEvt *evtv) override
	{
	}
	void timer() override
	{
	}
};


class Application
{
private:
	std::vector<std::unique_ptr<Maske>> masken;
public:
	Application()
	{
		masken.push_back(std::make_unique<Maske0>()); // Maske0 hinzufügen

		SetBuzzer(1, 1, 1, 1);
	}
	void Cycle(uint32_t evtc, tUserCEvt *evtv)
	{
		menu = keymenu(evtc, evtv, TasteAnzahl, TasteInhalt, dimmTimer);
		masken[GetCurrentMaskShown()]->cycle(evtc, evtv);
	}
	void Timer(void)
	{
		ipFunctions.ProcessFunctions();
		masken[GetCurrentMaskShown()]->timer();
	}
};
Application *application;
extern "C"
{
	int UserCPPInit(uint32_t initFlags)
	{
		application = new Application();
		return 100; // 1s timer
	}
	void UserCPPCycle(uint32_t evtc, tUserCEvt *evtv)
	{
		application->Cycle(evtc, evtv);
	}
	void UserCPPTimer(void)
	{
		application->Timer();
	}
	void UserCPPDeInit(void)
	{
		delete application;
	}
}
