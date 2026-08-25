#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Liefert die Kopfrotation, die auf die Vanilla-Kamera gelegt wird.
//
// Die Quelle ist austauschbar, weil die Bitness hier zum Problem wird:
// Oblivion.exe ist 32 Bit, OBVR.dll damit auch. OpenVR unterstuetzt 32-Bit-
// Anwendungen seit jeher, OpenXR nur bei einem Teil der Runtimes - SteamVR
// etwa erst ab Beta 2.17.2 und damit noch nicht im Stable-Zweig. Unter Proton
// faellt OpenXR ganz aus, weil wineopenxr dort nur fuer 64 Bit gebaut wird.
//
// Deshalb sind mehrere Quellen vorgesehen statt einer festen Anbindung. Der
// simulierte Kopf erlaubt es zudem, die gesamte Kette von der Quaternion bis
// zur Kameramatrix ohne Headset im laufenden Spiel zu pruefen.
//
// Alle Quellen liefern ihre Orientierung in OpenXR-Konvention (Y oben,
// -Z vorne), auch das OpenVR-Backend. Die Umrechnung nach Oblivion passiert
// an genau einer Stelle, damit jede Quelle denselben Weg nimmt.
enum class TrackerSource {
	None,       // keine Zusatzrotation
	Fixed,      // feste Testwinkel aus der Konfiguration
	Simulated,  // langsame Kopfbewegung, zum Pruefen ohne HMD
	OpenVR,     // echtes Headset ueber SteamVR; deckt 32 Bit zuverlaessig ab
	OpenXR,     // echtes Headset ueber OpenXR; nur mit 32-Bit-faehiger Runtime
};

struct TrackerSettings {
	TrackerSource source = TrackerSource::Fixed;

	// Fuer TrackerSource::Fixed. Grad, in Oblivion-Achsen.
	float fixedPitch = 0.0f;
	float fixedRoll = 0.0f;
	float fixedYaw = 0.0f;

	// Fuer TrackerSource::Simulated. Grad und Periodenlaenge in Frames.
	float simulatedYawAmplitude = 25.0f;
	float simulatedPitchAmplitude = 12.0f;
	UInt32 simulatedPeriodFrames = 600;
};

class HeadTracker {
public:
	void Configure(const TrackerSettings& settings);

	// Einmal pro Frame. frameIndex ersetzt beim simulierten Kopf die Zeit -
	// eine echte Uhr braucht OBVR dafuer nicht, und ohne sie bleibt das
	// Verhalten reproduzierbar.
	void Update(UInt32 frameIndex);

	// Nimmt die aktuelle Kopfhaltung als neue Nullstellung. Alles danach wird
	// relativ dazu gemeldet.
	void Recenter();

	// Rotation relativ zur Nullstellung, fertig in Oblivions Kameraraum.
	const NiMatrix33& GetCameraRotation() const { return m_cameraRotation; }

	// Die zuletzt gelesene Orientierung, noch in OpenXR-Konvention. Vor allem
	// fuer Diagnose im Log.
	const Quaternion& GetRawOrientation() const { return m_rawOrientation; }

private:
	Quaternion ReadSource(UInt32 frameIndex) const;

	TrackerSettings m_settings;
	Quaternion m_rawOrientation = Quaternion::Identity();
	Quaternion m_reference = Quaternion::Identity();
	NiMatrix33 m_cameraRotation = NiMatrix33::Identity();
};

}  // namespace obvr::vr
