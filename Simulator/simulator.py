#!/usr/bin/env python3
"""Simulate the Pump System's Master UART and pump status events on Linux."""

import argparse
import os
import select
import struct
import sys
import time
import tty
from dataclasses import dataclass
from enum import IntEnum


# Wire format and commands from Include/UARTConnectionConfig.h.
FRAME = struct.Struct("<BBB8s")
UART_DATA = 1
MASTER_ADDRESS = 0
MONITOR_TIMEOUT = 6.0


class Opcode(IntEnum):
    MASTER_REGISTRATION = 0x10
    MASTER_RESPONSE = 0x11
    REQUEST = 0x12
    RESPONSE = 0x13
    DEVICE_ONLINE = 0x20
    DEVICE_OFFLINE = 0x21
    FORWARD_LOCK_REACHED = 0x30
    REVERSE_LOCK_REACHED = 0x32
    DATA_PACKAGE = 0x40
    CLEAR_DATA_BUFFER = 0x41
    START_EXECUTION = 0x42


LIMIT_STATES = {
    "clear": Opcode.DEVICE_ONLINE,
    "forward": Opcode.FORWARD_LOCK_REACHED,
    "reverse": Opcode.REVERSE_LOCK_REACHED,
}

COMMANDS = """Commands:
  master on|off                             Enable Master or stop all UART replies
  pump ID online|offline                    Connect or disconnect a pump
  pump ID forward|reverse|clear             Set or release a limit switch
  status                                   Show Master and pumps
  help                                     Show commands
  quit                                     Close the virtual port and exit"""


@dataclass
class Pump:
    online: bool = True
    limit: str = "clear"

    def status_opcode(self):
        if not self.online:
            return Opcode.DEVICE_OFFLINE
        return LIMIT_STATES[self.limit]


class Simulator:
    def __init__(self, pump_ids):
        self.pumps = {pump_id: Pump() for pump_id in sorted(set(pump_ids))}
        self.master_enabled = True
        self.session_deadline = None
        self.received = bytearray()
        self.pending = bytearray()

    def send_status(self, device, opcode):
        self.pending.extend(FRAME.pack(UART_DATA, device, 1, bytes([opcode])))

    def expire_session(self):
        if self.session_deadline is None:
            return None
        remaining = self.session_deadline - time.monotonic()
        if remaining > 0:
            return remaining
        self.session_deadline = None
        self.pending.clear()
        print("Monitor session timed out.")
        return None

    def receive(self, data):
        self.received.extend(data)
        while len(self.received) >= FRAME.size:
            message_type, device, size, payload = FRAME.unpack_from(self.received)
            del self.received[:FRAME.size]
            if self.master_enabled and message_type == UART_DATA and 1 <= size <= len(payload):
                self.handle_frame(device, payload[:size])

    def handle_frame(self, device, payload):
        opcode = payload[0]
        if device == MASTER_ADDRESS and payload == bytes([Opcode.MASTER_REGISTRATION]):
            self.session_deadline = time.monotonic() + MONITOR_TIMEOUT
            self.send_status(MASTER_ADDRESS, Opcode.MASTER_RESPONSE)
            for pump_id, pump in self.pumps.items():
                self.send_status(pump_id, pump.status_opcode())
            print("Monitor registered.")
            return

        if self.session_deadline is None:
            return
        if device == MASTER_ADDRESS and payload == bytes([Opcode.REQUEST]):
            self.session_deadline = time.monotonic() + MONITOR_TIMEOUT
            self.send_status(MASTER_ADDRESS, Opcode.RESPONSE)
            return

        if device in self.pumps and opcode in (
            Opcode.CLEAR_DATA_BUFFER, Opcode.DATA_PACKAGE, Opcode.START_EXECUTION
        ):
            print(f"RX pump {device}: {Opcode(opcode).name} [{payload.hex(' ')}]")

    def command(self, line):
        args = line.lower().split()
        if not args:
            return True
        if args == ["quit"]:
            return False
        if args == ["help"]:
            print(COMMANDS)
        elif args == ["status"]:
            if not self.master_enabled:
                master = "off"
            elif self.session_deadline is None:
                master = "waiting for Monitor"
            else:
                master = "connected"
            print(f"Master: {master}")
            for pump_id, pump in self.pumps.items():
                link = "online" if pump.online else "offline"
                print(f"Pump {pump_id}: {link}, limit: {pump.limit}")
        elif args in (["master", "on"], ["master", "off"]):
            self.master_enabled = args[1] == "on"
            if not self.master_enabled:
                self.session_deadline = None
                self.pending.clear()
            print(f"Master: {args[1]}")
        elif len(args) == 3 and args[0] == "pump":
            self.change_pump(args[1], args[2])
        else:
            print("Unknown command. Type 'help'.")
        return True

    def change_pump(self, pump_id_text, state):
        try:
            pump_id = int(pump_id_text)
            pump = self.pumps[pump_id]
        except (ValueError, KeyError):
            print(f"Unknown pump: {pump_id_text}. Type 'status' for pump IDs.")
            return

        previous = pump.status_opcode()
        if state in ("online", "offline"):
            pump.online = state == "online"
        elif state in LIMIT_STATES:
            pump.limit = state
        else:
            print("Use online, offline, forward, reverse or clear.")
            return

        current = pump.status_opcode()
        if self.session_deadline is not None and current != previous:
            self.send_status(pump_id, current)
        link = "online" if pump.online else "offline"
        print(f"Pump {pump_id}: {link}, limit: {pump.limit}")

    def run(self, uart_fd):
        console = sys.stdin.fileno()
        commands = bytearray()
        while True:
            timeout = self.expire_session()
            readers = [uart_fd] if console is None else [uart_fd, console]
            writers = [uart_fd] if self.pending else []
            readable, writable, _ = select.select(readers, writers, [], timeout)
            self.expire_session()
            if uart_fd in readable:
                self.receive(os.read(uart_fd, 4096))
            if console in readable:
                data = os.read(console, 4096)
                commands.extend(data)
                while b"\n" in commands:
                    line, _, commands = commands.partition(b"\n")
                    if not self.command(line.decode(errors="replace")):
                        return
                if not data:
                    if not self.command(commands.decode(errors="replace")):
                        return
                    console = None
            if uart_fd in writable and self.pending:
                # A nonblocking PTY can accept only part of the queued bytes.
                try:
                    written = os.write(uart_fd, self.pending)
                except BlockingIOError:
                    continue
                del self.pending[:written]


def parse_pump_id(value):
    pump_id = int(value)
    if not 101 <= pump_id <= 255:
        raise argparse.ArgumentTypeError("pump ID must be between 101 and 255")
    return pump_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pump_ids", nargs="*", type=parse_pump_id,
                        help="IDs of virtual pumps (default: 101 102 103)")
    args = parser.parse_args()
    simulator = Simulator(args.pump_ids or [101, 102, 103])

    sys.stdout.reconfigure(line_buffering=True)
    uart_fd, port_fd = os.openpty()
    # Keep the slave open so Monitor can close and reopen the same path.
    try:
        tty.setraw(port_fd)
        os.set_blocking(uart_fd, False)
        print(f"UART port: {os.ttyname(port_fd)}")
        print("Master is on. Type 'help' to list commands.")
        simulator.run(uart_fd)
    except KeyboardInterrupt:
        pass
    finally:
        os.close(uart_fd)
        os.close(port_fd)


if __name__ == "__main__":
    main()
