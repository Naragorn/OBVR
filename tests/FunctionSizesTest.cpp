// The size ratchet (docs/god-functions-plan.md, phase 0): first the counting
// itself (tests/FunctionSizes.h) on small fixtures - every way a line can
// open, continue or not be a function, and what is and is not a file
// global - then the repository against tests/function-sizes.txt: a function
// over 100 lines must be listed there and no longer than its recorded
// length, a file's mutable globals no more than recorded, and no row may
// name a function that is gone. Smaller is always allowed and reported, so
// that the table can be tightened:
//
//   function_sizes_test --print > tests/function-sizes.txt

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "FunctionSizes.h"

using namespace obvr::tests::sizes;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr int kThresholdLines = 100;  // a function over this must be in the table

// --- the counting on fixtures ---------------------------------------------

bool OneFunction(const char* text, const char* name, int start, int lines) {
	const std::vector<FunctionSize> f = FunctionSizes(text);
	if (f.size() != 1) {
		std::printf("        %zu functions found\n", f.size());
		return false;
	}
	if (f[0].name != name || f[0].startLine != start || f[0].lines != lines) {
		std::printf("        got %s at %d, %d lines\n", f[0].name.c_str(), f[0].startLine, f[0].lines);
		return false;
	}
	return true;
}

void TestCounting() {
	std::printf("the counting\n");
	Check(OneFunction("int Foo() {\n\treturn 1;\n}\n", "Foo", 1, 3), "a one-line signature, three lines");
	Check(OneFunction("namespace a {\n\nvoid Foo(int a,\n         int b) {\n\tif (a) {\n\t\tb = 1;\n\t}\n}\n\n}\n", "Foo",
	                  3, 6),
	      "a signature over two lines counts from its first, nested braces stay inside");
	Check(OneFunction("void Foo() {\n\tconst char* s = \"}\";\n\tchar c = '{';\n\t// }\n\t/* {\n\t */\n}\n", "Foo", 1,
	                  7),
	      "braces in strings, characters and comments do not count");
	Check(FunctionSizes("void Foo(int a);\nint g_x = 0;\nstruct S {\n\tint a;\n};\n").empty(),
	      "a declaration, a global and a struct are not functions");
	Check(FunctionSizes("if (a) {\n\tb();\n}\nfor (;;) {\n}\n").empty(), "a statement at column 0 is not a function");
	Check(FunctionSizes("int g_x[] = {\n\t1,\n};\nFoo g_y = Foo(\n\t1) {\n};\n").empty(),
	      "an initialiser's brace is not a function");
	Check(OneFunction("int A::B(const C& c) const {\n\treturn c.x;\n}\n", "A::B", 1, 3), "a const method, qualified");
	Check(OneFunction("int Foo()\n{\n\treturn 1;\n}\n", "Foo", 1, 4), "the brace on its own line");
	{
		const std::vector<FunctionSize> f = FunctionSizes("void A() {\n}\n/* void X() {\n}\n*/\nvoid B() {\n\n}\n");
		Check(f.size() == 2 && f[0].name == "A" && f[0].lines == 2 && f[1].name == "B" && f[1].startLine == 6 &&
		          f[1].lines == 3,
		      "a block comment across lines hides its function; the ones around it count");
	}
	Check(OneFunction("auto Foo() -> int {\n\treturn [] {\n\t\treturn 1;\n\t}();\n}\n", "Foo", 1, 5),
	      "a lambda inside stays inside");

	std::printf("the globals\n");
	Check(FileGlobals("UInt32 g_x = 0;\nNiPoint3 g_p{0.0f};\nbool g_a[2] = {};\nui::Canvas g_l(\"a\", 1);\n"
	                  "static bool g_s = false;\nvr::Thing g_t;\n") == 6,
	      "assigned, braced, array, constructed, static and default-initialised globals count");
	Check(FileGlobals("constexpr float kX = 1.0f;\nconst Foo g_c = Foo();\nextern int g_e;\nusing g_u = int;\n"
	                  "static const int g_k = 1;\n") == 0,
	      "const, constexpr, extern, using and static const do not");
	Check(FileGlobals("\tg_x = 1;\n  int g_y = 0;\n// int g_z = 0;\n") == 0,
	      "an indented line and a comment are not file globals");
	Check(FileGlobals("void Foo(int g_x);\nbool Bar(g_x);\n") == 0, "a g_ name that is not a declarator's does not count");
	Check(FileGlobals("Tracker& Get() { return g_t; }\n") == 0,
	      "a global returned by a one-line function is not a declaration");
}

// --- the repository against the table ----------------------------------------

struct Row {
	std::string file;
	std::string name;  // a function's, or "(file globals)"
	int limit = 0;
};

std::vector<Row> ReadTable(const std::filesystem::path& path) {
	std::vector<Row> rows;
	std::ifstream in(path);
	std::string line;
	while (std::getline(in, line)) {
		// Written by hand on either platform: a byte order mark or CR LF ends
		// change nothing.
		if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
			line.erase(0, 3);
		}
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::istringstream fields(line);
		Row r;
		std::string limit;
		if (std::getline(fields, r.file, '\t') && std::getline(fields, r.name, '\t') && std::getline(fields, limit)) {
			r.limit = std::atoi(limit.c_str());
			rows.push_back(r);
		}
	}
	return rows;
}

std::string ReadFile(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	std::stringstream s;
	s << in.rdbuf();
	return s.str();
}

struct FileMeasure {
	std::string file;  // repository-relative, forward slashes
	std::vector<std::pair<std::string, int>> functions;  // name (with #n for a repeated one), lines
	int globals = 0;
};

std::vector<FileMeasure> MeasureRepository(const std::filesystem::path& repo) {
	std::vector<std::filesystem::path> paths;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(repo / "src")) {
		if (!entry.is_regular_file()) {
			continue;
		}
		const std::string ext = entry.path().extension().string();
		if (ext == ".cpp" || ext == ".h") {
			paths.push_back(entry.path());
		}
	}
	std::sort(paths.begin(), paths.end());
	std::vector<FileMeasure> out;
	for (const std::filesystem::path& path : paths) {
		FileMeasure m;
		m.file = std::filesystem::relative(path, repo).generic_string();
		const std::string text = ReadFile(path);
		std::map<std::string, int> seen;
		for (const FunctionSize& f : FunctionSizes(text)) {
			const int n = ++seen[f.name];
			m.functions.push_back({n == 1 ? f.name : f.name + "#" + std::to_string(n), f.lines});
		}
		m.globals = FileGlobals(text);
		out.push_back(m);
	}
	return out;
}

void PrintTable(const std::vector<FileMeasure>& measures) {
	std::printf("# The size ratchet (docs/god-functions-plan.md, phase 0): every function over %d lines\n"
	            "# and every file's mutable globals, with the length function_sizes_test allows it.\n"
	            "# Smaller passes; larger fails. Rewrite with: function_sizes_test --print > tests/function-sizes.txt\n"
	            "# file<TAB>function or (file globals)<TAB>lines or count\n",
	            kThresholdLines);
	for (const FileMeasure& m : measures) {
		for (const auto& f : m.functions) {
			if (f.second > kThresholdLines) {
				std::printf("%s\t%s\t%d\n", m.file.c_str(), f.first.c_str(), f.second);
			}
		}
		if (m.globals > 0) {
			std::printf("%s\t(file globals)\t%d\n", m.file.c_str(), m.globals);
		}
	}
}

void TestRepository(const std::filesystem::path& repo) {
	std::printf("the repository against tests/function-sizes.txt\n");
	const std::vector<Row> rows = ReadTable(repo / "tests" / "function-sizes.txt");
	Check(!rows.empty(), "the table read");
	const std::vector<FileMeasure> measures = MeasureRepository(repo);
	Check(!measures.empty(), "src/ scanned");
	int tighter = 0;
	for (const FileMeasure& m : measures) {
		for (const auto& f : m.functions) {
			const Row* row = nullptr;
			for (const Row& r : rows) {
				if (r.file == m.file && r.name == f.first) {
					row = &r;
				}
			}
			if (row == nullptr) {
				if (f.second > kThresholdLines) {
					std::printf("  FAIL  %s %s is %d lines, over %d, and not in the table - split it, or add it\n",
					            m.file.c_str(), f.first.c_str(), f.second, kThresholdLines);
					++g_failures;
				}
				continue;
			}
			if (f.second > row->limit) {
				std::printf("  FAIL  %s %s grew to %d lines, the table allows %d - split it, not the table\n",
				            m.file.c_str(), f.first.c_str(), f.second, row->limit);
				++g_failures;
			} else if (f.second < row->limit) {
				std::printf("        %s %s is %d lines, the table allows %d - could tighten\n", m.file.c_str(),
				            f.first.c_str(), f.second, row->limit);
				++tighter;
			}
		}
		const Row* globalsRow = nullptr;
		for (const Row& r : rows) {
			if (r.file == m.file && r.name == "(file globals)") {
				globalsRow = &r;
			}
		}
		const int allowed = globalsRow != nullptr ? globalsRow->limit : 0;
		if (m.globals > allowed) {
			std::printf("  FAIL  %s has %d mutable file globals, the table allows %d - give the new one an owner\n",
			            m.file.c_str(), m.globals, allowed);
			++g_failures;
		} else if (m.globals < allowed) {
			std::printf("        %s has %d file globals, the table allows %d - could tighten\n", m.file.c_str(),
			            m.globals, allowed);
			++tighter;
		}
	}
	// Rows for what is gone: the table must say what the code says.
	for (const Row& r : rows) {
		bool found = false;
		for (const FileMeasure& m : measures) {
			if (m.file != r.file) {
				continue;
			}
			if (r.name == "(file globals)") {
				found = true;
			}
			for (const auto& f : m.functions) {
				found = found || f.first == r.name;
			}
		}
		if (!found) {
			std::printf("  FAIL  the table names %s %s, which is gone - remove its row\n", r.file.c_str(),
			            r.name.c_str());
			++g_failures;
		}
	}
	std::printf("  %d rows, %d could be tightened\n", static_cast<int>(rows.size()), tighter);
}

}  // namespace

int main(int argc, char** argv) {
	const std::filesystem::path repo = OBVR_REPO_DIR;
	if (argc > 1 && std::strcmp(argv[1], "--print") == 0) {
		PrintTable(MeasureRepository(repo));
		return 0;
	}
	TestCounting();
	TestRepository(repo);
	if (g_failures > 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
