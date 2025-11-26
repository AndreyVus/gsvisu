// class FileQueue
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <UserCAPI.h> // Wichtig: Nutzt die GSe-Visu API Funktionen

class FileQueue
{
private:
	std::vector<std::string> m_files;
	std::vector<std::string> m_allowedExts;

	// Hilfsmethode: String in Kleinbuchstaben (für C++11)
	std::string toLower(const std::string &str) const
	{
		std::string lowerStr = str;
		std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
					   [](unsigned char c)
					   { return std::tolower(c); });
		return lowerStr;
	}

	// Optimierte Prüfung (wie zuvor besprochen)
	bool hasAllowedExtension(const std::string &filename) const
	{
		if (m_allowedExts.empty())
			return true; // Keine Filter = alle erlauben

		std::size_t dot = filename.rfind('.');
		if (dot == std::string::npos)
			return false;

		const char *fileExt = filename.c_str() + dot;
		std::size_t fileExtLen = filename.size() - dot;

		for (const auto &allowed : m_allowedExts)
		{
			if (allowed.size() != fileExtLen)
				continue;

			bool match = true;
			for (std::size_t i = 0; i < fileExtLen; ++i)
			{
				if (std::tolower(static_cast<unsigned char>(fileExt[i])) !=
					static_cast<unsigned char>(allowed[i])) // allowed ist schon lower
				{
					match = false;
					break;
				}
			}
			if (match)
				return true;
		}
		return false;
	}

	// Rekursive Scan-Funktion mit UserCAPI
	void scanDirectory(std::string path, bool recursive)
	{
		// Pfad muss mit '/' enden für Zusammensetzung
		if (!path.empty() && path.back() != '/')
		{
			path += '/';
		}

		tGsDir *dir = DirOpen(path.c_str());
		if (!dir)
			return;

		tGsDirEntry entry;
		// DirRead liefert 0, wenn erfolgreich ein Eintrag gelesen wurde
		while (DirRead(dir, &entry) == 0)
		{
			std::string name = entry.mName;

			// "." und ".." ignorieren
			if (name == "." || name == "..")
				continue;

			if (entry.mType == DIR_ENTRY_TYPE_FILE)
			{
				if (hasAllowedExtension(name))
				{
					m_files.push_back(path + name);
				}
			}
			else if (recursive && entry.mType == DIR_ENTRY_TYPE_DIR)
			{
				scanDirectory(path + name, recursive);
			}
		}

		DirClose(dir);
	}

public:
	FileQueue() = default;

	/**
	 * Initialisiert die Queue und sucht nach Dateien.
	 *
	 * @param startPath Startverzeichnis (z.B. "/gs/data/")
	 * @param recursive true durchsucht auch Unterordner
	 * @param extensions Liste der Endungen inkl. Punkt (z.B. {".wav", ".mp3"})
	 * @return Anzahl der gefundenen Dateien
	 */
	int init(const std::string &startPath, bool recursive, const std::vector<std::string> &extensions)
	{
		m_files.clear();
		m_allowedExts.clear();
		m_allowedExts.reserve(extensions.size());

		// Extensions vorverarbeiten (in Kleinbuchstaben wandeln für schnellen Vergleich)
		for (const auto &ext : extensions)
		{
			m_allowedExts.push_back(toLower(ext));
		}

		scanDirectory(startPath, recursive);

		return static_cast<int>(m_files.size());
	}

	// Zugriff per Index: string file = queue[0];
	const std::string &operator[](std::size_t index) const
	{
		// Sicherheitshalber leeren String zurückgeben, wenn Index falsch
		// (In C++ ist operator[] normalerweise ungeprüft, aber hier sicherer)
		static const std::string empty;
		if (index >= m_files.size())
		{
			return empty;
		}
		return m_files[index];
	}

	// Anzahl abfragen
	std::size_t size() const
	{
		return m_files.size();
	}

	// Prüfen ob leer
	bool empty() const
	{
		return m_files.empty();
	}
};
