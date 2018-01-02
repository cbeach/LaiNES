// #include "Sound_Queue.h"
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"

#include "deep_thought.grpc.pb.h"
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using org::beachc::deep_thought::Emulator;
using org::beachc::deep_thought::MachineState;
using org::beachc::deep_thought::VideoFrame;

void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_ANONYMOUS | MAP_SHARED;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, 0, 0);
}

class NESEmulatorImpl final : public Emulator::Service {
  Status play_game(ServerContext* context, ServerReaderWriter<VideoFrame, MachineState>* stream) override {

    //Shared memory for ipc
    VideoFrame* shared_frame = (VideoFrame*) create_shared_memory(sizeof(VideoFrame));
    MachineState* shared_state = 
      (MachineState*) create_shared_memory(sizeof(MachineState));

    bool* frame_ready = (bool*) create_shared_memory(sizeof(bool));
    bool* state_ready = (bool*) create_shared_memory(sizeof(bool));
    *frame_ready = false;
    *state_ready = false;

    int pid = fork();
    if (pid == 0) {
      //Child process
      
      //load cartridge
      APU::init();
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
        if (*state_ready == true) {
          //
          CPU::run_frame(shared_state, shared_frame);
          *state_ready = false;
          *frame_ready = true;
        }
      }
    } else if (pid > 0) {
      //Parent Process
      while (stream->Read(shared_state)) {
        //Read a new machine state off of the network

        //Wait for the child process to make a new VideoFrame available
        while (*frame_ready == false) { usleep(10); }
        
        //Write the new frame to the network
        stream->Write(*shared_frame);
        
        //Wait for the child process to finish reading the machine state before continuing
        while (*state_ready == true) { usleep(10); }
      }
    }
    munmap(shared_frame, sizeof(VideoFrame));
    munmap(shared_state, sizeof(MachineState));
    munmap(frame_ready, sizeof(bool));
    munmap(state_ready, sizeof(bool));

    return Status::OK;
  }
};

int main(int argc, char *argv[]) {
    // GUI::load_settings();
    // GUI::init();
    // GUI::run();
  std::string server_address("0.0.0.0:50051");

  NESEmulatorImpl emulator;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&emulator);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();

  return 0;
}
