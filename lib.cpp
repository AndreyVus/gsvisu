#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <ranges>
#include <unordered_map>
#include <sstream>
#include <string>
#include <tuple>
#include <optional>
#include <vector>

#include <gsSocket.h>
#include <gsSocketTCPServer.h>

template <typename T>
bool ImRang(const T& val, const T& a, const T& b) {
	return (a <= val && val <= b);
}

class R_TRIG
{
private:
	bool prev_clk; // vorheriger Taktzustand

public:
	R_TRIG() : prev_clk(false), Q(false) {} // Konstruktor zur Initialisierung
	bool Q; // Ausgangszustand
	bool operator()(bool new_clk)
	{
		Q = new_clk && !prev_clk; // steigende Flanke
		prev_clk = new_clk;		  // Speichern des aktuellen Taktsignals
		return Q;
	}
};

class F_TRIG
{
private:
	bool prev_clk; // vorheriger Taktzustand

public:
	bool Q; // Ausgangszustand
	F_TRIG() : prev_clk(false), Q(false) {} // Konstruktor zur Initialisierung
	bool operator()(bool new_clk)
	{
		Q = !new_clk && prev_clk; // fallende Flanke
		prev_clk = new_clk;		  // Speichern des aktuellen Taktsignals
		return Q;
	}
};

class TON
{
private:
    uint32_t startTime; // Startzeit des Timers in ms

public:
	bool Q; // Ausgangszustand
    TON() : startTime(0) {} // Konstruktor zum Initialisieren

    bool operator()(bool in, uint32_t presetTime)
    {
        uint32_t currentTime = GetMSTick();
        if(in){
            if(startTime == 0) 
                startTime = currentTime;// Timer starten, wenn er noch nicht läuft
            Q = (currentTime - startTime >= presetTime);// Überprüfe, ob die voreingestellte Zeit erreicht ist
        } else {
            startTime = 0;// Timer zurücksetzen
            Q = false;
        }
        return Q;
    }
};

class TOF
{
private:
    uint32_t startTime; // Startzeit des Timers (in Millisekunden)

public:
	bool Q; // Ausgangszustand
    TOF() : startTime(0) {} // Konstruktor zum Initialisieren

    bool operator()(bool in, uint32_t presetTime)
    {
        uint32_t currentTime = GetMSTick();
        if(in)
        {
            startTime = 0;
            Q = true; // Ausgang bleibt gesetzt, solange Eingang aktiv ist
        }
        else
        {
            if(currentTime == 0)
                startTime = currentTime; // Startzeit nur einmal setzen
            Q = (currentTime - startTime < presetTime); // Ausgang bleibt solange aktiv, bis die Zeit abgelaufen ist
        }
        return Q;
    }
};

class Smoother
{
private:
	uint32_t window_size_;
	std::deque<float> data_;

public:
	Smoother(uint32_t ws) : window_size_(ws){}
	float operator()(float value)
	{
		data_.push_back(value);
		if(data_.size() > window_size_)
			data_.pop_front();
		// return calculateAverage();
		if(data_.empty())
			return 0.0; // Oder NaN, um anzuzeigen, dass kein Mittelwert berechnet werden kann
		float sum = std::accumulate(data_.begin(), data_.end(), 0.0);
		return sum / data_.size();
	}
};
// Smoother mw10;//declare
// mw10(10); //init
// float average = mw10(value); //use
class Glattung
{
private:
	float f, y;

public:
	Glattung(float f_) : f(f_), y(0) {}
	float operator()(float x)
	{
		y += f * (x - y);
		return y;
	}
};

template <typename... Args>
void printfo(uint32_t o, const char *format, Args &&...args)
{
	int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...);
	if(ImRang(size, 1, 255))
	{
		std::string str(size + 1, '\0');
		std::snprintf(&str[0], size + 1, format, std::forward<Args>(args)...);
		SetVisObjData(o, str.c_str(), str.length() + 1);
	}
}
// Eigene clamp-Funktion (funktioniert in C++11 und neuer)
template <typename T>
const T& clamp(const T& value, const T& low, const T& high) {
  return std::max(low, std::min(value, high));
}
int32_t displayBacklicht = 1000;
void setHelligkeit(int32_t neuH)
{
	displayBacklicht = clamp(neuH, 0, 1000);
	SetDisplayBacklight(0, displayBacklicht);
}
uint32_t ledSin(uint32_t period)
{
	const float pi2 = 6.283185f;
	const float MaxLED2 = 32767.5f;
	float value = sinf(pi2 * fmodf(GetMSTick(), period) / period) + 1.0f;
	return value * MaxLED2;
}

int32_t getContent(uint32_t ec, tUserCEvt *ev, uint32_t type, uint32_t source, tCEvtContent *content, int32_t foundMax)
{
	int32_t found = 0;
	for(uint32_t o = 0; o < ec; o++)
		if(ev[o].Type == type && ev[o].Source == source && found < foundMax)
			content[found++] = ev[o].Content; // Store content in array
	return found;
}

class KeyMenu
{
private:
	bool pressed;
	R_TRIG click, dimm_on;
	F_TRIG release, dimm_off;
	TON dimm_aktiv;
	int32_t renc_alt;
	tCEvtContent menuCo[1];

public:
	int32_t lenc, renc, Nr; // Tastennummer
	KeyMenu() : renc_alt(0) {}

	int32_t operator()(uint32_t ec, tUserCEvt *ev, uint32_t TastenAnzahl, int32_t TasteInhalt, int32_t dimmTimer)
	{
		// Key Handling
		lenc = GetVar(HDL_SYS_ENC_LEFT);
		renc = GetVar(HDL_SYS_ENC_RIGHT);
		Nr = IsAnyKeyDown();
		pressed = (Nr != 0);
		click(pressed);
		release(pressed);
		if(pressed)
		{
			if(IsKeyDown(Nr) < 1000) // steigende Helligkeit
				SetKeyBacklight(Nr, 1 | GS_KEY_BACKLIGHT_BRIGHTNESS(IsKeyDown(Nr) / 10));
			else
				SetKeyBacklightColor(Nr, ledSin(300), ledSin(400), ledSin(500));
		}
		else if(release.Q)
			for(uint32_t o = 1; o <= TastenAnzahl; o++)
			{
				SetKeyBacklightColor(o, 0xffff, 0xffff, 0xffff);
				SetKeyBacklight(o, 1 | GS_KEY_BACKLIGHT_BRIGHTNESS(100));
			}
		// Display Dimmen
		if(dimmTimer)
		{
			dimm_on(dimm_aktiv(!(click.Q || release.Q ||  
				getContent(ec, ev, CEVT_TOUCH, CEVT_SOURCE_TOUCH, menuCo, GS_ARRAYELEMENTS(menuCo)) > 0 || 
				renc != renc_alt), dimmTimer)
			);
			dimm_off(dimm_aktiv.Q);
			renc_alt = renc;
			if(dimm_on.Q)
				SetDisplayBacklight(0, 0);
			else if(dimm_off.Q)
				SetDisplayBacklight(0, displayBacklicht);
		}
		// Tastenereignisse
		if(click.Q)
		{
			if(TasteInhalt && Nr == TasteInhalt)
				IsInfoContainerOn(0) ? InfoContainerOff(0) : InfoContainerOn(0);
			return Nr;
		}
		// Menu Handling
		for(int32_t o = getContent(ec, ev, CEVT_MENU_INDEX, CEVT_SOURCE_MENU, menuCo, GS_ARRAYELEMENTS(menuCo)) - 1; o >= 0; o--)
			return menuCo[o].mMenuIndex.ObjID;
		return -1;
	}
};
KeyMenu keymenu; //(uint32_t ec, tUserCEvt *ev, uint32_t TastenAnzahl, int32_t TasteInhalt, int32_t dimmTimer)
int32_t menu;

/*	Function      : void convertSeconds(int seconds, tSysTime* mTime)
	Description   : converts the given seconds into hour:minutes:seconds and write it into the given tSysTime structure
	Returnvalues  : none
*****************************************************************************/
void convertSeconds(int seconds, tSysTime *mTime)
{
	mTime->Hours = seconds / 3600;
	mTime->Minutes = (seconds % 3600) / 60;
	mTime->Seconds = seconds % 60;
}

class IPFunctions
{
private:
	tGsSocketTcpServer hdl_server; // ReturnValue HDL of the initialised server
	bool connected;
	template <typename... Args>
	void SendStringOverIP(const char *fmt, Args &&...args)
	{
		if(hdl_server == nullptr || !gsSocketTcpServerIsConnected(hdl_server))
			return;

		int size = 1 + snprintf(nullptr, 0, fmt, std::forward<Args>(args)...); // Calculate required buffer size (+1 for null terminator)
		if(size <= 1)
			return;														// Error in formatting
		std::unique_ptr<char[]> buffer(new char[size]);					// Create buffer with calculated size
		snprintf(buffer.get(), size, fmt, std::forward<Args>(args)...); // Format the string
		std::string str1 = std::string(buffer.get(), buffer.get() + size);

		tSysTime Time;
		RTCGetTime(&Time);
		std::ostringstream oss;
		oss << std::setw(2) << std::setfill('0') << Time.Hours << ':';
		oss << std::setw(2) << std::setfill('0') << Time.Minutes << ':';
		oss << std::setw(2) << std::setfill('0') << Time.Seconds << " <-- " << str1 << std::endl;
		std::string str2 = oss.str();

		gsSocketTcpServerWrite(hdl_server, str2.data(), str2.size());
	}

	// Command implementations
	void fREBOOT(const std::string &arg1, const std::string &arg2)
	{
		SendStringOverIP("Reboot");
		PowerCommand(GS_POWER_CMD_REBOOT, 0);
	}

	void fGETBSZ(const std::string &arg1, const std::string &arg2)
	{
		try
		{
			uint32_t bszNum = std::stoul(arg1);
			if(ImRang(bszNum, 0u, 31u))
			{
				tSysTime mTime;
				convertSeconds(HourCounterGet(bszNum), &mTime);
				SendStringOverIP("BSZ %02hhu:%02hhu:%02hhu", bszNum, mTime.Hours, mTime.Minutes, mTime.Seconds);
			}
			else
				SendStringOverIP("Ungültige BSZ angegeben.");
		}
		catch (const std::exception &)
		{
			SendStringOverIP("Ungültiger BSZ Parameter.");
		}
	}

	void fSETBSZ(const std::string &arg1, const std::string &arg2)
	{
		try
		{
			uint32_t bszNum = std::stoul(arg1);
			if(!ImRang(bszNum, 2u, 31u))
			{
				SendStringOverIP("BSZ %d kann nicht gesetzt werden!", bszNum);
				return;
			}

			tSysTime mTime;
			std::istringstream ss(arg2);
			char delimiter;
			if(ss >> mTime.Hours >> delimiter >> mTime.Minutes >> delimiter >> mTime.Seconds)
			{
				if(ImRang((int)mTime.Hours, 0, 23) && ImRang((int)mTime.Minutes, 0, 59) && ImRang((int)mTime.Seconds, 0, 59))
				{
					RTCSetTime(&mTime);
					SendStringOverIP("BSZ %d gesetzt: %02hhu:%02hhu:%02hhu", bszNum,
									 mTime.Hours, mTime.Minutes, mTime.Seconds);
				}
				else
					SendStringOverIP("SETBSZ, ungültige Uhrzeit angegeben: %02hhu:%02hhu:%02hhu\n"
									 "Zeitangabe muss im Format hh:mm:ss übergeben werden!",
									 mTime.Hours, mTime.Minutes, mTime.Seconds);
			}
			else
				SendStringOverIP("SETBSZ, ungültiges Zeitformat. Verwenden Sie hh:mm:ss");
		}
		catch (const std::exception &)
		{
			SendStringOverIP("SETBSZ, ungültige Parameter.");
		}
	}

	void fGETDATE(const std::string &arg1, const std::string &arg2)
	{
		tSysDate mDate;
		RTCGetDate(&mDate);
		SendStringOverIP("Datum: %02hhu.%02hhu.%02hhu", mDate.Day, mDate.Month, mDate.Year);
	}

	void fSETDATE(const std::string &arg1, const std::string &arg2)
	{
		tSysDate mDate;
		std::istringstream ss(arg1);
		char delimiter;
		if(!(ss >> mDate.Day >> delimiter >> mDate.Month >> delimiter >> mDate.Year))
		{
			SendStringOverIP("Ungültiges Datumsformat. Verwenden Sie dd.mm.yy");
			return;
		}

		if(ImRang((int)mDate.Day, 1, 31) && ImRang((int)mDate.Month, 1, 12) && ImRang((int)mDate.Year, 0, 99))
		{
			RTCSetDate(&mDate);
			SendStringOverIP("Datum gesetzt: %02hhu.%02hhu.%02hhu",
							 mDate.Day, mDate.Month, mDate.Year);
		}
		else
			SendStringOverIP("Ungültiges Datum angegeben: %02hhu.%02hhu.%02hhu\n"
							 "Datumsangabe muss im Format dd.mm.yy übergeben werden!",
							 mDate.Day, mDate.Month, mDate.Year);
	}

	void fGETINTFINFO(const std::string &arg1, const std::string &arg2)
	{
		if(arg1.empty())
		{
			SendStringOverIP("Bitte Schnittstelle angeben.");
			return;
		}

		tGsSocketIntfInfo tempInfo;
		if(!gsSocketGetIntfInfo(arg1.c_str(), &tempInfo))
		{
			SendStringOverIP("%s:\nIP: %s\nMAC: %s\nSTATUS: %s\nMEDIA: %s",
							 arg1.c_str(), tempInfo.mIpAddr, tempInfo.mHwAddr,
							 tempInfo.mStatus, tempInfo.mMedia);
		}
		else
			SendStringOverIP("Schnittstelle %s nicht vorhanden oder nicht initialisiert.",
							 arg1.c_str());
	}

	void fGETTIME(const std::string &arg1, const std::string &arg2)
	{
		tSysTime mTime;
		RTCGetTime(&mTime);
		SendStringOverIP("Uhrzeit: %02hhu:%02hhu:%02hhu",
						 mTime.Hours, mTime.Minutes, mTime.Seconds);
	}

	void fSETTIME(const std::string &arg1, const std::string &arg2)
	{
		tSysTime mTime;
		std::istringstream ss(arg1);
		char delimiter;
		if(!(ss >> mTime.Hours >> delimiter >> mTime.Minutes >> delimiter >> mTime.Seconds))
		{
			SendStringOverIP("SetTime, ungültiges Zeitformat. Verwenden Sie hh:mm:ss");
			return;
		}

		if(ImRang((int)mTime.Hours, 0, 23) && ImRang((int)mTime.Minutes, 0, 59) && ImRang((int)mTime.Seconds, 0, 59))
		{
			RTCSetTime(&mTime);
			SendStringOverIP("Uhrzeit gesetzt: %02hhu:%02hhu:%02hhu",
							 mTime.Hours, mTime.Minutes, mTime.Seconds);
		}
		else
			SendStringOverIP("SetTime, ungültige Uhrzeit angegeben: %02hhu:%02hhu:%02hhu\n"
							 "Zeitangabe muss im Format hh:mm:ss übergeben werden!",
							 mTime.Hours, mTime.Minutes, mTime.Seconds);
	}

	void fGETVAR(const std::string &arg1, const std::string &arg2)
	{
		try
		{
			uint32_t handle1 = std::stoul(arg1);
			uint32_t handle2 = arg2.empty() ? handle1 : std::stoul(arg2);

			for(uint32_t i = handle1;; i++)
			{
				if(-1 == GetVarIndex(i))
				{
					SendStringOverIP("GETVAR, Handle %d nicht vorhanden", i);
					return;
				}
				SendStringOverIP("Handle %d, Wert: %d", i, GetVar(i));
				if(i >= handle2)
					break;
			}
		}
		catch (const std::exception &)
		{
			SendStringOverIP("GETVAR, ungültige Handle Parameter.");
		}
	}

	void fSETVAR(const std::string &arg1, const std::string &arg2)
	{
		if(arg2.empty())
		{
			SendStringOverIP("SetVar, Value nicht vorhanden.");
			return;
		}

		try
		{
			uint32_t handle = std::stoul(arg1);
			uint32_t value = std::stoul(arg2);

			if(-1 == GetVarIndex(handle))
			{
				SendStringOverIP("SetVar, Handle %d nicht vorhanden.", handle);
				return;
			}

			SetVar(handle, value);
			SendStringOverIP("Handle %d auf Wert: %d gesetzt.", handle, value);
		}
		catch (const std::exception &)
		{
			SendStringOverIP("SetVar, ungültige Parameter.");
		}
	}

	void fMASK(const std::string &arg1, const std::string &arg2)
	{
		try
		{
			uint32_t mask = std::stoul(arg1);
			PrioMaskOn(mask);
		}
		catch (const std::exception &)
		{
			SendStringOverIP("Ungültiger Masken-Parameter.");
		}
	}

public:
	bool allow_FuncOverIP;		  // activate to use defined functions over the interface like change masks
	bool allow_Update;			  // activate project update function
	int32_t port;				  // use 23000 for project update
	std::string iface;			  // "ETH0" or "wlan0"
	tGsSocketIntfInfo socketInfo; // ReturnValue filled structure of the interface information

	// initialisation of the class
	IPFunctions(bool funcOverIP = true, bool update = true, int32_t serverPort = 23000, const std::string &interface = "eth0")
	{
		allow_FuncOverIP = funcOverIP;
		allow_Update = update;
		port = serverPort;
		iface = interface;
		connected = false;
		hdl_server = nullptr;
		std::memset(&socketInfo, 0, sizeof(socketInfo));
	}

	std::tuple<std::string, std::string, std::string> parseLine(const std::string &line)
	{
		std::istringstream stream(line);
		std::string command, arg1, arg2;
		if(!(stream >> command))
			return {};
		if(!(stream >> arg1))
			arg1 = "";
		if(!(stream >> arg2))
			arg2 = "";
		return std::make_tuple(command, arg1, arg2);
	}

	void ProcessFunctions()
	{
		if(!connected && !gsSocketGetIntfInfo(iface.data(), &socketInfo))
		{
			connected = (5 < strlen(socketInfo.mIpAddr));
		}
		else if(hdl_server)
		{
			std::vector<char> buf(1024); // Initialize buf with size 1024
			if(gsSocketTcpServerRead(hdl_server, (void *)buf.data(), buf.size()))
			{
				if(allow_Update && strstr(buf.data(), "updaterequest"))
				{
					SetProjectUpdate(GS_PRJ_UPDATE_TRIGGER);
				}
				else if(allow_FuncOverIP)
				{
					for(char &c : buf)
						c = toupper(c);
					auto result = parseLine(std::string(buf.data(), buf.size()));
					if(result == std::make_tuple("", "", ""))
						return;
					std::string command, arg1, arg2;
					std::tie(command, arg1, arg2) = result;
					std::unordered_map<std::string, std::function<void(const std::string &, const std::string &)>> commands = {
						{"BOOTLOADER", std::bind(&IPFunctions::fREBOOT, this, std::placeholders::_1, std::placeholders::_2)},
						{"REBOOT", std::bind(&IPFunctions::fREBOOT, this, std::placeholders::_1, std::placeholders::_2)},
						{"GETBSZ", std::bind(&IPFunctions::fGETBSZ, this, std::placeholders::_1, std::placeholders::_2)},
						{"SETBSZ", std::bind(&IPFunctions::fSETBSZ, this, std::placeholders::_1, std::placeholders::_2)},
						{"GETDATE", std::bind(&IPFunctions::fGETDATE, this, std::placeholders::_1, std::placeholders::_2)},
						{"SETDATE", std::bind(&IPFunctions::fSETDATE, this, std::placeholders::_1, std::placeholders::_2)},
						{"GETINTFINFO", std::bind(&IPFunctions::fGETINTFINFO, this, std::placeholders::_1, std::placeholders::_2)},
						{"GETTIME", std::bind(&IPFunctions::fGETTIME, this, std::placeholders::_1, std::placeholders::_2)},
						{"SETTIME", std::bind(&IPFunctions::fSETTIME, this, std::placeholders::_1, std::placeholders::_2)},
						{"GETVAR", std::bind(&IPFunctions::fGETVAR, this, std::placeholders::_1, std::placeholders::_2)},
						{"SETVAR", std::bind(&IPFunctions::fSETVAR, this, std::placeholders::_1, std::placeholders::_2)},
						{"MASK", std::bind(&IPFunctions::fMASK, this, std::placeholders::_1, std::placeholders::_2)},
					};
					auto it = commands.find(command);
					if(it != commands.end())
					{
						it->second(arg1, arg2);
					}
					else
					{
						SendStringOverIP("Unbekannter Befehl: %s\n",
										 "BOOTLOADER                  - Geraet in den Bootloader starten\n"
										 "GETBSZ <Nr>                 - Wert des BSZ ausgeben\n"
										 "GETDATE                     - Aktuelles Datum ausgeben\n"
										 "GETINTFINFO <Schnittstelle> - Settings der aktuell verwendeten Schnittstelle ausgeben\n"
										 "GETTIME                     - Aktuelle Uhrzeit ausgeben\n"
										 "GETVAR <Handle>             - Handle Wert wird ausgegeben\n"
										 "GETVAR <Handle1 Handle2>    - von Handle1 bis Handle2 werden die Werte ausgegeben\n"
										 "MASK <Nr>                   - Masken wechseln\n"
										 "REBOOT                      - Geraet Neustart\n"
										 "SETBSZ <Nr> <hhhh:mm:ss>    - Setze Betriebsstundenzähler\n"
										 "SETDATE <dd.mm.yy>          - Datum setzen\n"
										 "SETTIME <hh:mm:ss>          - Uhrzeit setzen\n"
										 "SETVAR <Handle> <Value>     - Handle auf Wert setzen\n",
										 command.c_str());
					}
				}
			}
		}
		else
			hdl_server = gsSocketTcpServerCreate(port, 0);
	}
	void DeInit()
	{
		connected = false;
		if(NULL != hdl_server)
		{
			gsSocketTcpServerDestroy(hdl_server);
			hdl_server = nullptr;
		}
	}
};
// IPFunctions ipFunctions;

class Maske
{
public:
	virtual ~Maske() = default;
	// virtual void init() {}
	virtual void cycle(uint32_t evtc, tUserCEvt *evtv) {}
	virtual void timer() {}
};
