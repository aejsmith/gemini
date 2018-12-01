/*
 * Copyright (C) 2018 Alex Smith
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

#pragma once

#include "Core/Utility.h"

enum InputType
{
    /** Discrete button (e.g. keyboard/mouse/gamepad button). */
    kInputType_Button,

    /** Continuous axis (e.g. mouse movement/gamepad analog stick). */
    kInputType_Axis,
};

/**
 * This enumeration is used to identify a physical input from an input device.
 * Input codes for keyboard keys are independent of whatever keyboard layout the
 * user has set, it is a fixed layout based on a standard US keyboard.
 */
enum InputCode
{
    /**
     * Keyboard codes. The values here are based on the USB keyboard usage page
     * standard, the same as what SDL uses. This simplifies conversion from SDL
     * definitions to internal ones.
     */
    kInputCode_A                = 4,
    kInputCode_B                = 5,
    kInputCode_C                = 6,
    kInputCode_D                = 7,
    kInputCode_E                = 8,
    kInputCode_F                = 9,
    kInputCode_G                = 10,
    kInputCode_H                = 11,
    kInputCode_I                = 12,
    kInputCode_J                = 13,
    kInputCode_K                = 14,
    kInputCode_L                = 15,
    kInputCode_M                = 16,
    kInputCode_N                = 17,
    kInputCode_O                = 18,
    kInputCode_P                = 19,
    kInputCode_Q                = 20,
    kInputCode_R                = 21,
    kInputCode_S                = 22,
    kInputCode_T                = 23,
    kInputCode_U                = 24,
    kInputCode_V                = 25,
    kInputCode_W                = 26,
    kInputCode_X                = 27,
    kInputCode_Y                = 28,
    kInputCode_Z                = 29,
    kInputCode_1                = 30,
    kInputCode_2                = 31,
    kInputCode_3                = 32,
    kInputCode_4                = 33,
    kInputCode_5                = 34,
    kInputCode_6                = 35,
    kInputCode_7                = 36,
    kInputCode_8                = 37,
    kInputCode_9                = 38,
    kInputCode_0                = 39,
    kInputCode_Return           = 40,
    kInputCode_Escape           = 41,
    kInputCode_Backspace        = 42,
    kInputCode_Tab              = 43,
    kInputCode_Space            = 44,
    kInputCode_Minus            = 45,
    kInputCode_Equals           = 46,
    kInputCode_LeftBracket      = 47,
    kInputCode_RightBracket     = 48,
    kInputCode_Backslash        = 49,
    kInputCode_Semicolon        = 51,
    kInputCode_Apostrophe       = 52,
    kInputCode_Grave            = 53,
    kInputCode_Comma            = 54,
    kInputCode_Period           = 55,
    kInputCode_Slash            = 56,
    kInputCode_CapsLock         = 57,
    kInputCode_F1               = 58,
    kInputCode_F2               = 59,
    kInputCode_F3               = 60,
    kInputCode_F4               = 61,
    kInputCode_F5               = 62,
    kInputCode_F6               = 63,
    kInputCode_F7               = 64,
    kInputCode_F8               = 65,
    kInputCode_F9               = 66,
    kInputCode_F10              = 67,
    kInputCode_F11              = 68,
    kInputCode_F12              = 69,
    kInputCode_PrintScreen      = 70,
    kInputCode_ScrollLock       = 71,
    kInputCode_Pause            = 72,
    kInputCode_Insert           = 73,
    kInputCode_Home             = 74,
    kInputCode_PageUp           = 75,
    kInputCode_Delete           = 76,
    kInputCode_End              = 77,
    kInputCode_PageDown         = 78,
    kInputCode_Right            = 79,
    kInputCode_Left             = 80,
    kInputCode_Down             = 81,
    kInputCode_Up               = 82,
    kInputCode_NumLock          = 83,
    kInputCode_KPDivide         = 84,
    kInputCode_KPMultiply       = 85,
    kInputCode_KPMinus          = 86,
    kInputCode_KPPlus           = 87,
    kInputCode_KPEnter          = 88,
    kInputCode_KP1              = 89,
    kInputCode_KP2              = 90,
    kInputCode_KP3              = 91,
    kInputCode_KP4              = 92,
    kInputCode_KP5              = 93,
    kInputCode_KP6              = 94,
    kInputCode_KP7              = 95,
    kInputCode_KP8              = 96,
    kInputCode_KP9              = 97,
    kInputCode_KP0              = 98,
    kInputCode_KPPeriod         = 99,
    kInputCode_NonUSBackslash   = 100,
    kInputCode_Application      = 101,
    kInputCode_KPEquals         = 103,
    kInputCode_LeftCtrl         = 224,
    kInputCode_LeftShift        = 225,
    kInputCode_LeftAlt          = 226,
    kInputCode_LeftSuper        = 227,
    kInputCode_RightCtrl        = 228,
    kInputCode_RightShift       = 229,
    kInputCode_RightAlt         = 230,
    kInputCode_RightSuper       = 231,

    kInputCodeKeyboardFirst     = kInputCode_A,
    kInputCodeKeyboardLast      = kInputCode_RightSuper,

    /**
     * Mouse codes.
     */
    kInputCode_MouseX,
    kInputCode_MouseY,
    kInputCode_MouseScroll,
    kInputCode_MouseLeft,
    kInputCode_MouseRight,
    kInputCode_MouseMiddle,

    kInputCodeMouseFirst        = kInputCode_MouseX,
    kInputCodeMouseLast         = kInputCode_MouseMiddle,

    kInputCodeCount,
};

/**
 * Enumeration of possible keyboard modifiers bitmasks. These can be OR'd
 * together.
 */
enum InputModifier : uint32_t
{
    kInputModifier_None         = 0,

    kInputModifier_LeftShift    = (1 << 0),
    kInputModifier_RightShift   = (1 << 1),
    kInputModifier_Shift        = kInputModifier_LeftShift | kInputModifier_RightShift,

    kInputModifier_LeftCtrl     = (1 << 2),
    kInputModifier_RightCtrl    = (1 << 3),
    kInputModifier_Ctrl         = kInputModifier_LeftCtrl | kInputModifier_RightCtrl,

    kInputModifier_LeftAlt      = (1 << 4),
    kInputModifier_RightAlt     = (1 << 5),
    kInputModifier_Alt          = kInputModifier_LeftAlt | kInputModifier_RightAlt,

    kInputModifier_LeftSuper    = (1 << 6),
    kInputModifier_RightSuper   = (1 << 7),
    kInputModifier_Super        = kInputModifier_LeftSuper | kInputModifier_RightSuper,

    kInputModifier_NumLock      = (1 << 8),
    kInputModifier_CapsLock     = (1 << 9),
};

DEFINE_ENUM_BITWISE_OPS(InputModifier);
