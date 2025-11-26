#include <cctype>
#include <UserCAPI.h>
#include <gsSocket.h>
#include <gsSocketTCPServer.h>

class GsEth
{
private:
	tGsSocketTcpServer serverHdl;
	tGsSocketIntfInfo socketInfo;
	bool online;

	// Send formatted string over TCP with timestamp
	void SendStringOverIP(const char *fmt, ...)
	{
		char user_message[512];
		va_list args;

		va_start(args, fmt);
		int user_len = vsnprintf(user_message, sizeof(user_message), fmt, args);
		va_end(args);

		// Check for errors or truncation
		if (user_len < 0 || (size_t)user_len >= sizeof(user_message))
		{
			return;
		}

		// Get current time
		tSysTime Time;
		RTCGetTime(&Time);

		// Create timestamped output (max 512 + 24 bytes for timestamp)
		char stringout[540];
		int final_len = snprintf(
			stringout,
			sizeof(stringout),
			"%02d:%02d:%02d <-- %s\r\n",
			Time.Hours,
			Time.Minutes,
			Time.Seconds,
			user_message);

		// Validate and send
		if (serverHdl && gsSocketTcpServerIsConnected(serverHdl))
		{
			if (final_len > 0 && (size_t)final_len < sizeof(stringout))
			{
				gsSocketTcpServerWrite(serverHdl, stringout, (size_t)final_len);
			}
		}
	}

	// Convert string to uppercase in-place
	void StrToUpperCase(char *str)
	{
		if (!str)
			return;

		while (*str)
		{
			*str = toupper((unsigned char)*str);
			str++;
		}
	}

	// Convert seconds to time structure
	void convertSeconds(int seconds, tSysTime *mTime)
	{
		if (!mTime)
			return;

		mTime->Hours = seconds / 3600;
		mTime->Minutes = (seconds % 3600) / 60;
		mTime->Seconds = (seconds % 3600) % 60;
	}

public:
	GsEth() : serverHdl(nullptr), socketInfo({}), online(false) {}

	// Handle network events
	void Evt(tUserCEvt *evtv, const int32_t objIP, const int32_t objMAC, const char *iface)
	{
		if (!evtv || !iface)
			return;

		if (CEVT_NETWORK == evtv->Type &&
			CEVT_NETWORK_STATE_CONFIG == evtv->Content.mNetwork.mState)
		{
			if (!online)
			{
				if (0 == gsSocketGetIntfInfo(iface, &socketInfo))
				{
					int32_t len = strlen(socketInfo.mIpAddr);
					online = (len > 5);
					if (online)
					{
						if (objIP != -1)
							SetVisObjData(objIP, socketInfo.mIpAddr, len + 1);
						if (objMAC != -1)
							SetVisObjData(objMAC, socketInfo.mHwAddr,
										  strlen(socketInfo.mHwAddr) + 1);
					}
				}
			}
		}
	}

	// Main cycle processing
	void Cycle(const bool allow_Update, const bool allow_FuncOverIP, const int32_t port)
	{
		// Create server if not yet created
		if (!serverHdl)
		{
			serverHdl = gsSocketTcpServerCreate(port, 0);
			return;
		}

		// Read from server
		char buf[256] = {0};
		int32_t bytes_read = gsSocketTcpServerRead(serverHdl, buf, sizeof(buf) - 1);

		if (bytes_read <= 0)
			return;

		// Ensure null-termination
		buf[bytes_read] = '\0';

		// Handle update request
		if (allow_Update && strstr(buf, "updaterequest"))
		{
			SetProjectUpdate(GS_PRJ_UPDATE_TRIGGER);
			return;
		}

		// Check if we should process commands
		if (!allow_FuncOverIP || !gsSocketTcpServerIsConnected(serverHdl) || bytes_read < 3)
			return;

		// Parse command
		char command[32] = {0};
		if (sscanf(buf, "%31s", command) != 1)
			return;

		StrToUpperCase(command);

		// Process commands
		if (strcmp(command, "HELP") == 0)
		{
			const char *help =
				"Available commands:\r\n"
				"BOOTLOADER                  - Start device in bootloader mode\r\n"
				"GETBSZ <Nr>                 - Get hour counter value\r\n"
				"SETBSZ <Nr> <hhhh:mm:ss>    - Set hour counter\r\n"
				"GETDATE                     - Get current date\r\n"
				"SETDATE <dd.mm.yy>          - Set date\r\n"
				"GETINTFINFO <Interface>     - Get interface settings\r\n"
				"GETTIME                     - Get current time\r\n"
				"SETTIME <hh:mm:ss>          - Set time\r\n"
				"GETVAR <Handle> - <Handle>  - Get variable values (range)\r\n"
				"GETVAR <Handle>             - Get variable value\r\n"
				"SETVAR <Handle> <Value>     - Set variable value\r\n"
				"MASK <Nr>                   - Switch mask\r\n"
				"REBOOT                      - Reboot device\r\n";
			SendStringOverIP("%s", help);
		}
		else if (strcmp(command, "BOOTLOADER") == 0 || strcmp(command, "REBOOT") == 0)
		{
			SendStringOverIP("Rebooting device...");
			PowerCommand(GS_POWER_CMD_REBOOT, 0);
		}
		else if (strcmp(command, "GETDATE") == 0)
		{
			tSysDate mDate;
			RTCGetDate(&mDate);
			SendStringOverIP("Date: %02d.%02d.%02d", mDate.Day, mDate.Month, mDate.Year);
		}
		else if (strcmp(command, "GETTIME") == 0)
		{
			tSysTime mTime;
			RTCGetTime(&mTime);
			SendStringOverIP("Time: %02d:%02d:%02d", mTime.Hours, mTime.Minutes, mTime.Seconds);
		}
		else if (strcmp(command, "MASK") == 0)
		{
			uint32_t maskNr;
			if (sscanf(buf, "%*s %u", &maskNr) == 1)
			{
				PrioMaskOn(maskNr);
				SendStringOverIP("Switched to mask %u", maskNr);
			}
			else
			{
				SendStringOverIP("Error: Missing argument for MASK");
			}
		}
		else if (strcmp(command, "GETINTFINFO") == 0)
		{
			char iface[32] = {0};
			if (sscanf(buf, "%*s %31s", iface) == 1)
			{
				tGsSocketIntfInfo tempInfo;
				if (gsSocketGetIntfInfo(iface, &tempInfo) == 0)
				{
					SendStringOverIP("\"%s\" IP: %s", iface, tempInfo.mIpAddr);
					SendStringOverIP("\"%s\" MAC: %s", iface, tempInfo.mHwAddr);
					SendStringOverIP("\"%s\" STATUS: %s", iface, tempInfo.mStatus);
					SendStringOverIP("\"%s\" MEDIA: %s", iface, tempInfo.mMedia);
				}
				else
				{
					SendStringOverIP("Error: Interface not found or not initialized: %s", iface);
				}
			}
			else
			{
				SendStringOverIP("Error: Missing interface name for GETINTFINFO");
			}
		}
		else if (strcmp(command, "SETTIME") == 0)
		{
			tSysTime mTime;
			if (sscanf(buf, "%*s %hhu:%hhu:%hhu", &mTime.Hours, &mTime.Minutes, &mTime.Seconds) == 3)
			{
				if (mTime.Hours < 24 && mTime.Minutes < 60 && mTime.Seconds < 60)
				{
					RTCSetTime(&mTime);
					SendStringOverIP("Time set: %02d:%02d:%02d", mTime.Hours, mTime.Minutes, mTime.Seconds);
				}
				else
				{
					SendStringOverIP("Error: Invalid time value");
				}
			}
			else
			{
				SendStringOverIP("Error: Invalid time format. Expected: hh:mm:ss");
			}
		}
		else if (strcmp(command, "SETDATE") == 0)
		{
			tSysDate mDate;
			if (sscanf(buf, "%*s %hhu.%hhu.%hhu", &mDate.Day, &mDate.Month, &mDate.Year) == 3)
			{
				if (mDate.Day >= 1 && mDate.Day <= 31 &&
					mDate.Month >= 1 && mDate.Month <= 12 &&
					mDate.Year <= 99)
				{
					RTCSetDate(&mDate);
					SendStringOverIP("Date set: %02d.%02d.%02d", mDate.Day, mDate.Month, mDate.Year);
				}
				else
				{
					SendStringOverIP("Error: Invalid date value");
				}
			}
			else
			{
				SendStringOverIP("Error: Invalid date format. Expected: dd.mm.yy");
			}
		}
		else if (strcmp(command, "GETBSZ") == 0)
		{
			uint32_t bsz;
			if (sscanf(buf, "%*s %u", &bsz) == 1)
			{
				if (bsz < 32)
				{
					tSysTime mTime;
					convertSeconds(HourCounterGet(bsz), &mTime);
					SendStringOverIP("Hour counter %u: %u:%02u:%02u",
									 bsz, mTime.Hours, mTime.Minutes, mTime.Seconds);
				}
				else
				{
					SendStringOverIP("Error: Invalid hour counter number: %u (valid: 0-31)", bsz);
				}
			}
			else
			{
				SendStringOverIP("Error: Missing hour counter number");
			}
		}
		else if (strcmp(command, "SETBSZ") == 0)
		{
			uint32_t bsz, hours;
			uint8_t minutes, seconds;
			if (sscanf(buf, "%*s %u %u:%hhu:%hhu", &bsz, &hours, &minutes, &seconds) == 4)
			{
				if (bsz > 1 && bsz < 32)
				{
					if (minutes < 60 && seconds < 60)
					{
						uint32_t total_seconds = hours * 3600 + minutes * 60 + seconds;
						HourCounterSet(bsz, total_seconds);
						SendStringOverIP("Hour counter %u successfully set", bsz);
					}
					else
					{
						SendStringOverIP("Error: Invalid time value");
					}
				}
				else
				{
					SendStringOverIP("Error: Hour counter %u cannot be set (must be > 1 and < 32)", bsz);
				}
			}
			else
			{
				SendStringOverIP("Error: Invalid format. Expected: SETBSZ <Nr> <hhhh:mm:ss>");
			}
		}
		else if (strcmp(command, "GETVAR") == 0)
		{
			uint32_t hdl1, hdl2;
			char dash;

			// Check for range format: GETVAR <hdl1> - <hdl2>
			if (sscanf(buf, "%*s %u %c %u", &hdl1, &dash, &hdl2) == 3 && dash == '-')
			{
				if (hdl1 > hdl2)
				{
					SendStringOverIP("Error: Invalid range (start > end)");
				}
				else if (hdl2 - hdl1 > 100)
				{
					SendStringOverIP("Error: Range too large (max 100 variables)");
				}
				else
				{
					for (uint32_t i = hdl1; i <= hdl2; i++)
					{
						if (GetVarIndex(i) != -1)
						{
							SendStringOverIP("Handle %u: %d", i, GetVar(i));
						}
						else
						{
							SendStringOverIP("Handle %u: not found", i);
						}
					}
				}
			}
			// Single variable format: GETVAR <hdl1>
			else if (sscanf(buf, "%*s %u", &hdl1) == 1)
			{
				if (GetVarIndex(hdl1) != -1)
				{
					SendStringOverIP("Handle %u: %d", hdl1, GetVar(hdl1));
				}
				else
				{
					SendStringOverIP("Handle %u: not found", hdl1);
				}
			}
			else
			{
				SendStringOverIP("Error: Invalid arguments for GETVAR");
			}
		}
		else if (strcmp(command, "SETVAR") == 0)
		{
			uint32_t hdl;
			int32_t val;
			if (sscanf(buf, "%*s %u %d", &hdl, &val) == 2)
			{
				if (GetVarIndex(hdl) != -1)
				{
					SetVar(hdl, val);
					SendStringOverIP("Handle %u set to %d", hdl, val);
				}
				else
				{
					SendStringOverIP("Handle %u: not found", hdl);
				}
			}
			else
			{
				SendStringOverIP("Error: Invalid format. Expected: SETVAR <Handle> <Value>");
			}
		}
		else
		{
			SendStringOverIP("Unknown command: %s\r\nType 'HELP' for command list", command);
		}
	}
};
