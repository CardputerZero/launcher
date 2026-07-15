#!/usr/bin/env python3
import argparse
import getpass
import pathlib
import sys
import time

import zmq


KEY_CODES = {
    "esc": 1,
    "enter": 28,
    "left": 105,
    "right": 106,
    "up": 103,
    "down": 108,
}


def key_code(value: str) -> int:
    lowered = value.lower()
    if lowered in KEY_CODES:
        return KEY_CODES[lowered]
    return int(value, 0)


def request(socket: zmq.Socket, text: str) -> list[bytes]:
    socket.send_string(text)
    parts = socket.recv_multipart()
    if not parts or not parts[0].startswith(b"OK"):
        message = parts[0].decode("utf-8", "replace") if parts else "empty reply"
        raise RuntimeError(message)
    return parts


def send_key(socket: zmq.Socket, code: int, state: int, mods: int = 0) -> None:
    request(socket, f"key {code} {state} {mods}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Control a cp0_lvgl device over ZMQ RPC.")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5557")
    parser.add_argument("--timeout", type=int, default=3000, help="RPC timeout in milliseconds")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("ping")
    screenshot = sub.add_parser("screenshot")
    screenshot.add_argument("output", type=pathlib.Path)
    key = sub.add_parser("key")
    key.add_argument("key", type=key_code)
    key.add_argument("state", choices=("up", "down", "repeat"))
    key.add_argument("--mods", type=int, default=0)
    tap = sub.add_parser("tap")
    tap.add_argument("key", type=key_code)
    tap.add_argument("--duration", type=float, default=0.08)
    text = sub.add_parser("text")
    text.add_argument("value")
    password = sub.add_parser("password")
    password.add_argument("value", nargs="?")
    hold = sub.add_parser("hold")
    hold.add_argument("key", type=key_code)
    hold.add_argument("seconds", type=float)
    args = parser.parse_args()

    context = zmq.Context.instance()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.LINGER, 0)
    socket.setsockopt(zmq.RCVTIMEO, args.timeout)
    socket.setsockopt(zmq.SNDTIMEO, args.timeout)
    socket.connect(args.endpoint)

    try:
        if args.command == "ping":
            print(request(socket, "ping")[0].decode())
        elif args.command == "screenshot":
            parts = request(socket, "screenshot")
            if len(parts) != 2:
                raise RuntimeError("screenshot reply has no image frame")
            args.output.write_bytes(parts[1])
            print(f"saved {args.output} ({len(parts[1])} bytes)")
        elif args.command == "key":
            states = {"up": 0, "down": 1, "repeat": 2}
            send_key(socket, args.key, states[args.state], args.mods)
        elif args.command == "tap":
            send_key(socket, args.key, 1)
            time.sleep(max(0.0, args.duration))
            send_key(socket, args.key, 0)
        elif args.command == "text":
            request(socket, f"text {args.value}")
        elif args.command == "password":
            value = args.value if args.value is not None else getpass.getpass("Password: ")
            request(socket, f"password {value}")
        elif args.command == "hold":
            send_key(socket, args.key, 1)
            try:
                time.sleep(max(0.0, args.seconds))
            finally:
                send_key(socket, args.key, 0)
    except (RuntimeError, zmq.ZMQError, OSError) as error:
        print(f"cp0_rpc: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
