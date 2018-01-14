#include <bitset>
#include <iostream>
#include "apu.hpp"
#include "easylogging++.hpp"

#include "deep_thought.grpc.pb.h"
using org::beachc::deep_thought::MachineState;
using org::beachc::deep_thought::VideoFrame;
using org::beachc::deep_thought::nes::NESControllerState;
namespace Joypad {


int iteration = 0;
u8 joypad_bits[2];  // Joypad shift registers.
bool strobe;        // Joypad strobe latch.


u8 get_joypad_state(int n, NESControllerState controller)
{
    u8 j = 0;
    if (n == 0) {
      j |= (controller.a() ? 1 : 0)                    << 0;
      j |= (controller.b() ? 1 : 0)                    << 1;
      j |= (controller.select() ? 1 : 0)               << 2;
      j |= (controller.start() ? 1 : 0)                << 3;
      j |= (controller.dpad().up() ? 1 : 0)            << 4;
      j |= (controller.dpad().down() ? 1 : 0)          << 5;
      j |= (controller.dpad().left() ? 1 : 0)          << 6;
      j |= (controller.dpad().right() ? 1 : 0)         << 7;

      LOG(INFO) << "controller iteration " << iteration;
      LOG(INFO) << "--------------------------";
      LOG(INFO) << "  register  : " << std::bitset< 8 >(j).to_string();
      LOG(INFO) << "  up        : " << controller.dpad().up();
      LOG(INFO) << "  down      : " << controller.dpad().down();
      LOG(INFO) << "  left      : " << controller.dpad().left();
      LOG(INFO) << "  right     : " << controller.dpad().right();
      LOG(INFO) << "  a         : " << controller.a();
      LOG(INFO) << "  b         : " << controller.b();
      LOG(INFO) << "  select    : " << controller.select();
      LOG(INFO) << "  start     : " << controller.start() << std::endl;
    } else if(n == 1) {
      // Explicitly set player two's input to 0
      j = 0;
    }

    return j;
}

/* Read joypad state (NES register format) */
u8 read_state(int n, NESControllerState controller)
{
    //Casey: need to fix this, this will only work for single player
    iteration++;
    if (strobe)
        return 0x40 | (get_joypad_state(n, controller) & 1);

    // Get the status of a button and shift the register:
    u8 j = 0x40 | (joypad_bits[n] & 1);
    joypad_bits[n] = 0x80 | (joypad_bits[n] >> 1);

    return j;
}

void write_strobe(bool v, NESControllerState controller)
{
  if (strobe and !v)
    for (int i = 0; i < 2; i++)
      joypad_bits[i] = get_joypad_state(i, controller);

  strobe = v;
}


}
