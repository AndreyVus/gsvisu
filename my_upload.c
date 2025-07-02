#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>   // Für Verzeichnisoperationen
#include <unistd.h>   // Für access und close
#include <sys/stat.h> // Für stat
#include <sys/wait.h> // Für WIFEXITED, WEXITSTATUS beim Prüfen des system()-Rückgabewerts
#include <sys/socket.h> // Für Socket-Kommunikation
#include <arpa/inet.h>  // Für inet_pton

// Gemeinsame Konstanten für Server-Verbindung
#define SERVER_IP "172.16.128.31"
#define SERVER_PORT 23080
#define SERVER_USER "gs"

// Konstanten für Dateinamen, Pfade und Befehlsteile
#define KEY_DIR ".keypair"
#define KEY_EXT ".key"
#define GDZ_EXT ".GDZ" // Groß-/Kleinschreibung beachten
#define SCP_DEST SERVER_USER "@" SERVER_IP ":/tmp/dlprj.gdz"

// Nachricht für Updateanforderung
#define UPDATE_MESSAGE "updaterequest\n"

// Maximale Puffergrößen (erhöht um Warnungen zu vermeiden)
#define MAX_PATH_LEN 512
#define MAX_CMD_LEN 1024

// Helper-Funktion, um zu prüfen, ob ein String mit einem Suffix endet
int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str) return 0;
    return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}

// Funktion zur Ausführung des SCP-Befehls
int execute_scp(const char *key_file_path, const char *gdz_file_name) {
    char scp_command[MAX_CMD_LEN];
    
    // SCP-Befehl zusammenbauen
    int cmd_len = snprintf(scp_command, sizeof(scp_command), 
                          "scp -i %s %s %s", 
                          key_file_path, gdz_file_name, SCP_DEST);
    
    if (cmd_len < 0 || cmd_len >= sizeof(scp_command)) {
        fprintf(stderr, "Fehler: SCP-Befehl ist zu lang (%d Zeichen).\n", cmd_len);
        return 1;
    }

    printf("\nZusammengebauter SCP-Befehl:\n%s\n", scp_command);

    // SCP-Befehl ausführen
    printf("\nFühre SCP-Befehl aus...\n");
    int system_status = system(scp_command);

    // Ergebnis des Befehls prüfen
    if (system_status == -1) {
        perror("Fehler beim Ausführen des Befehls system()");
        return 1;
    } else {
        if (WIFEXITED(system_status)) {
            int exit_code = WEXITSTATUS(system_status);
            if (exit_code == 0) {
                printf("Dateiübertragung erfolgreich (Exit Code 0).\n");
                return 0; // Erfolg
            } else {
                fprintf(stderr, "Dateiübertragung fehlgeschlagen (scp Exit Code %d).\n", exit_code);
                return exit_code;
            }
        } else {
            fprintf(stderr, "SCP-Befehl wurde nicht normal beendet.\n");
            return 1;
        }
    }
}

// Funktion zum Senden der Updateanfrage
int send_update_request() {
    int sock;
    struct sockaddr_in server_addr;
    ssize_t sent_bytes;

    printf("\n--- Sende Update-Anfrage ---\n");

    // Socket erstellen: AF_INET für IPv4, SOCK_STREAM für TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Fehler beim Erstellen des Sockets");
        return 1;
    }
    printf("Socket erstellt.\n");

    // Serveradresse vorbereiten
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(SERVER_PORT); // Port in Netzwerk-Byte-Reihenfolge

    // IP-Adresse von String in Binärform konvertieren
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Ungültige IP-Adresse oder Adresse wird nicht unterstützt");
        close(sock);
        return 1;
    }

    // Verbindung zum Server herstellen
    printf("Versuche Verbindung zu %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Verbindungsfehler");
        close(sock);
        return 1;
    }
    printf("Verbindung erfolgreich hergestellt.\n");

    // Nachricht senden
    printf("Sende Nachricht: '%s'\n", UPDATE_MESSAGE);
    sent_bytes = send(sock, UPDATE_MESSAGE, strlen(UPDATE_MESSAGE), 0);
    
    if (sent_bytes < 0) {
        perror("Fehler beim Senden der Nachricht");
        close(sock);
        return 1;
    } else if (sent_bytes != strlen(UPDATE_MESSAGE)) {
        fprintf(stderr, "Warnung: Nicht alle Bytes gesendet (%zd von %zu)\n", 
                sent_bytes, strlen(UPDATE_MESSAGE));
        close(sock);
        return 1;
    } else {
        printf("Nachricht erfolgreich gesendet (%zd Bytes).\n", sent_bytes);
    }

    // Socket schließen
    close(sock);
    printf("Socket geschlossen.\n");
    return 0;
}

int main() {
    char key_file_path[MAX_PATH_LEN] = ""; // Voller Pfad zur Key-Datei (.keypair/...)
    char gdz_file_name[MAX_PATH_LEN] = ""; // Nur der Dateiname der GDZ-Datei
    DIR *dir;
    struct dirent *entry;
    int result;

    printf("Suche nach Key-Datei ('*%s') im Verzeichnis '%s'...\n", KEY_EXT, KEY_DIR);

    // --- 1. Key-Datei finden ---
    dir = opendir(KEY_DIR);
    if (!dir) {
        // opendir schlägt fehl, wenn das Verzeichnis nicht existiert oder keine Rechte da sind
        perror("Fehler beim Öffnen des Verzeichnisses " KEY_DIR);
        fprintf(stderr, "Stellen Sie sicher, dass das Verzeichnis '%s' existiert und zugänglich ist.\n", KEY_DIR);
        return 1; // Fehlercode zurückgeben
    }

    // Durchsuche das Verzeichnis
    while ((entry = readdir(dir)) != NULL) {
        // Überspringe die Einträge "." und ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Optional: Nur reguläre Dateien prüfen (könnte d_type nicht immer unterstützen)
        // if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            if (ends_with(entry->d_name, KEY_EXT)) {
                // Gefunden! Speicher den Pfad und beende die Suche.
                // snprintf ist sicherer als sprintf, da Pufferüberläufe vermieden werden
                int len = snprintf(key_file_path, sizeof(key_file_path), "%s/%s", KEY_DIR, entry->d_name);
                if (len < 0 || len >= sizeof(key_file_path)) {
                    fprintf(stderr, "Fehler: Pfad zur Key-Datei ist zu lang (%d Zeichen).\n", len);
                    closedir(dir);
                    return 1;
                }
                printf("Gefundene Key-Datei: %s\n", key_file_path);
                break; // Nimm die erste gefundene Datei
            }
        // }
    }
    closedir(dir); // Verzeichnis wieder schließen

    // Prüfen, ob eine Key-Datei gefunden wurde
    if (key_file_path[0] == '\0') {
        fprintf(stderr, "Fehler: Keine Datei mit der Endung '%s' im Verzeichnis '%s' gefunden.\n", KEY_EXT, KEY_DIR);
        return 1; // Fehlercode zurückgeben
    }

    printf("\nSuche nach GDZ-Datei ('*%s') im aktuellen Verzeichnis ('.')...\n", GDZ_EXT);

    // --- 2. GDZ-Datei im aktuellen Verzeichnis finden ---
    dir = opendir("."); // Aktuelles Verzeichnis öffnen
    if (!dir) {
        perror("Fehler beim Öffnen des aktuellen Verzeichnisses");
        return 1; // Fehlercode zurückgeben
    }

    // Durchsuche das aktuelle Verzeichnis
    while ((entry = readdir(dir)) != NULL) {
         // Überspringe die Einträge "." und ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Optional: Nur reguläre Dateien prüfen
        // if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            if (ends_with(entry->d_name, GDZ_EXT)) {
                // Gefunden! Speicher den Dateinamen und beende die Suche.
                strncpy(gdz_file_name, entry->d_name, sizeof(gdz_file_name) - 1);
                gdz_file_name[sizeof(gdz_file_name) - 1] = '\0'; // Null-Terminierung sicherstellen
                printf("Gefundene GDZ-Datei: %s\n", gdz_file_name);
                break; // Nimm die erste gefundene Datei
            }
        // }
    }
    closedir(dir); // Verzeichnis wieder schließen

    // Prüfen, ob eine GDZ-Datei gefunden wurde
    if (gdz_file_name[0] == '\0') {
        fprintf(stderr, "Fehler: Keine Datei mit der Endung '%s' im aktuellen Verzeichnis gefunden.\n", GDZ_EXT);
        return 1; // Fehlercode zurückgeben
    }

    // --- 3. SCP ausführen und Datei hochladen ---
    printf("\n--- Starte Dateiübertragung ---\n");
    result = execute_scp(key_file_path, gdz_file_name);
    if (result != 0) {
        printf("Fehler bei der Dateiübertragung. Beende Programm.\n");
        return result;
    }
    
    // --- 5. Updateanfrage senden ---
    printf("\n--- Dateiübertragung erfolgreich ---\n");
    result = send_update_request();
    if (result != 0) {
        printf("Fehler beim Senden der Updateanfrage. Beende Programm.\n");
        return result;
    }
    
    printf("\n--- Vorgang erfolgreich abgeschlossen ---\n");
    printf("1. GDZ-Datei '%s' übertragen\n", gdz_file_name);
    printf("2. Updateanfrage erfolgreich gesendet\n");
    
    return 0;
}
