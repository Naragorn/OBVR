// Wird nur im SDK-freien Verifikationsbau mituebersetzt.
//
// Ohne CRT fehlen ein paar Symbole, die der MSVC-kompatible Codegenerator
// voraussetzt. Beim regulaeren MSVC-Bau liefert die Runtime sie selbst,
// deshalb darf diese Datei dort nicht mitgebaut werden.

#if defined(OBVR_NO_WINSDK)

// Der Compiler emittiert einen Verweis auf _fltused, sobald Fliesskomma
// benutzt wird. Der Wert ist ein historischer Marker der MSVC-Runtime; nur
// seine Existenz zaehlt.
extern "C" int _fltused = 0x9875;

#endif
