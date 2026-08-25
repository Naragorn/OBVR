#pragma once

#include "core/Types.h"
#include "vr/HeadTracker.h"

namespace obvr {

// Einstellungen aus OBVR.ini neben Oblivion.exe.
struct Config {
	bool cameraHookEnabled = true;

	vr::TrackerSettings tracker;

	// Wie oft der Kamerazustand geloggt wird, in Frames. 0 schaltet das
	// laufende Logging ab; Zustandswechsel werden trotzdem gemeldet.
	UInt32 logEveryFrames = 0;

	// Wie oft die INI im laufenden Spiel neu gelesen wird, in Frames.
	// 0 schaltet das ab.
	//
	// Ohne das kostet jede Aenderung an den Testwinkeln einen kompletten
	// Neustart samt Laden eines Spielstands. Mit Hot-Reload laesst sich die
	// Kamera im laufenden Spiel abstimmen.
	UInt32 reloadEveryFrames = 0;

	// true, wenn die Datei gefunden und gelesen wurde.
	bool Load(const char* fileName);

	// Liest nur die Werte neu, die sich gefahrlos im laufenden Spiel aendern
	// lassen. cameraHookEnabled bleibt aussen vor: der Hook ist zu diesem
	// Zeitpunkt laengst gesetzt.
	bool Reload(const char* fileName);
};

Config& GetConfig();

}  // namespace obvr
