from sys import exit
from glob import glob

import math
import multiprocessing
import os
import time

from termcolor import cprint

import grpc
import cv2
import numpy as np

import common_pb2_grpc
import common_pb2
import controller
import deep_thought_pb2_grpc
import deep_thought_pb2
import nes_pb2_grpc
import nes_pb2


data_dir = "/home/mcsmash/dev/data/game_playing/frames/super_mario_bros/plays/"

def get_play_number():
    plays = [int(os.path.basename(p)) for p in glob('{}/*'.format(data_dir))]
    if len(plays) == 0:
        return 1
    else:
        return sorted(plays, reverse=True)[0] + 1


def generate_constant_machine_states():
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

def get_input_state(controller, frame_rate=math.inf):
    while True:
        time.sleep(1 / frame_rate)
        ms = deep_thought_pb2.MachineState(
            nes_console_state=nes_pb2.NESConsoleState(
                player1_input=controller.state(),
                game=common_pb2.Game(
                    name="Super Mario Brothers",
                    #path="/home/app/smb.nes"
                    path="/home/mcsmash/dev/emulators/LaiNES/smb.nes"
                )
            )
        )
        yield ms

def play_game_stream(stub, event_handler=None):
    device = controller.select_device()
    play_number = get_play_number()
    cprint("starting play number {0:04d}".format(play_number))
    play_dir = os.path.join(data_dir, '{0:04d}'.format(play_number))
    cprint("play data is in {}".format(play_dir))
    if not os.path.exists(play_dir):
        os.mkdir(play_dir)

    #async_events(device, nes_con)
    cel = controller.ControllerEventLoop(device, nes_con)
    cel.start()

    print('hello')

    if event_handler is not None:
        m_state = get_input_state(event_handler, frame_rate=60)
    else:
        m_state = generate_constant_machine_states()

    responses = stub.play_game(m_state)
    for i, response in enumerate(responses):
        img = np.reshape(np.frombuffer(response.raw_frame.data, dtype='uint8'), (240, 256, 4))
        cv2.imshow('game session', img)
        cv2.imwrite(os.path.join(play_dir, '{}.png'.format(i)), img[:, :,  :-1])
        if cv2.waitKey(1) == 27:
            break


if __name__ == '__main__':
    nes_con = controller.NESController()
    channel = grpc.insecure_channel('localhost:50051')
    stub = deep_thought_pb2_grpc.EmulatorStub(channel)
    play_game_stream(stub, event_handler=nes_con)
