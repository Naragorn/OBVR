#pragma once

#include "game/GameTypes.h"
#include "vr/HeadTracker.h"

namespace obvr::camera {

// Haengt sich an das Ende von Oblivions Kameraberechnung.
//
// Ablauf pro Frame:
//
//   Oblivion berechnet die Vanilla-Kamera
//        -> schreibt localTransform.pos und .rot des CameraNode
//        -> OBVR legt die Kopfrotation darauf
//        -> Oblivion laeuft weiter und aktualisiert den Szenengraph
//
// Damit ist die VR-Formel umgesetzt:
//
//   finale Kamera = Vanilla-Kamera * relative Kopfrotation
//
// Gibt false zurueck, wenn die erwarteten Bytes nicht an der Zieladresse
// stehen. Dann wird nichts gepatcht - eine falsche Spielversion soll kein
// zerschossenes Codesegment ergeben.
bool Install();

// Zustand des letzten Hook-Durchlaufs, fuer Logging und spaetere Nutzung.
struct State {
	bool sawCameraNode = false;
	bool isThirdPerson = false;
	UInt32 frameCount = 0;
};

const State& GetState();

// Der Tracker, dessen Rotation auf die Kamera gelegt wird.
vr::HeadTracker& GetHeadTracker();

}  // namespace obvr::camera
