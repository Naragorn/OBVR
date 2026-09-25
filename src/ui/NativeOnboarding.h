#pragma once

namespace obvr::ui {

// Prototype only: IDs belong to our GenericMenu XML, not to MainMenu.
constexpr int kNativeClassic = 9101;
constexpr int kNativeMotion = 9102;
// Explicit focus for GenericMenu: its engine keyboard handler does not cycle
// our custom button IDs. Mouse movement releases this focus back to hit testing.
inline int NavigateOnboarding(int selected,bool previous,bool next,bool mouseMoved) {
 if(mouseMoved) return -1;
 if(next) return kNativeMotion;
 if(previous) return kNativeClassic;
 return selected;
}
enum class NativePage { Choices, SaveFailed };
enum class NativeState { Waiting, Open, Done, Failed };

// Engine effects isolated behind a small port so all lifecycle flows run
// without Oblivion, MenuQue, a filesystem or a headset.
struct NativeMenuPort {
 virtual ~NativeMenuPort() = default;
 virtual bool Open(NativePage page) = 0;
 virtual bool SaveMode(bool fullVR) = 0;
};

class NativeOnboarding {
public:
 NativeState State() const { return m_state; }
 void Tick(bool mainMenu, bool foreignMenu, int button, NativeMenuPort& port) {
  if (m_state == NativeState::Done || m_state == NativeState::Failed) return;
  if (foreignMenu) {
   if (m_state == NativeState::Open) m_state = NativeState::Failed;
   return;
  }
  if (m_state == NativeState::Waiting) {
   if (mainMenu) Show(NativePage::Choices, port);
   return;
  }
  if (button == kNativeClassic || button == kNativeMotion) {
   if (port.SaveMode(button == kNativeMotion)) m_state = NativeState::Done;
   else Show(NativePage::SaveFailed, port);

  } else if (button != -1 || mainMenu) {
   // Unknown IDs or externally closed menu: leave control with the game.
   m_state = NativeState::Failed;
  }
 }
private:
 void Show(NativePage page, NativeMenuPort& port) {
  m_state = port.Open(page) ? NativeState::Open : NativeState::Failed;
 }
 NativeState m_state = NativeState::Waiting;
};

} // namespace obvr::ui
