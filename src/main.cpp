// #include "Sound_Queue.h"
#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"


void run() {
    // Framerate control:
    u32 frameStart, frameTime;

    while (true) {
        // Handle events:
        /************************ refactor into a streaming endpoint for grpc
        while (SDL_PollEvent(&e))
            switch (e.type)
            {
                case SDL_QUIT: return;
                case SDL_KEYDOWN:
                    if (keys[SDL_SCANCODE_ESCAPE] and Cartridge::loaded())
                        toggle_pause();
                    else if (pause)
                        menu->update(keys);
            }
        ************************/

        if (true) CPU::run_frame();
    }
}

int main(int argc, char *argv[]) {
    // GUI::load_settings();
    // GUI::init();
    // GUI::run();

    APU::init();
    run();

    return 0;
}
