#include <string>
#include <vector>
#include <atomic>
#include <algorithm>
#include <UserCAPI.h>
#include <UserCEvents.h>
#include <gsThread.h>
#include <gsAudio.h>
#include <gstovisu.h>
#include "gseth.cpp"
#include "FileQueue.cpp"
#include "vartab.h"
#include "objtab.h"
#include "printfo.h"

// ========================================
// Konfiguration
// ========================================
#define MUSIK_ORDNER "/tmp/usb/"
#define RECURSIVE false

class Application
{
private:
	GsEth gsEth;
	FileQueue musicQueue;
	int currentTrackIndex = 0;
	tGsThread t1;
	std::string audioDatei;
	bool usb_connected = false;
	std::vector<std::string> exts;

	// --- Thread-Synchronisation ---
	// std::atomic garantiert, dass keine Race-Conditions beim Lesen/Schreiben passieren
	std::atomic<bool> isRunning{false};		// Ist der Thread aktiv?
	std::atomic<bool> stopRequested{false}; // Wurde Stop gedrückt?
	static void *threadEntry(void *arg)
	{
		Application *self = static_cast<Application *>(arg);
		if (self)
			self->playLogic();
		return nullptr;
	}

	void playLogic()
	{
		// Sicherheitscheck
		if (musicQueue.empty())
		{
			isRunning.store(false);
			gsThreadExit(NULL);
			return;
		}
		if (currentTrackIndex >= (int)musicQueue.size())
			currentTrackIndex = 0;

		std::string audioDatei = musicQueue[currentTrackIndex++];

		// 1. Audio vorbereiten
		tGsAudioHdl audioHdl = gsAudioCreate();
		if (!audioDatei.empty())
		{
			SetVisObjData(OBJ_TITEL, audioDatei.c_str(), audioDatei.size() + 1);

			// Einfache Weiche für wav/mp3
			if (audioDatei.rfind(".wav") != std::string::npos)
				gsAudioPlayWAV(audioHdl, audioDatei.c_str());
			else
				gsAudioPlayDec(audioHdl, audioDatei.c_str()); // mp3 etc.

			// 2. Die Hauptschleife des Threads
			// Wir laufen solange Musik spielt UND kein Stop angefordert wurde
			do
			{					// Kurze Pause, um CPU zu sparen
				usleep(500000); // 500ms
			} while (gsAudioIsPlaying(audioHdl) && !stopRequested.load());
		}

		// 3. Aufräumen (Passiert IMMER, egal ob Stop gedrückt wurde oder Lied zu Ende ist)
		gsAudioDestroy(audioHdl);
		SetVisObjData(OBJ_TITEL, "bereit", 7);

		// 4. Status zurücksetzen: Thread meldet sich ab
		isRunning.store(false);

		// Thread beendet sich
		gsThreadExit(NULL);
	}

	// bool bt_start(tUserCEvt *e)
	// {
	// 	return (e->Type == CEVT_MENU_ACTION &&
	// 			e->Content.mMenuAction.mState == CEVT_STATE_PRESS &&
	// 			e->Content.mMenuAction.mObjID == OBJ_START);
	// }

	bool bt_next(tUserCEvt *e)
	{
		return (e->Type == CEVT_MENU_ACTION &&
				e->Content.mMenuAction.mState == CEVT_STATE_PRESS &&
				e->Content.mMenuAction.mObjID == OBJ_NEXT);
	}

	bool usb_on(tUserCEvt *e)
	{
		if (e->Type == CEVT_USB_MEMORY)
		{
			usb_connected = e->Content.mUsbMemory.mState;
			return usb_connected;
		}
		return false;
	}

	bool usb_off(tUserCEvt *e)
	{
		if (e->Type == CEVT_USB_MEMORY)
		{
			usb_connected = e->Content.mUsbMemory.mState;
			return !usb_connected;
		}
		return false;
	}

	void usb_off_action()
	{
		USBUnmountStick(MUSIK_ORDNER);
		musicQueue.init("", false, exts); // Queue leeren

		if (isRunning.load())
		{
			stopRequested.store(true); // Thread beenden lassen
			usleep(300000);			   // Thread sollte weg sein
		}
		SetVisObjData(OBJ_TITEL, "USB getrennt", 13);
	}
	// bool bt_reset(tUserCEvt *e)
	// {
	// 	return (e->Type == CEVT_MENU_ACTION &&
	// 			e->Content.mMenuAction.mState == CEVT_STATE_PRESS &&
	// 			e->Content.mMenuAction.mObjID == OBJ_RESET);
	// }
	// void reset_action()
	// {
	// 	// 1. Playback sofort beenden
	// 	if (isRunning.load())
	// 	{
	// 		stopRequested.store(true); // Thread beenden lassen
	// 		usleep(300000);			   // Thread sollte weg sein
	// 	}

	// 	// 2. Alle eigenen Zustände löschen
	// 	musicQueue.init("", false, exts); // leere Queue
	// 	currentTrackIndex = 0;
	// 	stopRequested.store(false);
	// 	isRunning.store(false);

	// 	USBUnmountStick(MUSIK_ORDNER);
	// 	usb_connected = false;
	// 	// 3. Visualisierung zurücksetzen
	// 	SetVisObjData(OBJ_TITEL, "reset", 6);
	// }

public:
	~Application()
	{
		if (isRunning.load())
		{
			stopRequested.store(true);
			usleep(200000);
		}
	}

	Application() : gsEth()
	{
		exts = {".wav", ".mp3"};
	}

	void Cycle(uint32_t evtc, tUserCEvt *evtv)
	{
		for (tUserCEvt *e = evtv; evtc--; ++e)
		{
			// if (bt_reset(e))
			// {
			// 	reset_action();
			// }
			// else
			if (usb_on(e))
			{
				currentTrackIndex = 0; // Reset Playlist bei neuem Stick
				// USB verbunden: Verzeichnis neu einlesen
				int count = musicQueue.init(MUSIK_ORDNER, RECURSIVE, exts);
				if (count > 0)
					SetVisObjData(OBJ_TITEL, "USB bereit", 11);
				else
					SetVisObjData(OBJ_TITEL, "Keine Musik", 12);
			}
			else if (usb_off(e))
			{
				usb_off_action();
			}
			// else if (bt_start(e))
			// {
			// 	if (!isRunning.load()) // Nur starten, wenn noch NICHT läuft
			// 	{
			// 		stopRequested.store(false); // Stop-Flag resetten
			// 		isRunning.store(true);		// Als laufend markieren
			// 		gsThreadCreate(&t1, NULL, Application::threadEntry, this);
			// 	}
			// }
			else if (bt_next(e))
			{
				// Next gedrückt:
				// Wir brechen den aktuellen Thread ab.
				// Im nächsten Cycle greift die "Auto-Play" Logik unten
				// und startet den Thread neu (mit inkrementiertem Index).
				if (isRunning.load())
					stopRequested.store(true);
			}
			else
				this->gsEth.Evt(e, OBJ_IP, -1, "eth0");
		}
		this->gsEth.Cycle(true, true, 23000);

		// Auto-Play & Next-Logik (State Machine)
		// Wenn USB da ist UND die Queue voll ist UND gerade KEIN Thread läuft: Starte Playback.
		if (usb_connected && !musicQueue.empty() && !isRunning.load())
		{
			// if (access(MUSIK_ORDNER, R_OK | X_OK) != 0) // Verzeichnis weg → sofort als „weg“ behandeln
			// {
			// 	usb_connected = false;
			// 	usb_off_action();
			// }
			// else
			// {
			stopRequested.store(false); // Stop-Flag resetten
			isRunning.store(true);		// Als laufend markieren
			gsThreadCreate(&t1, NULL, Application::threadEntry, this);
			// }
		}
	}

	void MStoRGB(uint32_t ms, uint32_t cycleTime, float *r, float *g, float *b)
	{
		/* Hue aus der relativen Position im Zyklus (0 … 359) */
		uint32_t pos = ms % cycleTime;			  /* 0 … cycleTime‑1 */
		int hue = (int)(pos * 360UL / cycleTime); /* 0 … 359 */

		/* Sektor 0‑5 und Fraktion innerhalb des Sektors (0 … 1) */
		uint32_t sector = hue / 60;					/* 0‑5 */
		float fraction = (float)(hue % 60) / 60.0f; /* 0 … 1 */

		float q = 1 - fraction;
		float t = fraction;

		switch (sector)
		{
		case 0:
			*r = 1.0f;
			*g = t;
			*b = 0.0f;
			break;
		case 1:
			*r = q;
			*g = 1.0f;
			*b = 0.0f;
			break;
		case 2:
			*r = 0.0f;
			*g = 1.0f;
			*b = t;
			break;
		case 3:
			*r = 0.0f;
			*g = q;
			*b = 1.0f;
			break;
		case 4:
			*r = t;
			*g = 0.0f;
			*b = 1.0f;
			break;
		default: /*5*/
			*r = 1.0f;
			*g = 0.0f;
			*b = q;
			break;
		}
	}
	void Timer()
	{
#define MAX_RGB 0xffff
		const int32_t cycleTime = 5000; /* 5 s für einen vollen Regenbogen */
		float r, g, b;
		MStoRGB(GetMSTick(), cycleTime, &r, &g, &b);
		SetStatusLedColor(0, r * MAX_RGB, g * MAX_RGB, b * MAX_RGB);
	}
};

static Application *app = nullptr;
extern "C"
{
	int UserCPPInit(uint32_t initFlags)
	{
		app = new Application();
		return 20; // x10ms
	}

	void UserCPPCycle(uint32_t evtc, tUserCEvt *evtv)
	{
		if (app)
			app->Cycle(evtc, evtv);
	}

	void UserCPPTimer(void)
	{
		if (app)
			app->Timer();
	}

	void UserCPPDeInit(void)
	{
		delete app;
		app = nullptr;
	}
}
