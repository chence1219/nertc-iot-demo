#include "generic_safe_board.h"

void GenericSafeBoard::InitializeButtons(ButtonClickCallback) {
}

void GenericSafeBoard::InitializeSpi() {
}

void GenericSafeBoard::InitializeLcdDisplay() {
}

Button* GenericSafeBoard::GetButton(int) const {
    return nullptr;
}

DECLARE_BOARD(GenericSafeBoard)
