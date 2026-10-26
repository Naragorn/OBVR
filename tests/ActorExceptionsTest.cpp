// Checks the lists of actors left out of the fighters' ring and the enemy
// attack tempo (game/ActorExceptionsLogic.h): a comma-separated setting read
// into entries, and an actor matched by its name or its race's name.

#include <cstdio>
#include <cstring>
#include <string>

#include "game/ActorExceptionsLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

ActorIdentity Who(const char* name, const char* race) {
	ActorIdentity a;
	a.name = name;
	a.race = race;
	return a;
}

void TestParse() {
	std::printf("The setting read into entries\n");
	ActorExceptions e = ParseActorExceptions("Zombie, Walker ,,  Headless Horse ");
	Check(e.count == 3, "three entries; blanks and empty ones left out");
	Check(std::string(e.entry[0]) == "Zombie" && std::string(e.entry[1]) == "Walker" &&
	          std::string(e.entry[2]) == "Headless Horse",
	      "each trimmed, a space inside kept");
	Check(ParseActorExceptions("").count == 0 && ParseActorExceptions(nullptr).count == 0, "empty or none: no entries");
	char many[1024] = "";
	for (int i = 0; i < kActorExceptionsMax + 4; ++i) {
		std::strcat(many, "x,");
	}
	Check(ParseActorExceptions(many).count == kActorExceptionsMax, "past the table's size the rest is left out");
	char longOne[200];
	for (int i = 0; i < 199; ++i) {
		longOne[i] = 'z';
	}
	longOne[199] = '\0';
	ActorExceptions l = ParseActorExceptions(longOne);
	Check(l.count == 1 && std::strlen(l.entry[0]) == kActorExceptionChars - 1, "an entry too long is cut to fit");
}

void TestMatch() {
	std::printf("An actor matched\n");
	const ActorExceptions e = ParseActorExceptions("Zombie,Walker");
	Check(ActorExcepted(e, Who("Zombie", nullptr)), "a vanilla zombie by its name");
	Check(ActorExcepted(e, Who("Walker", nullptr)), "a Walkers walker by its name");
	Check(ActorExcepted(e, Who("Headless ZOMBIE", "")), "within the name, any case");
	Check(ActorExcepted(e, Who("Jauffre", "Walker")), "a person of the Walker race by the race");
	Check(!ActorExcepted(e, Who("Imperial Legion Soldier", "Imperial")), "a gate guard: not excepted");
	Check(!ActorExcepted(e, Who("Walk", "Imperial")), "a part of an entry is no match");
	Check(!ActorExcepted(ParseActorExceptions(""), Who("Zombie", "Walker")), "an empty list excepts nobody");
	Check(!ActorExcepted(e, Who(nullptr, nullptr)), "nothing read about the actor: not excepted");
}

}  // namespace

int main() {
	TestParse();
	TestMatch();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
