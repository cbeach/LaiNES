#include "apu.hpp"

#include "deep_thought.grpc.pb.h"
using org::beachc::deep_thought::MachineState;
using org::beachc::deep_thought::VideoFrame;
using org::beachc::deep_thought::nes::NESControllerState;
namespace Joypad {


u8 joypad_bits[2];  // Joypad shift registers.
bool strobe;        // Joypad strobe latch.

/* Read joypad state (NES register format) */
u8 read_state(NESControllerState controller)
{
    int n = 0;
    // When strobe is high, it keeps reading A:
    // Casey: Change this from the gui to some kind of grpc thing
    //if (strobe)
    //    return 0x40 | (GUI::get_joypad_state(n) & 1);

    // Get the status of a button and shift the register:
    
    // Casey: what is the meaning of these next two lines
    u8 j = 0x40 | (joypad_bits[n] & 1);
    joypad_bits[n] = 0x80 | (joypad_bits[n] >> 1);

    j |= (controller.a() ? 1 : 0)                 << 0;
    j |= (controller.b() ? 1 : 0)                 << 1;
    j |= (controller.select() ? 1 : 0)            << 2;
    j |= (controller.start() ? 1 : 0)             << 3;
    j |= (controller.dpad().up() ? 1 : 0)         << 4;
    j |= (controller.dpad().down() ? 1 : 0)       << 5;
    j |= (controller.dpad().left() ? 1 : 0)       << 6;
    j |= (controller.dpad().right() ? 1 : 0)      << 7;
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
