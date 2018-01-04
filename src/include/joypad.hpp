#pragma once
#include "common.hpp"

#include "deep_thought.grpc.pb.h"
using org::beachc::deep_thought::nes::NESControllerState;
namespace Joypad {


u8 read_state(NESControllerState);
void write_strobe(bool v);


}
