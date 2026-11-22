#!/usr/bin/env python3
"""Send one Codex chat's lifecycle state to a StopWatch over USB or the LAN."""

import argparse
from contextlib import closing
import hmac
from ipaddress import IPv4Address
import json
import os
from pathlib import Path
import re
import socket
import sqlite3
import time
import uuid

TURN_STATES = {
    "task_started": b"1",
    "task_complete": b"2",
    "turn_aborted": b"3",
}
LAN_PORT = 45255
DEVICE_NAME_LENGTH = 16
NAMED_DEVICE_BIT = 1 << 63
HEARTBEAT_SECONDS = 1
REFRESH_REQUEST = b"[codex] refresh"


def lan_packet(state, key, sequence, device=None):
    heartbeat = sequence.to_bytes(8, "big") + state
    if device is None:
        payload = b"SWC1" + heartbeat
    else:
        device_id, name = device
        payload = b"SWC2" + device_id.to_bytes(8, "big") + heartbeat + name.ljust(DEVICE_NAME_LENGTH, b"\0")
    return payload + hmac.digest(key, payload, "sha256")[:16]


def lan_device(key, label=None):
    hostname = socket.gethostname()
    if label is None:
        label = re.sub(r"[^ -~]+", "-", hostname.split(".", 1)[0]).strip(" -")[:DEVICE_NAME_LENGTH] or "Codex"
    else:
        label = label.strip()
    if not (1 <= len(label) <= DEVICE_NAME_LENGTH and label.isascii() and label.isprintable()):
        raise ValueError(f"Device name must be 1–{DEVICE_NAME_LENGTH} printable ASCII characters.")

    # The watch sees a pairing-specific identifier instead of the raw hardware address.
    identity = uuid.getnode().to_bytes(6, "big") + hostname.encode("utf-8")
    digest = hmac.digest(key, b"stopwatch-device\0" + identity, "sha256")
    device_id = int.from_bytes(digest[:8], "big") | NAMED_DEVICE_BIT
    return device_id, label.encode("ascii")


def read_lan_key(key_file=None):
    if key_file is not None:
        key = key_file.read_text(encoding="utf-8-sig").strip()
        if re.fullmatch(r"[0-9a-fA-F]{64}", key) is None:
            raise ValueError("The LAN key file must contain exactly 64 hexadecimal characters.")
        return bytes.fromhex(key)

    config = Path(__file__).resolve().parents[1] / "include" / "LocalConfig.h"
    text = config.read_text(encoding="utf-8-sig")
    match = re.search(r'^\s*#define\s+SW_CODEX_LAN_KEY\s+"([0-9a-fA-F]{64})"\s*$', text, re.M)
    if match is None:
        raise ValueError("Missing 64-digit SW_CODEX_LAN_KEY in include/LocalConfig.h.")
    return bytes.fromhex(match[1])


def find_rollout(codex_home, thread_id):
    sessions = (codex_home / "sessions").resolve()
    for database in sorted(codex_home.glob("state_*.sqlite"), reverse=True):
        try:
            with closing(sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True, timeout=1)) as db:
                row = db.execute("SELECT rollout_path FROM threads WHERE id = ? AND archived = 0",
                                 (thread_id,)).fetchone()
        except sqlite3.Error:
            continue
        if row:
            try:
                rollout = Path(row[0]).resolve()
                # Windows may return the same file with a \\?\ volume prefix.
                if rollout.is_file() and any(parent.samefile(sessions) for parent in rollout.parents):
                    return rollout
            except OSError:
                continue
    return None


class ChatActivity:
    """Tail lifecycle records from the selected chat's local rollout."""

    def __init__(self, rollout):
        self.rollout = rollout
        self.identity = None
        self.offset = 0
        self.state = b"0"
        self.progress_at = time.monotonic()

    def poll(self):
        try:
            with self.rollout.open("rb") as stream:
                info = os.fstat(stream.fileno())
                identity = (info.st_dev, info.st_ino)
                if identity != self.identity or info.st_size < self.offset:
                    self.identity, self.offset, self.state = identity, 0, b"0"
                previous_offset = self.offset
                stream.seek(self.offset)
                while (line := stream.readline()).endswith(b"\n"):
                    self.offset = stream.tell()
                    if b'"event_msg"' not in line or not any(kind.encode() in line for kind in TURN_STATES):
                        continue
                    try:
                        event = json.loads(line)
                    except (ValueError, UnicodeDecodeError):
                        continue
                    if event.get("type") == "event_msg":
                        self.state = TURN_STATES.get(event.get("payload", {}).get("type"), self.state)
                if self.offset != previous_offset:
                    self.progress_at = time.monotonic()
        except OSError:
            return b"4"

        if self.state == b"1" and time.monotonic() - self.progress_at > 900:
            return b"4"
        return self.state


def wait_for_refresh(board):
    if board is None or not board.is_open:
        time.sleep(HEARTBEAT_SECONDS)
        return
    deadline = time.monotonic() + HEARTBEAT_SECONDS
    tail = b""
    while time.monotonic() < deadline:
        received = tail + board.read_all()
        if REFRESH_REQUEST in received:
            return
        tail = received[-(len(REFRESH_REQUEST) - 1):]
        time.sleep(0.05)


def run(port, activity, targets=(), key=None, device=None):
    last_state = None
    usb_waiting = False
    lan_sending = None
    board = None
    udp = None
    if port:
        import serial

        board = serial.Serial(baudrate=115200, timeout=0, write_timeout=0.5)
        board.port = port
        board.dtr = board.rts = False
    try:
        if targets:
            udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            udp.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        while True:
            state = activity.poll()
            if udp is not None:
                message = lan_packet(state, key, time.monotonic_ns(), device)
                sent = False
                for target in targets:
                    try:
                        udp.sendto(message, (target, LAN_PORT))
                        sent = True
                    except OSError:
                        continue
                if sent != lan_sending:
                    print("Sending LAN status" if sent else "Waiting for the LAN interface", flush=True)
                    lan_sending = sent
            if board is not None:
                try:
                    if not board.is_open:
                        board.open()
                        usb_waiting = False
                        print(f"StopWatch connected on {port}", flush=True)
                    board.write(state)
                except OSError:
                    board.close()
                    if not usb_waiting:
                        print(f"Waiting for the StopWatch on {port}", flush=True)
                        usb_waiting = True
            if state != last_state:
                print(f"Codex state {state.decode('ascii')}", flush=True)
                last_state = state
            try:
                wait_for_refresh(board)
            except OSError:
                board.close()
                time.sleep(HEARTBEAT_SECONDS)
    finally:
        if board is not None:
            board.close()
        if udp is not None:
            udp.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="StopWatch USB serial port, for example COM5")
    parser.add_argument("--lan", action="store_true", help="Send status over the local IPv4 network.")
    parser.add_argument("--lan-target", type=IPv4Address, action="append",
                        help="LAN broadcast or device address. Defaults to 255.255.255.255.")
    parser.add_argument("--lan-key-file", type=Path, help="File containing only the StopWatch LAN pairing key.")
    parser.add_argument("--device-name", help="LAN device name on the StopWatch. Defaults to this computer's name.")
    parser.add_argument("--legacy-lan", action="store_true", help="Use the original LAN format for older firmware.")
    parser.add_argument("--thread-id", default=os.environ.get("CODEX_THREAD_ID"),
                        help="Codex chat ID. Defaults to the chat that launched this command.")
    args = parser.parse_args()
    if not args.port and not args.lan:
        parser.error("Select --port, --lan, or both.")
    if not args.lan and (args.lan_target or args.lan_key_file or args.device_name is not None or args.legacy_lan):
        parser.error("LAN options require --lan.")
    if args.legacy_lan and args.device_name is not None:
        parser.error("Device names require the current LAN format.")
    if not args.thread_id:
        parser.error("Pass --thread-id or run the bridge from the desired Codex chat.")
    codex_home = Path(os.environ.get("CODEX_HOME") or Path.home() / ".codex")
    rollout = find_rollout(codex_home, args.thread_id)
    if rollout is None:
        parser.error("The chat does not have a readable local session.")
    targets = ()
    key, device = None, None
    if args.lan:
        try:
            key = read_lan_key(args.lan_key_file)
            if not args.legacy_lan:
                device = lan_device(key, args.device_name)
        except (OSError, ValueError) as error:
            parser.error(str(error))
        targets = tuple(str(target) for target in (args.lan_target or ("255.255.255.255",)))
    try:
        run(args.port, ChatActivity(rollout), targets, key, device)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
