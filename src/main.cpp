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

#include "easylogging++.hpp"
INITIALIZE_EASYLOGGINGPP

const int usecs_wait_time = 1000000;

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

class NonForkingNESEmulatorImpl final : public Emulator::Service { 
  Status play_game(ServerContext* context, ServerReaderWriter<VideoFrame, MachineState>* stream) override {
    LOG(DEBUG) << "start non-forking main loop" << endl;
    MachineState state;
    for (int i = 0; stream->Read(&state); i++) {
      VideoFrame frame;
      LOG(DEBUG) << "new state received" << endl;
      
      //Check to see if a cartridge is loaded, load if not
      if (!Cartridge::loaded()) {
        //Handle cases in which we're downloading the rom, or the rom is given as data 
        LOG(INFO) << "Beggining new game: " << state.nes_console_state().game().name().c_str()
          << " - " << state.nes_console_state().game().path().c_str() << endl;
        Cartridge::load(state.nes_console_state().game().path().c_str()); 
        APU::init();
      }
      //Run a frame, pass the state and frame variables by reference
      CPU::run_frame(state, frame);

      //Put the machine state into the VideoFrame
      frame.mutable_machine_state()->CopyFrom(state);
      LOG(DEBUG) << "Sending frame" << endl;
      stream->Write(frame);
      
      LOG(DEBUG) << "Continueing with main loop." << endl;
    }

    return Status::OK;
  }
};
class NESEmulatorImpl final : public Emulator::Service { 
  Status play_game(ServerContext* context, ServerReaderWriter<VideoFrame, MachineState>* stream) override {
    //Shared memory for ipc
    LOG(DEBUG) << "allocating shared memory" << endl;
    const int KB = 1024;
    const int MB = 1048576;
    void* shared_frame_buffer = create_shared_memory(MB);
    void* shared_state_buffer = create_shared_memory(MB);

    bool* frame_ready = (bool*) create_shared_memory(sizeof(bool));
    bool* state_ready = (bool*) create_shared_memory(sizeof(bool));

    LOG(DEBUG) << "initializing locks" << endl;
    *frame_ready = false;
    *state_ready = false;

    LOG(DEBUG) << "forking" << endl;
    int pid = fork();
    if (pid == 0) {
      //Child process
      
      LOG(DEBUG) << "child: starting main loop" << endl;
      for (int i = 0; true; i++) {
        LOG(DEBUG) << "child: check if state is ready - lock value is: " << *state_ready << endl;
        // Check to see if the state is ready
        if (*state_ready == true) {
          MachineState state; 
          VideoFrame frame;
          LOG(DEBUG) << "child: shared state buffer value is: " << (char*) shared_state_buffer << endl;
          LOG(DEBUG) << "child: parsing shared state buffer" << endl;

          // Parse the state out of shared memory
          if (state.ParseFromString(std::string((char*) shared_state_buffer))) {
            LOG(DEBUG) << "child: successfully parsed the state" << endl;
          } else {
            LOG(FATAL) << "child: failed to parse the state" << endl;
          }

          //Check to see if a cartridge is loaded, load if not
          if (!Cartridge::loaded()) {
            //Handle cases in which we're downloading the rom, or the rom is given as data 
            LOG(INFO) << "Beggining new game: " << state.nes_console_state().game().name().c_str()
              << " - " << state.nes_console_state().game().path().c_str() << endl;
            Cartridge::load(state.nes_console_state().game().path().c_str()); 
            APU::init();
          }
          //Run a frame, pass the state and frame variables by reference
          CPU::run_frame(state, frame);

          //Put the machine state into the VideoFrame
          frame.mutable_machine_state()->CopyFrom(state);

          //Serialize the VideoFrame and and copy it into shared memory
          std::string private_frame_buffer;
          if (frame.SerializeToString(&private_frame_buffer)) {
            memcpy((void*) shared_frame_buffer, (void*) private_frame_buffer.c_str(), 
                private_frame_buffer.length() + 1);
            LOG(DEBUG) << "child: frame serialization successfull - copying frame buffer of size " << sizeof(private_frame_buffer) << " to shared memory" << endl;
            LOG(DEBUG) << "child: shared frame buffer value is: " << (char*) shared_frame_buffer << endl;
          } else {
            LOG(FATAL) << "child: failed frame serialization" << endl;
          }

          LOG(DEBUG) << "child: update the shared locks" << endl;
          *state_ready = false;
          *frame_ready = true;
        } else {
          usleep(usecs_wait_time);
        }
      }
    } else if (pid > 0) {
      //Parent Process
      //while (stream->Read(shared_state_buffer)) {
      LOG(DEBUG) << "parent: start main loop" << endl;
      MachineState state;
      for (int i = 0; stream->Read(&state); i++) {
        std::string private_state_buffer;
        LOG(DEBUG) << "parent: Beginning serialization" << endl;
        //Received a new event from the user.
        //Serialize it and copy the resulting data into the shared memory
        if (state.SerializeToString(&private_state_buffer)) {
          memcpy((void*) shared_state_buffer, (void*) private_state_buffer.c_str(), 
              private_state_buffer.length() + 1);
          LOG(DEBUG) << "parent: state serialization successfull - copying state buffer of size " << sizeof(private_state_buffer) << " to shared memory" << endl;
          LOG(DEBUG) << "parent: shared state buffer value is: " << (char*) shared_state_buffer << endl;
        } else {
          LOG(FATAL) << "parent: failed state serialization" << endl;
        }
        //Inform the child that a new event is ready to be read out of memory.
        *state_ready = true;

        //Wait for the child process to make a new VideoFrame available
        while (*frame_ready == false) { 
          LOG(DEBUG) << "parent: frame lock value is: " << *frame_ready << " sleeping" << endl;
          usleep(usecs_wait_time); 
        }

        if (*frame_ready == true) {
          LOG(DEBUG) << "parent: frame is ready" << endl;
          LOG(DEBUG) << "parent: update the frame lock" << endl;
          *frame_ready = false;
          
          //Parse the video frame out of memory
          VideoFrame frame;
          LOG(DEBUG) << "parent: shared frame buffer value is: " << (char*) shared_frame_buffer << endl;
          if (frame.ParseFromString(std::string((char*) shared_frame_buffer))) {
            LOG(DEBUG) << "parent: successfully parsed the frame" << endl;
          } else {
            LOG(ERROR) << "parent: failed to parse the frame" << endl;
          }
          LOG(DEBUG) << "parent: Game - " << state.nes_console_state().game().name() << " | "
            << frame.raw_frame().data() << endl;
          //Send the frame data over the wire
          stream->Write(frame);
        }
        
        //Make sure that the child process has released the machine state before continuing
        while (*state_ready == true) { 
          LOG(DEBUG) << "parent: waiting for child to release state" << endl;
          usleep(usecs_wait_time); 
        }
        LOG(DEBUG) << "parent: state released. Continueing with main loop." << endl;
      }
    }

    munmap(shared_frame_buffer, sizeof(VideoFrame));
    munmap(shared_state_buffer, sizeof(MachineState));
    munmap(frame_ready, sizeof(bool));
    munmap(state_ready, sizeof(bool));

    return Status::OK;
  }
};

void configureLoggerFromFile(char* path) {
  // Load configuration from file
  el::Configurations conf(path);
  // Reconfigure single logger
  el::Loggers::reconfigureLogger("default", conf);
  // Actually reconfigure all loggers instead
  el::Loggers::reconfigureAllLoggers(conf);
  // Now all the loggers will use configuration from file

}

void configureLoggerFromCode() {
  el::Configurations defaultConf;
  defaultConf.setToDefault();
  // Values are always std::string
  defaultConf.set(el::Level::Info, el::ConfigurationType::Enabled, "%datetime %level %msg");
  defaultConf.set(el::Level::Info, el::ConfigurationType::Enabled, "true");
  defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
  // default logger uses default configurations
  el::Loggers::reconfigureLogger("default", defaultConf);
  // To set GLOBAL configurations you may use
  //defaultConf.setGlobally(
  //  el::ConfigurationType::Format, "%date %msg");
  el::Loggers::addFlag(el::LoggingFlag::HierarchicalLogging);
  el::Loggers::setDefaultConfigurations(defaultConf, true);
  el::Loggers::reconfigureAllLoggers(defaultConf);
}


int main(int argc, char *argv[]) {
  START_EASYLOGGINGPP(argc, argv);
  configureLoggerFromFile("./easylogging.conf");

  std::string server_address("0.0.0.0:50051");
  NonForkingNESEmulatorImpl emulator;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&emulator);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  LOG(INFO) << "Server listening on " << server_address << std::endl;
  server->Wait();

  return 0;
}
