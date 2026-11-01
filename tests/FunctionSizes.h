#pragma once

// The size ratchet's counting (docs/god-functions-plan.md, phase 0): the
// length of every function definition in a source file and the number of
// mutable file globals it declares, from the text alone, so that
// function_sizes_test can refuse a function that grew past the length
// recorded in tests/function-sizes.txt or a file that gained a global.
//
// The count is the one the plan measured with: a function is a line that
// starts at column 0 with an identifier (the file's namespaces are not
// indented), continues until a line ends with `{`, and reads as
// `name(...) [const] {` once comments and literals are blanked; it ends on
// the line that closes that brace. A mutable file global is a column-0
// declaration of a `g_` name that is not const, constexpr, extern or using.

#include <cstddef>
#include <string>
#include <vector>

namespace obvr::tests::sizes {

struct FunctionSize {
	std::string name;
	int startLine = 0;  // 1-based
	int lines = 0;      // from the signature's first line to the closing brace
};

inline bool IsIdentChar(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// One line with its comments, string and character literals blanked (the
// braces inside them do not count); `inBlockComment` carries a `/* ... */`
// across lines.
inline std::string BlankLiterals(const std::string& line, bool& inBlockComment) {
	std::string out;
	out.reserve(line.size());
	for (std::size_t i = 0; i < line.size(); ++i) {
		const char c = line[i];
		const char next = i + 1 < line.size() ? line[i + 1] : '\0';
		if (inBlockComment) {
			if (c == '*' && next == '/') {
				inBlockComment = false;
				++i;
			}
			continue;
		}
		if (c == '/' && next == '*') {
			inBlockComment = true;
			++i;
			continue;
		}
		if (c == '/' && next == '/') {
			break;
		}
		if (c == '"' || c == '\'') {
			const char quote = c;
			out += quote;
			++i;
			while (i < line.size() && line[i] != quote) {
				if (line[i] == '\\') {
					++i;
				}
				++i;
			}
			out += quote;
			continue;
		}
		out += c;
	}
	return out;
}

inline bool StartsWithWord(const std::string& s, const char* word) {
	const std::size_t n = std::char_traits<char>::length(word);
	return s.compare(0, n, word) == 0 && (s.size() == n || !IsIdentChar(s[n]));
}

// Whether a column-0 line can open a function definition: it starts with an
// identifier that is not a declaration keyword or a statement's.
inline bool CanOpenFunction(const std::string& line) {
	if (line.empty() || !(IsIdentChar(line[0]) && !(line[0] >= '0' && line[0] <= '9'))) {
		return false;
	}
	static const char* const kNot[] = {"namespace", "using",  "struct", "class",  "enum",   "template", "constexpr",
	                                   "static_assert", "extern", "typedef", "if", "for", "while", "switch", "else",
	                                   "return", "do", "try", "catch", "case", "default", "goto"};
	for (const char* word : kNot) {
		if (StartsWithWord(line, word)) {
			return false;
		}
	}
	return true;
}

inline std::string TrimRight(std::string s) {
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
		s.pop_back();
	}
	return s;
}

// The function's name out of a blanked signature that ends with `{`:
// `... name(args) [const] [noexcept] [override] {`. Empty when the text does
// not read as one (an initialiser list, a lambda, a struct).
inline std::string FunctionNameOf(const std::string& signature) {
	std::string s = TrimRight(signature);
	if (s.empty() || s.back() != '{') {
		return "";
	}
	s.pop_back();
	s = TrimRight(s);
	for (const char* tail : {"override", "noexcept", "const"}) {
		const std::size_t n = std::char_traits<char>::length(tail);
		if (s.size() >= n && s.compare(s.size() - n, n, tail) == 0) {
			s.resize(s.size() - n);
			s = TrimRight(s);
		}
	}
	// A trailing return type: `auto name(...) -> T {`.
	if (!s.empty() && s.back() != ')') {
		const std::size_t arrow = s.rfind("->");
		if (arrow != std::string::npos && s.rfind(')') != std::string::npos && s.rfind(')') < arrow) {
			s = TrimRight(s.substr(0, arrow));
		}
	}
	if (s.empty() || s.back() != ')') {
		return "";
	}
	// Back to the matching '('.
	int depth = 0;
	std::size_t open = std::string::npos;
	for (std::size_t i = s.size(); i-- > 0;) {
		if (s[i] == ')') {
			++depth;
		} else if (s[i] == '(') {
			if (--depth == 0) {
				open = i;
				break;
			}
		}
	}
	if (open == std::string::npos) {
		return "";
	}
	if (s.find(';') != std::string::npos) {
		return "";
	}
	std::size_t end = open;
	while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
		--end;
	}
	std::size_t begin = end;
	while (begin > 0 && (IsIdentChar(s[begin - 1]) || s[begin - 1] == ':' || s[begin - 1] == '~')) {
		--begin;
	}
	if (begin == end) {
		return "";
	}
	const std::string name = s.substr(begin, end - begin);
	// `= Foo(...) {`: an initialiser, not a function.
	std::size_t before = begin;
	while (before > 0 && (s[before - 1] == ' ' || s[before - 1] == '\t')) {
		--before;
	}
	if (before > 0 && s[before - 1] == '=') {
		return "";
	}
	return name;
}

inline std::vector<std::string> SplitLines(const std::string& text) {
	std::vector<std::string> lines;
	std::size_t at = 0;
	while (at <= text.size()) {
		const std::size_t nl = text.find('\n', at);
		if (nl == std::string::npos) {
			if (at < text.size()) {
				lines.push_back(text.substr(at));
			}
			break;
		}
		lines.push_back(text.substr(at, nl - at));
		at = nl + 1;
	}
	return lines;
}

// Every function definition in the text, in file order.
inline std::vector<FunctionSize> FunctionSizes(const std::string& text) {
	std::vector<FunctionSize> out;
	const std::vector<std::string> lines = SplitLines(text);
	bool inBlock = false;
	int depth = 0;
	// The definition being read: its first line and the signature so far.
	int signatureStart = 0;
	std::string signature;
	// The function being measured: its name, first line and the depth it
	// opened at.
	std::string name;
	int start = 0;
	int base = 0;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const int lineNo = static_cast<int>(i) + 1;
		const std::string code = BlankLiterals(lines[i], inBlock);
		if (name.empty()) {
			if (signatureStart == 0) {
				if (CanOpenFunction(lines[i])) {
					signatureStart = lineNo;
					signature = code;
				}
			} else {
				signature += ' ';
				signature += code;
			}
			if (signatureStart != 0) {
				const std::string trimmed = TrimRight(code);
				if (trimmed.empty() || code.find(';') != std::string::npos) {
					signatureStart = 0;  // a declaration, a global, a blank line
					signature.clear();
				} else if (!trimmed.empty() && trimmed.back() == '{') {
					name = FunctionNameOf(signature);
					if (!name.empty()) {
						start = signatureStart;
						base = depth;
					}
					signatureStart = 0;
					signature.clear();
				}
			}
		}
		int opens = 0;
		int closes = 0;
		for (const char c : code) {
			opens += c == '{';
			closes += c == '}';
		}
		depth += opens - closes;
		if (!name.empty() && closes > 0 && depth == base) {
			out.push_back(FunctionSize{name, start, lineNo - start + 1});
			name.clear();
		}
	}
	return out;
}

// The mutable file globals: column-0 declarations of a `g_` name.
inline int FileGlobals(const std::string& text) {
	int count = 0;
	bool inBlock = false;
	for (const std::string& raw : SplitLines(text)) {
		const std::string line = BlankLiterals(raw, inBlock);
		if (line.empty() || !IsIdentChar(line[0]) || (line[0] >= '0' && line[0] <= '9')) {
			continue;
		}
		if (StartsWithWord(line, "const") || StartsWithWord(line, "constexpr") || StartsWithWord(line, "extern") ||
		    StartsWithWord(line, "using") || StartsWithWord(line, "typedef") || StartsWithWord(line, "static const") ||
		    StartsWithWord(line, "static constexpr")) {
			continue;
		}
		// A `g_` name that a type precedes and that a declarator's end follows.
		std::size_t at = line.find(" g_");
		if (at == std::string::npos) {
			at = line.find("\tg_");
			if (at == std::string::npos) {
				continue;
			}
		}
		// `Type& Get() { return g_x; }`: a one-line function, not a declaration.
		if (line.find_first_of("{(") < at) {
			continue;
		}
		std::size_t end = at + 3;
		while (end < line.size() && IsIdentChar(line[end])) {
			++end;
		}
		while (end < line.size() && (line[end] == ' ' || line[end] == '\t')) {
			++end;
		}
		while (end < line.size() && line[end] == '[') {
			const std::size_t close = line.find(']', end);
			if (close == std::string::npos) {
				break;
			}
			end = close + 1;
			while (end < line.size() && (line[end] == ' ' || line[end] == '\t')) {
				++end;
			}
		}
		if (end >= line.size()) {
			continue;
		}
		const char c = line[end];
		if (c == '=' || c == ';' || c == '{' || c == '(') {
			++count;
		}
	}
	return count;
}

}  // namespace obvr::tests::sizes
