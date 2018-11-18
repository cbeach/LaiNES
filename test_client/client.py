import json
import math
import multiprocessing
import os
import time
import sys

from glob import glob
from sys import exit

import grpc
import cv2
import numpy as np

from google.protobuf.json_format import MessageToJson
import common_pb2_grpc
import common_pb2
import controller
import deep_thought_pb2_grpc
import deep_thought_pb2
import nes_pb2_grpc
import nes_pb2


def save_play(frames, actions, meta_data, as_pngs=False):
    data_dir = os.environ['DATA_DIR']
    play_dir = os.path.join(data_dir, 'game_playing/play_data')
    plays = []
    for i in glob(play_dir + '/*'):
        plays.append(int(os.path.basename(i)))
    if len(plays) == 0:
        latest = 1
    else:
        latest = max(plays) + 1
    new_play_dir = os.path.join(play_dir, str(latest))
    os.mkdir(new_play_dir)

    # Write the frames
    if as_pngs is True:
        for i, f in enumerate(frames):
            cv2.imwrite(os.path.join(new_play_dir, '{}.png'.format(i)), f)
    else:
        np.save(os.path.join(new_play_dir, 'frames'), np.array(frames))

    # Write the actions
    with open(os.path.join(new_play_dir, 'actions.json'), 'w') as fp:
        actions = [json.loads(MessageToJson(i)) for i in actions]
        json.dump(actions, fp)

    # Write the meta data
    with open(os.path.join(new_play_dir, 'meta_data.json'), 'w') as fp:
        json.dump(meta_data, fp)


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
                    path="/home/app/smb.nes"
                )
            )
        )
        yield ms

def play_game_stream(stub, event_handler=None):
    device = controller.select_device()
    #async_events(device, nes_con)
    cel = controller.ControllerEventLoop(device, nes_con)
    cel.start()

    if event_handler is not None:
        m_state = get_input_state(event_handler, frame_rate=60)
    else:
        m_state = generate_constant_machine_states()

    responses = stub.play_game(m_state)
    counter = 0
    frames = []
    actions = []
    debounce = False

    for i, response in enumerate(responses):
        counter += 1
        img = np.reshape(np.frombuffer(response.raw_frame.data, dtype='uint8'), (240, 256, 4))
        img = img[:, :, :-1]
        act = response.machine_state.nes_console_state.player1_input
        game = response.machine_state.nes_console_state.game
        frames.append(img)
        actions.append(act)

        if act.select is True and debounce is False:
            debounce = True
            meta_data = {
                'game': {
                    'name': game.name,
                    'path': game.path,
                },
                'frame_count': counter,
            }
            save_play(frames, actions, meta_data)
            frames = []
            actions = []
        elif act.select is False and debounce is True:
            debounce = False
        cv2.imshow('game session', img)
        if cv2.waitKey(1) == 27:
            break


if __name__ == '__main__':
    nes_con = controller.NESController()
    channel = grpc.insecure_channel('localhost:50051')
    stub = deep_thought_pb2_grpc.EmulatorStub(channel)
    play_game_stream(stub, event_handler=nes_con)
