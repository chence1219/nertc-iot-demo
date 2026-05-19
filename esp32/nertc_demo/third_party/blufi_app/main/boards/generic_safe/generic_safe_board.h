#ifndef _GENERIC_SAFE_BOARD_H_
#define _GENERIC_SAFE_BOARD_H_

#include "boards/board.h"
#include "display/display.h"

class GenericSafeBoard : public Board {
public:
    void InitializeButtons(ButtonClickCallback on_click) override;
    void InitializeSpi() override;
    void InitializeLcdDisplay() override;

    Display* GetDisplay() const override { return const_cast<NoDisplay*>(&display_); }
    Button* GetButton(int index) const override;

private:
    NoDisplay display_;
};

#endif // _GENERIC_SAFE_BOARD_H_
