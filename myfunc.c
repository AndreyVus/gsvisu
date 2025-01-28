#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <UserCEvents.h>
#include <UserCAPI.h>
#include "gstovisu.h"

#define SetBit(A, k) (A |= (1 << k))
#define ClearBit(A, k) (A &= ~(1 << k))
#define TestBit(A, k) (A & (1 << k))
#define Min(a, b) ((a <= b) ? (a) : (b))
#define Max(a, b) ((a >= b) ? (a) : (b))
#define sign(x) (((x) > 0) - ((x) < 0))

// #define AI_SpannungIGN 12
// #define AI_LICHTSENSOR 13
#define AI_Spannung 17
#define AI_Temperatur 18

#define sysHourTimer 2
// A043
#define AI_Lichtsensor 0
#define AI_SpannungIGN 2

/* ===== libs ===== */

typedef struct
{
	bool clk; // alter wert
	bool Q;
} T_R_TRIG, T_F_TRIG;
void R_TRIG(T_R_TRIG *R_TRIG, bool clk)
{
	R_TRIG->Q = clk && !R_TRIG->clk;
	R_TRIG->clk = clk;
}
void F_TRIG(T_F_TRIG *F_TRIG, bool clk)
{
	F_TRIG->Q = !clk && F_TRIG->clk;
	F_TRIG->clk = clk;
}
typedef struct
{
	uint32_t PT,   // Voreingestellte Zeit (in Millisekunden)
		ET,		   // Abgelaufene Zeit (in Millisekunden)
		startTime; // Startzeit des Timers
	bool Q;
} T_TON, T_TOF;
bool TON(T_TON *t, bool in, uint32_t presetTime)
{
	if (!in || presetTime == 0)
	{
		t->ET = 0;
		t->startTime = 0;
		t->Q = false;
	}
	else
	{
		if (t->startTime == 0)
		{
			t->PT = presetTime;
			t->startTime = GetMSTick();
		}
		t->ET = GetMSTick() - t->startTime;
		t->Q = (t->ET >= t->PT);
	}
	return t->Q;
}
bool TOF(T_TOF *t, bool in, uint32_t presetTime)
{
	if (in)
	{
		t->ET = 0;
		t->startTime = 0;
		t->Q = true;
	}
	else
	{
		if (t->startTime == 0)
		{
			t->PT = presetTime;
			t->startTime = GetMSTick();
		}
		t->ET = GetMSTick() - t->startTime;
		t->Q = (t->ET < t->PT);
	}
	return t->Q;
}
float_t ls;
void Lichtsensor(float_t f)
{
	ls += f * (GetAnalogInput(AI_LICHTSENSOR) - ls);
}
void printfo(uint32_t ob, char *_format, ...)
{
	va_list vl;
	va_start(vl, _format);
	char str[256];
	int len = vsnprintf(str, sizeof(str), _format, vl);
	va_end(vl);
	SetVisObjData(ob, str, len + 1);
}
int32_t limit(int32_t x, int32_t a, int32_t b)
{
	if (x < a)
		return a;
	else if (x > b)
		return b;
	else
		return x;
}
uint32_t ledSin(uint32_t period)
{
	const float pi2 = 6.283185f;
	const float MaxLED2 = 32767.5f;
	float value = sinf(pi2 * fmodf(GetMSTick(), period) / period) + 1;
	return value * MaxLED2;
}
int32_t getContent(uint32_t ec, tUserCEvt *ev, uint32_t type, uint32_t source, tCEvtContent *content, int32_t foundMax)
{
	int32_t found = 0;
	for (uint32_t o = 0; o < ec; o++)
		if (ev[o].Type == type && ev[o].Source == source && found < foundMax)
			content[found++] = ev[o].Content; // Store content in array
	return found;
}
typedef struct
{
	uint32_t Nr;
	bool pressed;
	T_R_TRIG click;
	T_F_TRIG release;
} T_key;
T_key key;
int32_t renc, lenc, menu, displayBacklicht = 1000;
void setHelligkeit(int32_t neuH)
{
	displayBacklicht = limit(neuH, 0, 1000);
	SetDisplayBacklight(0, displayBacklicht);
}
int32_t key_menu(uint32_t ec, tUserCEvt *ev, uint32_t TastenAnzahl, int32_t TasteInhalt, int32_t dimmTimer)
{
	static T_TON dimm_aktiv;
	static T_R_TRIG dimm_on;
	static T_F_TRIG dimm_off;
	static int32_t renc_alt;
	tCEvtContent menuCo[1];
	bool touch_event = false;

	for (int32_t o = getContent(ec, ev, CEVT_TOUCH, CEVT_SOURCE_TOUCH, menuCo, GS_ARRAYELEMENTS(menuCo)) - 1; o >= 0; o--)
		touch_event = true;

	// Key Handling
	lenc = GetVar(HDL_SYS_ENC_LEFT);
	renc = GetVar(HDL_SYS_ENC_RIGHT);
	key.Nr = IsAnyKeyDown();
	key.pressed = (key.Nr != 0);
	R_TRIG(&key.click, key.pressed);
	F_TRIG(&key.release, key.pressed);

	if (key.pressed)
	{
		uint32_t helligkeit = limit(0, IsKeyDown(key.Nr) / 10, 100);
		if (helligkeit < 100)
			SetKeyBacklight(key.Nr, GS_KEY_BACKLIGHT_WHITE | GS_KEY_BACKLIGHT_BRIGHTNESS(helligkeit));
		else
			SetKeyBacklightColor(key.Nr, ledSin(300), ledSin(400), ledSin(500));
	}
	else if (key.release.Q)
	{
		for (uint32_t o = 1; o <= TastenAnzahl; o++)
		{
			SetKeyBacklight(o, GS_KEY_BACKLIGHT_WHITE | GS_KEY_BACKLIGHT_BRIGHTNESS(100));
			SetKeyBacklightColor(o, 0xffff, 0xffff, 0xffff);
		}
	}

	// Display Dimmen
	TON(&dimm_aktiv, !(key.click.Q || key.release.Q || touch_event || renc != renc_alt), dimmTimer);
	R_TRIG(&dimm_on, dimm_aktiv.Q);
	F_TRIG(&dimm_off, dimm_aktiv.Q);
	renc_alt = renc;
	if (dimm_on.Q)
		SetDisplayBacklight(0, 0);
	else if (dimm_off.Q)
		SetDisplayBacklight(0, displayBacklicht);

	// Überprüfen der Tastenereignisse
	if (key.click.Q)
	{
		if (TasteInhalt != 0 && key.Nr == TasteInhalt)
		{
			if (IsInfoContainerOn(0))
				InfoContainerOff(0);
			else
				InfoContainerOn(0);
		}
		return key.Nr;
	}

	// Menüevent Handling
	for (int32_t o = getContent(ec, ev, CEVT_MENU_INDEX, CEVT_SOURCE_MENU, menuCo, GS_ARRAYELEMENTS(menuCo)) - 1; o >= 0; o--)
		return menuCo[o].mMenuIndex.ObjID;

	return -1; // Standardrückgabe
}

typedef void (*PageFunc)(uint32_t, tUserCEvt *);
/*
void UserCCycle(uint32_t evtc, tUserCEvt *evtv)
{
	menu = key_menu(evtc, evtv, 8, NULL, 60000);
	((PageFunc[]){mainPage, maske1, maske2})[GetCurrentMaskShown()](evtc, evtv);
}
*/

/* State Machine */
typedef enum // Enum für die Zustände
{
	start_video1,
	warte_7s,
	stop_video1,
	zeig_ip,
	bildschirm2,
} Zustand;
Zustand aktueller_zustand;
typedef enum // Enum für die Ereignisse
{
	nix,
	video1_is_on,
	timer_7s_ok,
	video1_is_stop,
	f1_taste,
	f2_taste,
	esc_taste,
} Ereignis;
Ereignis ereignis;
typedef struct // Tabelle für die State Machine
{
	Zustand aktueller_zustand;
	Ereignis ereignis;
	Zustand neuer_zustand;
} StateMachineTabelle;
StateMachineTabelle state_machine_tabelle[] = {
	{start_video1, video1_is_on, warte_7s},
	{warte_7s, timer_7s_ok, stop_video1},
	{warte_7s, esc_taste, stop_video1},
	{stop_video1, video1_is_stop, zeig_ip},
	{zeig_ip, f1_taste, start_video1},
	{zeig_ip, f2_taste, bildschirm2},
	{bildschirm2, f1_taste, zeig_ip}};
// Funktion, die den aktuellen Zustand und das Ereignis als Eingabe nimmt
// und den neuen Zustand zurückgibt
Zustand state_machine()
{
	for (int i = 0; i < sizeof(state_machine_tabelle) / sizeof(StateMachineTabelle); i++)
		if (state_machine_tabelle[i].aktueller_zustand == aktueller_zustand &&
			state_machine_tabelle[i].ereignis == ereignis)
			aktueller_zustand = state_machine_tabelle[i].neuer_zustand;
	return aktueller_zustand;
}
// switch (state_machine())...

/* CAN */
void send1b(uint8_t mChannel, uint32_t cobId, uint16_t ind, uint8_t subind, uint8_t value)
{
	tCanFrame msg_out;
	msg_out.mId = cobId;
	msg_out.mFlags = GS_CAN_FLAG_STD_ID; // 11-Bit
	msg_out.mChannel = mChannel;
	msg_out.mLen = 5; // Data lenght
	msg_out.mData.u8[0] = 0x2f;
	msg_out.mData.u8[1] = ind & 0x00ff;
	msg_out.mData.u8[2] = ind >> 8;
	msg_out.mData.u8[3] = subind;
	msg_out.mData.u8[4] = value;
	CANSendFrame(&msg_out);
}
void send2b(uint8_t mChannel, uint32_t cobId, uint16_t ind, uint8_t subind, uint16_t value)
{
	tCanFrame msg_out;
	msg_out.mId = cobId;
	msg_out.mFlags = GS_CAN_FLAG_STD_ID; // 11-Bit
	msg_out.mChannel = mChannel;
	msg_out.mLen = 6; // Data lenght
	msg_out.mData.u8[0] = 0x2b;
	msg_out.mData.u8[1] = ind & 0x00ff;
	msg_out.mData.u8[2] = ind >> 8;
	msg_out.mData.u8[3] = subind;
	msg_out.mData.u16[2] = value;
	CANSendFrame(&msg_out);
}

/* scrolling */
void scrolling(int start, int end)
{
	char buf[255];
	for (int i = start + 1; i <= end; i++)
	{
		GetVisObjData(i, buf, 255);
		SetVisObjData(i - 1, buf, 255);
	}
}

// file exist
int fileExists(const char *filename)
{
	tGsFile *handle = FileOpen(filename, "r");
	if (handle)
	{
		FileClose(handle);
		return 1; // Datei existiert
	}
	return 0; // Datei existiert nicht
}

// ******* xml ******** //

// Struktur für ein XML-Dokument
typedef struct XmlDoc
{
	char *declaration;
	struct XMLNode *root;
} XmlDoc;

// Struktur für XML-Knoten
typedef struct XMLNode
{
	char *name;
	char *content;
	struct XMLNode **children;
	int children_count;
	struct XMLNode *parent;
} XMLNode;

char *strndup(const char *s, size_t n)
{
	size_t len = strlen(s);
	if (n < len)
		len = n;
	char *dup = (char *)malloc(len + 1);
	if (dup)
	{
		strncpy(dup, s, len);
		dup[len] = '\0';
	}
	return dup;
}

// Hilfsfunktion zum Erstellen eines neuen XML-Knotens
XMLNode *create_node(const char *name, const char *content, XMLNode *parent)
{
	XMLNode *node = (XMLNode *)malloc(sizeof(XMLNode));
	node->name = strdup(name);
	node->content = content ? strdup(content) : NULL;
	node->children = NULL;
	node->children_count = 0;
	node->parent = parent;
	return node;
}

// Funktion zum Hinzufügen eines Kindknotens zu einem Elternknoten
void add_child(XMLNode *parent, XMLNode *child)
{
	parent->children = (XMLNode **)realloc(parent->children, sizeof(XMLNode *) * (parent->children_count + 1));
	parent->children[parent->children_count++] = child;
	child->parent = parent;
}

// Funktion zum Freigeben des XML-Baums
void free_xml_tree(XMLNode *node)
{
	for (int i = 0; i < node->children_count; i++)
	{
		free_xml_tree(node->children[i]);
	}
	free(node->name);
	if (node->content)
		free(node->content);
	free(node->children);
	free(node);
}

// Funktion zum Freigeben des XML-Dokuments
void freeXML(XmlDoc *doc)
{
	if (doc->declaration)
		free(doc->declaration);
	if (doc->root)
		free_xml_tree(doc->root);
	free(doc);
}

// Hilfsfunktion zum Aufteilen des Pfades in Komponenten
char **split_path(const char *path, int *count)
{
	char *path_copy = strdup(path);
	char *token = strtok(path_copy, "/");
	char **components = NULL;
	int component_count = 0;

	while (token != NULL)
	{
		components = (char **)realloc(components, sizeof(char *) * (component_count + 1));
		components[component_count++] = strdup(token);
		token = strtok(NULL, "/");
	}

	free(path_copy);
	*count = component_count;
	return components;
}

// Hilfsfunktion zum Freigeben von Pfadkomponenten
void free_path_components(char **components, int count)
{
	while (count--)
		free(components[count]);
	free(components);
}

// Funktion zum Finden eines Knotens anhand eines mehrstufigen XPath-ähnlichen Pfades
XMLNode *find_node_by_path(XMLNode *root, char **components, int count)
{
	XMLNode *current = root;

	for (int i = 0; i < count; i++)
	{
		char *component = components[i];
		char *filter = strchr(component, '[');

		int found = 0;
		if (filter)
		{
			*filter = '\0';
			filter++;
			char *filter_end = strchr(filter, ']');
			if (filter_end)
			{
				*filter_end = '\0';
			}

			char *filter_name = strtok(filter, "=");
			char *filter_value = strtok(NULL, "=");
			if (filter_value)
			{
				if (*filter_value == '"')
				{
					filter_value++;
					filter_value[strlen(filter_value) - 1] = '\0';
				}
			}

			for (int j = 0; j < current->children_count; j++)
			{
				XMLNode *child = current->children[j];
				if (strcmp(child->name, component) == 0)
				{
					for (int k = 0; k < child->children_count; k++)
					{
						XMLNode *grandchild = child->children[k];
						if (strcmp(grandchild->name, filter_name) == 0 && strcmp(grandchild->content, filter_value) == 0)
						{
							current = child;
							found = 1;
							break;
						}
					}
				}
				if (found)
					break;
			}
		}
		else
		{
			for (int j = 0; j < current->children_count; j++)
			{
				if (strcmp(current->children[j]->name, component) == 0)
				{
					current = current->children[j];
					found = 1;
					break;
				}
			}
		}
		if (!found)
			return NULL;
	}

	return current;
}

// Funktion zum Ersetzen des Inhalts eines Knotens anhand eines mehrstufigen XPath-ähnlichen Pfades
int replace_node_content(XMLNode *root, const char *path, const char *new_content)
{
	int count;
	char **components = split_path(path, &count);

	// Zielknoten finden
	XMLNode *target = find_node_by_path(root, components, count);

	if (target)
	{
		// Alten Inhalt freigeben und neuen Inhalt setzen
		free(target->content);
		target->content = strdup(new_content);
		free_path_components(components, count);
		return 1; // Erfolg
	}

	free_path_components(components, count);
	return 0; // Knoten nicht gefunden
}

// Hilfsfunktion zum Überspringen von Leerzeichen
const char *skip_whitespace(const char *str)
{
	while (isspace(*str))
		str++;
	return str;
}

// Funktion zum Parsen der XML-Deklaration
char *parse_xml_declaration(const char **xml_string)
{
	const char *ptr = *xml_string;
	ptr = skip_whitespace(ptr);
	if (ptr[0] == '<' && ptr[1] == '?')
	{
		ptr += 2;
		const char *start = ptr;
		while (*ptr && !(ptr[0] == '?' && ptr[1] == '>'))
			ptr++;
		if (*ptr == '?' && ptr[1] == '>')
		{
			ptr += 2;
			*xml_string = ptr;
			return strndup(start, ptr - start - 2);
		}
	}
	return NULL;
}

// Hauptfunktion zum Parsen von XML
XmlDoc *parseXML(const char *xml_string)
{
	XmlDoc *doc = (XmlDoc *)malloc(sizeof(XmlDoc));
	doc->declaration = parse_xml_declaration(&xml_string);
	XMLNode *root = NULL;
	XMLNode *current_node = NULL;

	while (*xml_string)
	{
		xml_string = skip_whitespace(xml_string);

		if (*xml_string == '<')
		{
			xml_string++;

			if (*xml_string == '/')
			{ // Schließen eines Tags
				xml_string++;
				const char *name_start = xml_string;
				while (*xml_string && *xml_string != '>')
					xml_string++;
				char *name = strndup(name_start, xml_string - name_start);
				if (current_node && strcmp(current_node->name, name) == 0)
				{
					current_node = current_node->parent;
				}
				free(name);
				xml_string++;
			}
			else
			{ // Öffnen eines Tags
				const char *name_start = xml_string;
				while (*xml_string && *xml_string != '>' && !isspace(*xml_string))
					xml_string++;
				char *name = strndup(name_start, xml_string - name_start);
				XMLNode *new_node = create_node(name, NULL, current_node);
				free(name);

				if (current_node)
				{
					add_child(current_node, new_node);
				}
				else
				{
					root = new_node;
				}
				current_node = new_node;

				while (*xml_string && *xml_string != '>')
					xml_string++;
				xml_string++;
			}
		}
		else
		{ // Inhalt des Knotens
			const char *content_start = xml_string;
			while (*xml_string && *xml_string != '<')
				xml_string++;
			char *content = strndup(content_start, xml_string - content_start);
			if (current_node && current_node->content == NULL)
			{
				current_node->content = content;
			}
			else
			{
				free(content);
			}
		}
	}
	doc->root = root;
	return doc;
}

// Hilfsfunktion, um sicherzustellen, dass genügend Kapazität im Ergebnis-String vorhanden ist
void ensureCapacity(char **result, int *length, int *capacity, int additional)
{
	while (*length + additional >= *capacity)
	{
		*capacity *= 2;
		*result = realloc(*result, *capacity * sizeof(char));
	}
}

// Hilfsfunktion zum rekursiven Serialisieren des XML-Baums
void serializeXMLHelper(XMLNode *node, char **result, int *length, int *capacity)
{
	// Geschätzte benötigte Kapazität für Start-Tag, Inhalt und End-Tag
	int startTagLength = strlen(node->name) + 2; // <tag>
	int endTagLength = strlen(node->name) + 3;	 // </tag>
	int contentLength = node->content ? strlen(node->content) : 0;

	ensureCapacity(result, length, capacity, startTagLength + contentLength + endTagLength);

	// Start-Tag serialisieren
	strcat(*result, "<");
	strcat(*result, node->name);
	strcat(*result, ">");
	*length += startTagLength;

	// Inhalt, falls vorhanden, serialisieren
	if (node->content != NULL)
	{
		strcat(*result, node->content);
		*length += contentLength;
	}

	// Kinderknoten rekursiv serialisieren
	for (int i = 0; i < node->children_count; ++i)
	{
		serializeXMLHelper(node->children[i], result, length, capacity);
	}

	// End-Tag serialisieren
	strcat(*result, "</");
	strcat(*result, node->name);
	strcat(*result, ">");
	*length += endTagLength;
}

// Funktion zum Serialisieren des XML-Baums in einen String
char *serializeXML(XmlDoc *doc)
{
	if (doc && doc->root)
	{
		int length = 0;
		int capacity = 1024; // Start mit einer angemessenen anfänglichen Kapazität
		char *result = malloc(capacity * sizeof(char));
		if (result)
		{
			result[0] = '\0'; // Initialisiere den Ergebnis-String

			// XML-Deklaration hinzufügen
			if (doc->declaration)
			{
				ensureCapacity(&result, &length, &capacity, strlen(doc->declaration) + 6);
				strcat(result, "<?");
				strcat(result, doc->declaration);
				strcat(result, "?>");
				length += strlen(doc->declaration) + 5;
			}

			// Wurzelknoten serialisieren
			serializeXMLHelper(doc->root, &result, &length, &capacity);
			return result;
		}
	}
	return NULL;
}

char *loadXML(const char *fname)
{
	// Größe der Datei erhalten
	size_t fsize = FileSize(fname);
	if (fsize)
	{
		// Puffer allozieren, ein zusätzliches Byte für das Nullterminierungszeichen
		char *buffer = (char *)malloc(fsize + 1);
		if (buffer)
		{
			// Datei öffnen, lesen und schließen
			tGsFile *handle = FileOpen(fname, "rb");
			if (handle)
			{
				int ok = FileRead(buffer, fsize, 1, handle);
				FileClose(handle);
				if (ok)
				{
					buffer[fsize] = '\0'; // Nullterminierung
					return buffer;
				}
			}
			free(buffer);
		}
	}
	return NULL;
}

/* TODO test
int saveXML(const char* fname, const char* xmlContent) {
	int ok = 0;
	tGsFile* handle = FileOpen(fname, "w+b");
	if (handle) {
		//size_t xmlLength = strlen(xmlContent);
		ok = (
			//1==FileWrite(xmlContent, xmlLength, 1, handle)
			0<FilePuts(xmlContent, handle)
			&& 0==FileSync(handle)
		);
		FileClose(handle);
	}
	return ok;
}
*/
