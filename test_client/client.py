# Copyright 2015 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""The Python implementation of the gRPC route guide client."""

from __future__ import print_function

from sys import exit

import grpc

import common_pb2_grpc
import common_pb2
import deep_thought_pb2_grpc
import deep_thought_pb2
import nes_pb2_grpc
import nes_pb2


def generate_machine_states():
  ms = deep_thought_pb2.MachineState(
    nes_console_state=nes_pb2.NESConsoleState(
      game=common_pb2.Game(
        name="Super Mario Brothers",
        path="/home/mcsmash/dev/emulators/LaiNES/smb.nes"
      )
    )
  )
  while True:
      yield ms


def play_game_stream(stub):
  m_state = generate_machine_states()
  responses = stub.play_game(m_state)
  counter = 0
  for response in responses:
    counter += 1
    print('frame# {}: {}'.format(i, response.raw_frame.data))


def run():
  channel = grpc.insecure_channel('localhost:50051')
  stub = deep_thought_pb2_grpc.EmulatorStub(channel)
  print("-------------- play_game --------------")
  play_game_stream(stub)
  #except KeyboardInterupt:
  #  exit(0)


if __name__ == '__main__':
  run()
