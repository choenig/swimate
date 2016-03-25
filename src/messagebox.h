#pragma once

#include "pebble.h"

typedef void (*VoidFnc)();

void showMessageBox(const char * msg,
                    VoidFnc okFunction, VoidFnc nokFunction,
                    uint32_t okResourceId, uint32_t nokResourceId);
