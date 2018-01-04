// #include "Sound_Queue.h"
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <iostream>

#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"

#include "deep_thought.grpc.pb.h"
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

using namespace std;

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
using org::beachc::deep_thought::RawRGB32;

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
    cout << "allocating shared memory" << endl;
    const int KB = 1024;
    const int MB = 1048576;
    void* shared_frame_buffer = create_shared_memory(MB);
    void* shared_state_buffer = create_shared_memory(MB);

    bool* frame_ready = (bool*) create_shared_memory(sizeof(bool));
    bool* state_ready = (bool*) create_shared_memory(sizeof(bool));

    cout << "initializing locks" << endl;
    *frame_ready = false;
    *state_ready = false;

    cout << "forking" << endl;
    int pid = fork();
    if (pid == 0) {
      //Child process
      
      cout << "child: starting main loop" << endl;
      for (int i = 0; true; i++) {
        cout << "child: check if state is ready - lock value is: " << *state_ready << endl;
        if (*state_ready == true) {
          MachineState state; 
          VideoFrame frame;
          cout << "child: shared state buffer value is: " << (char*) shared_state_buffer << endl;
          cout << "child: parsing shared state buffer" << endl;
          if (state.ParseFromString(std::string((char*) shared_state_buffer))) {
            cout << "child: successfully parsed the state" << endl;
          } else {
            cout << "child: failed to parse the state" << endl;
          }
          std::string private_frame_buffer;
          std::string data = "hello, world: " + std::to_string(i);
          cout << "child: update the shared_frame_buffer" << endl;
          frame.mutable_raw_frame()->set_data(data);
          cout << "child: Game - " << state.nes_console_state().game().name() << " | "
            << frame.raw_frame().data() << endl;
          if (!Cartridge::loaded()) {
            //Handle cases in which we're downloading the rom, or the rom is given as data 
            cout << "loading: " 
              << state.nes_console_state().game().name() << endl; 

            Cartridge::load(state.nes_console_state().game().path().c_str()); 
            APU::init();

          }
          CPU::run_frame(state, frame);
          if (frame.SerializeToString(&private_frame_buffer)) {
            memcpy((void*) shared_frame_buffer, (void*) private_frame_buffer.c_str(), 
                private_frame_buffer.length() + 1);
            cout << "child: frame serialization successfull - copying frame buffer of size " << sizeof(private_frame_buffer) << " to shared memory" << endl;
            cout << "child: shared frame buffer value is: " << (char*) shared_frame_buffer << endl;
          } else {
            cout << "child: failed frame serialization" << endl;
          }
          cout << "child: update the shared locks" << endl;
          *state_ready = false;
          *frame_ready = true;
        } else {
          usleep(1000000);
        }
      }
    } else if (pid > 0) {
      //Parent Process
      //while (stream->Read(shared_state_buffer)) {
      cout << "parent: start main loop" << endl;
      MachineState state;
      for (int i = 0; stream->Read(&state); i++) {
        std::string private_state_buffer;
        cout << "parent: Beginning serialization" << endl;
        if (state.SerializeToString(&private_state_buffer)) {
          memcpy((void*) shared_state_buffer, (void*) private_state_buffer.c_str(), 
              private_state_buffer.length() + 1);
          cout << "parent: state serialization successfull - copying state buffer of size " << sizeof(private_state_buffer) << " to shared memory" << endl;
          cout << "parent: shared state buffer value is: " << (char*) shared_state_buffer << endl;
        } else {
          cout << "parent: failed state serialization" << endl;
        }
        *state_ready = true;
        //Read a new machine state off of the network
        //Wait for the child process to make a new VideoFrame available
        while (*frame_ready == false) { 
          cout << "parent: frame lock value is: " << *frame_ready << " sleeping" << endl;
          usleep(1000000); 
        }
        if (*frame_ready == true) {
          cout << "parent: frame is ready" << endl;
          cout << "parent: update the frame lock" << endl;
          *frame_ready = false;
          
          //Write the new frame to the network

          VideoFrame frame;
          cout << "parent: shared frame buffer value is: " << (char*) shared_frame_buffer << endl;
          if (frame.ParseFromString(std::string((char*) shared_frame_buffer))) {
            cout << "parent: successfully parsed the frame" << endl;
          } else {
            cout << "parent: failed to parse the frame" << endl;
          }
          cout << "parent: Game - " << state.nes_console_state().game().name() << " | "
            << frame.raw_frame().data() << endl;
          stream->Write(frame);
        }
        
        //Wait for the child process to finish reading the machine state before continuing
        while (*state_ready == true) { 
          cout << "parent: waiting for child to release state" << endl;
          usleep(1000000); 
        }
        cout << "parent: state released. Continueing with main loop." << endl;
        if (i > 100) {
          break;
        }
      }
    }
    munmap(shared_frame_buffer, sizeof(VideoFrame));
    munmap(shared_state_buffer, sizeof(MachineState));
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
