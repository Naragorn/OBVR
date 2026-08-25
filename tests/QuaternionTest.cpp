// Prueft die Quaternion-Mathematik und den Basiswechsel von OpenXR nach
// Oblivion.
//
// Der wichtigste Test ist die Gegenprobe gegen EulerToMatrix: diese
// Funktion wurde im laufenden Spiel verifiziert (Pitch und Roll sichtbar
// korrekt). Wenn die Quaternion-Route dieselben Matrizen liefert, ist sie an
// eine belegte Referenz gebunden statt nur in sich stimmig.

#include <cmath>
#include <cstdio>

#include "core/Rotation.h"
#include "vr/Quaternion.h"

namespace {

int g_failures = 0;

constexpr float kEpsilon = 1e-4f;

void CheckNear(float actual, float expected, const char* what) {
	if (std::fabs(actual - expected) <= kEpsilon) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FEHLT %s: %.6f, erwartet %.6f\n", what, actual, expected);
		++g_failures;
	}
}

void CheckMatrixNear(const obvr::NiMatrix33& actual, const obvr::NiMatrix33& expected,
                     const char* what) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			if (std::fabs(actual.data[row][col] - expected.data[row][col]) > kEpsilon) {
				std::printf("  FEHLT %s: [%d][%d] ist %.6f, erwartet %.6f\n",
				            what, row, col, actual.data[row][col], expected.data[row][col]);
				++g_failures;
				return;
			}
		}
	}
	std::printf("  ok    %s\n", what);
}

using obvr::vr::Quaternion;

void TestBasics() {
	std::printf("Grundrechnung\n");

	const Quaternion identity = Quaternion::Identity();
	CheckMatrixNear(obvr::vr::ToMatrix(identity), obvr::NiMatrix33::Identity(),
	                "Identitaet ergibt Einheitsmatrix");

	// Eine Rotation mit ihrer eigenen Inversen verkettet muss sich aufheben.
	const Quaternion rotation = obvr::vr::FromAxisAngle(0.3f, 0.5f, 0.8f, 47.0f);
	const Quaternion undone = rotation.Conjugate() * rotation;
	CheckNear(undone.w, 1.0f, "q^-1 * q hat w = 1");
	CheckNear(undone.x, 0.0f, "q^-1 * q hat x = 0");
	CheckNear(undone.y, 0.0f, "q^-1 * q hat y = 0");
	CheckNear(undone.z, 0.0f, "q^-1 * q hat z = 0");

	// FromAxisAngle muss die Achse selbst normieren, sonst waere das Ergebnis
	// von der Laenge des uebergebenen Vektors abhaengig.
	const Quaternion fromLongAxis = obvr::vr::FromAxisAngle(0.0f, 0.0f, 5.0f, 30.0f);
	const Quaternion fromUnitAxis = obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 30.0f);
	CheckNear(fromLongAxis.z, fromUnitAxis.z, "Achsenlaenge beeinflusst das Ergebnis nicht");

	CheckNear(obvr::vr::FromAxisAngle(1.0f, 2.0f, 3.0f, 90.0f).LengthSquared(), 1.0f,
	          "Ergebnis ist normiert");
}

// Die entscheidende Bindung an die im Spiel bestaetigte Referenz.
void TestAgainstVerifiedRotation() {
	std::printf("Gegenprobe gegen EulerToMatrix (im Spiel verifiziert)\n");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(1.0f, 0.0f, 0.0f, 20.0f)),
	                obvr::EulerToMatrix(20.0f, 0.0f, 0.0f),
	                "Drehung um X entspricht BuildRotation(20,0,0) = Pitch");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 20.0f)),
	                obvr::EulerToMatrix(0.0f, 20.0f, 0.0f),
	                "Drehung um Y entspricht BuildRotation(0,20,0) = Roll");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 20.0f)),
	                obvr::EulerToMatrix(0.0f, 0.0f, 20.0f),
	                "Drehung um Z entspricht BuildRotation(0,0,20) = Yaw");
}

void TestOpenXrAxisSwap() {
	std::printf("Basiswechsel OpenXR nach Oblivion\n");

	// OpenXR: Y ist oben. Nach links und rechts schauen heisst dort, um Y zu
	// drehen. In Oblivion ist Z oben, das Yaw gehoert also auf Z.
	const Quaternion xrYaw = obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 25.0f);
	const Quaternion oblYaw = obvr::vr::FromOpenXR(xrYaw);
	CheckMatrixNear(obvr::vr::ToMatrix(oblYaw), obvr::EulerToMatrix(0.0f, 0.0f, 25.0f),
	                "OpenXR-Yaw um Y wird zu Oblivion-Yaw um Z");

	// OpenXR: X ist rechts, Pitch dreht um X. In Oblivion ebenfalls X.
	const Quaternion xrPitch = obvr::vr::FromAxisAngle(1.0f, 0.0f, 0.0f, 25.0f);
	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenXR(xrPitch)),
	                obvr::EulerToMatrix(25.0f, 0.0f, 0.0f),
	                "OpenXR-Pitch um X bleibt Pitch um X");

	// OpenXR: -Z ist die Blickrichtung, Roll dreht also um Z. In Oblivion ist
	// die Blickrichtung +Y, und wegen der Umkehrung wird daraus -Y.
	const Quaternion xrRoll = obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 25.0f);
	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenXR(xrRoll)),
	                obvr::EulerToMatrix(0.0f, -25.0f, 0.0f),
	                "OpenXR-Roll um Z wird zu Oblivion-Roll um -Y");

	// Der Basiswechsel darf die Haendigkeit nicht kippen; eine gespiegelte
	// Kamera waere im Headset sofort als falsch erkennbar.
	const Quaternion arbitrary = obvr::vr::FromAxisAngle(0.4f, -0.7f, 0.2f, 63.0f);
	const obvr::NiMatrix33 m = obvr::vr::ToMatrix(obvr::vr::FromOpenXR(arbitrary));
	const float determinant =
		m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
		m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
		m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
	CheckNear(determinant, 1.0f, "Determinante bleibt +1, keine Spiegelung");
}

void TestRecenter() {
	std::printf("Recenter\n");

	// Recenter merkt sich die aktuelle Orientierung als neue Null. Direkt
	// danach muss die relative Rotation die Identitaet sein.
	const Quaternion reference = obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 130.0f);
	const Quaternion relative = reference.Conjugate() * reference;
	CheckMatrixNear(obvr::vr::ToMatrix(relative.Normalized()), obvr::NiMatrix33::Identity(),
	                "unmittelbar nach Recenter ist die Rotation neutral");

	// Dreht der Kopf sich danach um 30 Grad weiter, muessen genau diese
	// 30 Grad uebrig bleiben - unabhaengig davon, wie die Referenz stand.
	const Quaternion current =
		obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 160.0f);
	const Quaternion delta = (reference.Conjugate() * current).Normalized();
	CheckMatrixNear(obvr::vr::ToMatrix(delta),
	                obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 30.0f)),
	                "30 Grad nach dem Recenter ergeben 30 Grad Kamerarotation");
}

}  // namespace

int main() {
	std::printf("OBVR Quaternion-Test\n\n");

	TestBasics();
	std::printf("\n");
	TestAgainstVerifiedRotation();
	std::printf("\n");
	TestOpenXrAxisSwap();
	std::printf("\n");
	TestRecenter();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("Alle Pruefungen bestanden.\n");
		return 0;
	}

	std::printf("%d Pruefung(en) fehlgeschlagen.\n", g_failures);
	return 1;
}
