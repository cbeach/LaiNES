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
bool select_state = false;

/* Read joypad state (NES register format) */
u8 read_state(NESControllerState controller)
{
    int n = 0;
    iteration++;
    // When strobe is high, it keeps reading A:
    // Casey: Change this from the gui to some kind of grpc thing
    //if (strobe)
    //    return 0x40 | (GUI::get_joypad_state(n) & 1);

    // Get the status of a button and shift the register:
    
    // Casey: what is the meaning of these next two lines
    u8 j;
    u8 select_on =  0xFF;
    u8 select_off = 0x6F;
    //j = 0x40 | (joypad_bits[n] & 1);
    //joypad_bits[n] = 0x80 | (joypad_bits[n] >> 1);

    j |= (controller.a() ? 1 : 0)                    << 0;
    j |= (controller.b() ? 1 : 0)                    << 1;
    j |= (controller.select() ? 1 : 0)               << 2;
    j |= (controller.start() ? 1 : 0)                << 3;
    j |= (controller.dpad().up() ? 1 : 0)            << 4;
    j |= (controller.dpad().down() ? 1 : 0)          << 5;
    j |= (controller.dpad().left() ? 1 : 0)          << 6;
    j |= (controller.dpad().right() ? 1 : 0)         << 7;

    if (iteration % 1000 && select_state == false) {
      select_state = true;
      j |= 1 << 2;
    } else if (iteration % 1000 && select_state == true) {
      select_state = false;
      j &= select_off;
    }

    LOG(INFO) << "controller iteration " << iteration;
    LOG(INFO) << "--------------------------";
    LOG(INFO) << "  register: " << std::bitset< 8 >((long) j).to_string();
    LOG(INFO) << "  up      : " << controller.dpad().up();
    LOG(INFO) << "  down    : " << controller.dpad().down();
    LOG(INFO) << "  left    : " << controller.dpad().left();
    LOG(INFO) << "  right   : " << controller.dpad().right();
    LOG(INFO) << "  a       : " << controller.a();
    LOG(INFO) << "  b       : " << controller.b();
    LOG(INFO) << "  select  : " << controller.select();
    LOG(INFO) << "  start   : " << controller.start() << std::endl;
    

    return j;
}

void write_strobe(bool v)
{
    // Read the joypad data on strobe's transition 1 -> 0.
    // Casey: Change this from the gui to some kind of grpc thing
    //if (strobe and !v)
    //    for (int i = 0; i < 2; i++)
    //        joypad_bits[i] = GUI::get_joypad_state(i);

    strobe = v;
}


}
