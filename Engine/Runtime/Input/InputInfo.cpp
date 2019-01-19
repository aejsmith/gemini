/*
 * Copyright (C) 2018-2019 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "Input/InputInfo.h"

#include "Core/HashTable.h"
#include "Core/String.h"

#include <array>

class GlobalInputInfo : public InputInfo
{
public:
    using InfoMap         = HashMap<std::string, InputInfo*>;
    using InfoArray       = std::array<InputInfo*, kInputCodeCount>;

public:
                            GlobalInputInfo(const InputCode   inCode,
                                            const char* const inName,
                                            const InputType   inType);

public:
    static InfoMap&         GetInfoMap();
    static InfoArray&       GetInfoArray();

};

GlobalInputInfo::GlobalInputInfo(const InputCode   inCode,
                                 const char* const inName,
                                 const InputType   inType)
{
    this->code = inCode;
    this->name = inName;
    this->type = inType;

    GetInfoMap().insert(std::make_pair(this->name, this));
    GetInfoArray()[this->code] = this;
}

GlobalInputInfo::InfoMap& GlobalInputInfo::GetInfoMap()
{
    static InfoMap map;
    return map;
}

GlobalInputInfo::InfoArray& GlobalInputInfo::GetInfoArray()
{
    static InfoArray array;
    return array;
}

const InputInfo* InputInfo::Lookup(const InputCode inCode)
{
    return GlobalInputInfo::GetInfoArray()[inCode];
}

const InputInfo* InputInfo::Lookup(const char* const inName)
{
    const auto& map = GlobalInputInfo::GetInfoMap();

    auto ret = map.find(inName);
    return (ret != map.end()) ? ret->second : nullptr;
}

/*
 * We could declare this all as an array, but since C++ doesn't have designated
 * array initialisers (e.g. with GCC you can do [i] = ... in array initialisers)
 * we'd have to maintain the ordering of this list properly. Avoid that by
 * having the array populated at runtime in the constructors.
 */

#define DEFINE_INPUT(Name, Type) \
    static GlobalInputInfo sInfo## Name (kInputCode_ ## Name, #Name, Type)

DEFINE_INPUT(A,                 kInputType_Button);
DEFINE_INPUT(B,                 kInputType_Button);
DEFINE_INPUT(C,                 kInputType_Button);
DEFINE_INPUT(D,                 kInputType_Button);
DEFINE_INPUT(E,                 kInputType_Button);
DEFINE_INPUT(F,                 kInputType_Button);
DEFINE_INPUT(G,                 kInputType_Button);
DEFINE_INPUT(H,                 kInputType_Button);
DEFINE_INPUT(I,                 kInputType_Button);
DEFINE_INPUT(J,                 kInputType_Button);
DEFINE_INPUT(K,                 kInputType_Button);
DEFINE_INPUT(L,                 kInputType_Button);
DEFINE_INPUT(M,                 kInputType_Button);
DEFINE_INPUT(N,                 kInputType_Button);
DEFINE_INPUT(O,                 kInputType_Button);
DEFINE_INPUT(P,                 kInputType_Button);
DEFINE_INPUT(Q,                 kInputType_Button);
DEFINE_INPUT(R,                 kInputType_Button);
DEFINE_INPUT(S,                 kInputType_Button);
DEFINE_INPUT(T,                 kInputType_Button);
DEFINE_INPUT(U,                 kInputType_Button);
DEFINE_INPUT(V,                 kInputType_Button);
DEFINE_INPUT(W,                 kInputType_Button);
DEFINE_INPUT(X,                 kInputType_Button);
DEFINE_INPUT(Y,                 kInputType_Button);
DEFINE_INPUT(Z,                 kInputType_Button);
DEFINE_INPUT(1,                 kInputType_Button);
DEFINE_INPUT(2,                 kInputType_Button);
DEFINE_INPUT(3,                 kInputType_Button);
DEFINE_INPUT(4,                 kInputType_Button);
DEFINE_INPUT(5,                 kInputType_Button);
DEFINE_INPUT(6,                 kInputType_Button);
DEFINE_INPUT(7,                 kInputType_Button);
DEFINE_INPUT(8,                 kInputType_Button);
DEFINE_INPUT(9,                 kInputType_Button);
DEFINE_INPUT(0,                 kInputType_Button);
DEFINE_INPUT(Return,            kInputType_Button);
DEFINE_INPUT(Escape,            kInputType_Button);
DEFINE_INPUT(Backspace,         kInputType_Button);
DEFINE_INPUT(Tab,               kInputType_Button);
DEFINE_INPUT(Space,             kInputType_Button);
DEFINE_INPUT(Minus,             kInputType_Button);
DEFINE_INPUT(Equals,            kInputType_Button);
DEFINE_INPUT(LeftBracket,       kInputType_Button);
DEFINE_INPUT(RightBracket,      kInputType_Button);
DEFINE_INPUT(Backslash,         kInputType_Button);
DEFINE_INPUT(Semicolon,         kInputType_Button);
DEFINE_INPUT(Apostrophe,        kInputType_Button);
DEFINE_INPUT(Grave,             kInputType_Button);
DEFINE_INPUT(Comma,             kInputType_Button);
DEFINE_INPUT(Period,            kInputType_Button);
DEFINE_INPUT(Slash,             kInputType_Button);
DEFINE_INPUT(CapsLock,          kInputType_Button);
DEFINE_INPUT(F1,                kInputType_Button);
DEFINE_INPUT(F2,                kInputType_Button);
DEFINE_INPUT(F3,                kInputType_Button);
DEFINE_INPUT(F4,                kInputType_Button);
DEFINE_INPUT(F5,                kInputType_Button);
DEFINE_INPUT(F6,                kInputType_Button);
DEFINE_INPUT(F7,                kInputType_Button);
DEFINE_INPUT(F8,                kInputType_Button);
DEFINE_INPUT(F9,                kInputType_Button);
DEFINE_INPUT(F10,               kInputType_Button);
DEFINE_INPUT(F11,               kInputType_Button);
DEFINE_INPUT(F12,               kInputType_Button);
DEFINE_INPUT(PrintScreen,       kInputType_Button);
DEFINE_INPUT(ScrollLock,        kInputType_Button);
DEFINE_INPUT(Pause,             kInputType_Button);
DEFINE_INPUT(Insert,            kInputType_Button);
DEFINE_INPUT(Home,              kInputType_Button);
DEFINE_INPUT(PageUp,            kInputType_Button);
DEFINE_INPUT(Delete,            kInputType_Button);
DEFINE_INPUT(End,               kInputType_Button);
DEFINE_INPUT(PageDown,          kInputType_Button);
DEFINE_INPUT(Right,             kInputType_Button);
DEFINE_INPUT(Left,              kInputType_Button);
DEFINE_INPUT(Down,              kInputType_Button);
DEFINE_INPUT(Up,                kInputType_Button);
DEFINE_INPUT(NumLock,           kInputType_Button);
DEFINE_INPUT(KPDivide,          kInputType_Button);
DEFINE_INPUT(KPMultiply,        kInputType_Button);
DEFINE_INPUT(KPMinus,           kInputType_Button);
DEFINE_INPUT(KPPlus,            kInputType_Button);
DEFINE_INPUT(KPEnter,           kInputType_Button);
DEFINE_INPUT(KP1,               kInputType_Button);
DEFINE_INPUT(KP2,               kInputType_Button);
DEFINE_INPUT(KP3,               kInputType_Button);
DEFINE_INPUT(KP4,               kInputType_Button);
DEFINE_INPUT(KP5,               kInputType_Button);
DEFINE_INPUT(KP6,               kInputType_Button);
DEFINE_INPUT(KP7,               kInputType_Button);
DEFINE_INPUT(KP8,               kInputType_Button);
DEFINE_INPUT(KP9,               kInputType_Button);
DEFINE_INPUT(KP0,               kInputType_Button);
DEFINE_INPUT(KPPeriod,          kInputType_Button);
DEFINE_INPUT(NonUSBackslash,    kInputType_Button);
DEFINE_INPUT(Application,       kInputType_Button);
DEFINE_INPUT(KPEquals,          kInputType_Button);
DEFINE_INPUT(LeftCtrl,          kInputType_Button);
DEFINE_INPUT(LeftShift,         kInputType_Button);
DEFINE_INPUT(LeftAlt,           kInputType_Button);
DEFINE_INPUT(LeftSuper,         kInputType_Button);
DEFINE_INPUT(RightCtrl,         kInputType_Button);
DEFINE_INPUT(RightShift,        kInputType_Button);
DEFINE_INPUT(RightAlt,          kInputType_Button);
DEFINE_INPUT(RightSuper,        kInputType_Button);
DEFINE_INPUT(MouseX,            kInputType_Axis);
DEFINE_INPUT(MouseY,            kInputType_Axis);
DEFINE_INPUT(MouseScroll,       kInputType_Axis);
DEFINE_INPUT(MouseLeft,         kInputType_Button);
DEFINE_INPUT(MouseRight,        kInputType_Button);
DEFINE_INPUT(MouseMiddle,       kInputType_Button);
