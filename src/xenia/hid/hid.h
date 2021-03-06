/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_HID_H_
#define XENIA_HID_HID_H_

#include <xenia/hid/input_system.h>


XEDECLARECLASS1(xe, Emulator);


namespace xe {
namespace hid {


InputSystem* Create(Emulator* emulator);


}  // namespace hid
}  // namespace xe


#endif  // XENIA_HID_HID_H_
