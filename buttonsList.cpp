#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // for size_t
#include <UserCEvents.h>
#include <UserCAPI.h>
#include "vartab.h"
#include "objtab.h"
#include "gstovisu.h"
#include "RCColor.h"


int32_t getContent(uint32_t ec, tUserCEvt *ev, uint32_t type, uint32_t source, tCEvtContent *content, int32_t foundMax)
{
  int32_t found = 0;
  for (uint32_t o = 0; o < ec; o++)
    if (ev[o].Type == type && ev[o].Source == source && found < foundMax)
      content[found++] = ev[o].Content; // Store content in array
  return found;
}

int32_t keymenu(uint32_t ec, tUserCEvt *ev)
{
	int32_t keyNr = IsAnyKeyDown();//key
	static bool fp;
	bool click = (keyNr != 0) && !fp;
	fp = (keyNr != 0);
	if (click)
		return keyNr;

	tCEvtContent menuCo[1];// Menüevent Handling
	for (int32_t o = getContent(ec, ev, CEVT_MENU_INDEX, CEVT_SOURCE_MENU, menuCo, GS_ARRAYELEMENTS(menuCo)) - 1; o >= 0; o--)
		return menuCo[o].mMenuIndex.ObjID;
	return -1; // Standardrückgabe
}
int32_t ImRang(int32_t num, int32_t min, int32_t max)
{
	return (num < min) ? min : (num > max) ? max : num;
}

typedef struct Button Button_t;
typedef struct ButtonList ButtonList_t;

struct Button
{
	int32_t id;
	bool zustand;
};

Button_t Button_create(int32_t _id)
{
	Button_t button;
	button.id = _id;
	button.zustand = false;
	return button;
}

void Button_click(Button_t* button)
{
	if (button == NULL) return;
	button->zustand = !button->zustand;
	SendToVisuObj(button->id, GS_TO_VISU_SET_BK_COLOR, button->zustand ? 0xc0ffc0 : 0xc0c0c0);
}

#define MAX_BUTTONS 4 // Define a maximum number of buttons in ButtonList

struct ButtonList
{
	int32_t select;
	Button_t buttonList[MAX_BUTTONS];
	size_t buttonCount; // Keep track of the number of buttons in the list
};

ButtonList_t ButtonList_create()
{
	ButtonList_t buttonListObj;
	buttonListObj.select = -1;
	buttonListObj.buttonCount = 0; // Initialize button count to 0
	return buttonListObj;
}

void ButtonList_addButton(ButtonList_t* buttonListObj, int32_t id)
{
	if (buttonListObj == NULL || buttonListObj->buttonCount >= MAX_BUTTONS) return;
	buttonListObj->buttonList[buttonListObj->buttonCount] = Button_create(id);
	buttonListObj->buttonCount++;
}

void ButtonList_click(ButtonList_t* buttonListObj, int32_t id)
{
	if (buttonListObj == NULL) return;
	for (size_t i = 0; i < buttonListObj->buttonCount; i++) {
		if (buttonListObj->buttonList[i].id == id) {
			Button_click(&buttonListObj->buttonList[i]);
			buttonListObj->select = buttonListObj->buttonList[i].zustand ? (int32_t)i : -1;
		} else if (buttonListObj->buttonList[i].zustand) {
			Button_click(&buttonListObj->buttonList[i]);
		}
	}
}

#define OFFSET_VAL 15 // Define offset as a macro for clarity
#define MAX_KANALLIST_SIZE 2 // Based on kanalList.resize(1) in Maske5 constructor

int32_t offset = OFFSET_VAL;
ButtonList_t kanalList[MAX_KANALLIST_SIZE];

int UserCInit(uint32_t initFlags)
{
	int32_t port = 0;
	SetVideoInDestinationViewport(port, 77, 72, 370, 256);//screen
	SetVideoInSourceViewport(port, 0, 0, 720, 576);//cam
	port = 1;
	SetVideoInDestinationViewport(port, 577, 72, 370, 256);//screen
	SetVideoInSourceViewport(port, 0, 0, 720, 576);//cam
	//SetVideoInOnOff(port, true);

	for(int32_t i=0; i< MAX_KANALLIST_SIZE; ++i) {
		kanalList[i] = ButtonList_create();
		for(int32_t j=0; j<MAX_BUTTONS; ++j){
			ButtonList_addButton(&kanalList[i], j + offset + i*MAX_BUTTONS);
		}
	}

	return 0;
}

void UserCCycle(uint32_t evtc, tUserCEvt * evtv)
{
	int32_t menu = keymenu(evtc, evtv);
	SetVar(HDL_STROEMUNG, GetMSTick() >> 4);
	if(ImRang(menu, offset, offset + MAX_BUTTONS * MAX_KANALLIST_SIZE - 1)){
		int32_t port = (menu - offset) / MAX_BUTTONS; // Integer division as in C++
		if(port >= 0 && port < MAX_KANALLIST_SIZE) // Safety check for port index
		{
			SetVideoInOnOff(port, false);//kanal Off
			ButtonList_click(&kanalList[port], menu);
			if(kanalList[port].select >= 0){
				SetVideoInParam(port, "input", kanalList[port].select);//kanal select on
				SetVideoInOnOff(port, true);
			}
		}
	}
}

void UserCTimer(void){}
