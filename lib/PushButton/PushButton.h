/**
 * Header       PushButton.h
 * Author       2021-10-06 Charles Geiser (https://www.dodeka.ch)
 * 
 * Purpose      Declaration of the class PushButton
 * 
 * Constructor
 * arguments    pin   Input pin to which the pushbutton is conneted
 */

#ifndef _PUSHBUTTON_H_
#define _PUSHBUTTON_H
#include <Arduino.h>

typedef void (*CallbackFunction)();

class PushButton
{
  public:
    PushButton(uint8_t pin) : _pin(pin)
    {
      pinMode(_pin, INPUT_PULLUP);
    }

    void addOnClickCB(CallbackFunction cb);
    void addOnLongClickCB(CallbackFunction cb);
    void addOnDoubleClickCB(CallbackFunction cb);

    void loop();

  private:
    static void _nop(){};
    const uint8_t _pin;
    CallbackFunction _onClick = _nop;
    CallbackFunction _onLongClick = _nop;
    CallbackFunction _onDoubleClick = _nop;
    uint8_t _clickCount;
    int     _btnState;
    unsigned long _msDebounce = 50;        // After 50ms the button should have reached a steady state
    unsigned long _msLongClick = 300;      // A button that is held longer than 300ms is considered as longClick
    unsigned long _msDoubleClickGap = 250; // Two button clicks within 250ms count as a doubleClick
    unsigned long _msButtonDown;
    unsigned long _msFirstClick;
};
#endif