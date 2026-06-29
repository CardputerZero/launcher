#!/usr/bin/env python3
import argparse
import sys

import zmq


def main() -> int:
    parser = argparse.ArgumentParser(description="Subscribe to cp0_lvgl ZeroMQ logs.")
    parser.add_argument("endpoint", nargs="?", default="tcp://127.0.0.1:5556",
                        help="ZeroMQ PUB endpoint, e.g. tcp://192.168.1.23:5556")
    parser.add_argument("--topic", default="", help="Topic prefix to subscribe to, e.g. bt")
    args = parser.parse_args()

    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.SUB)
    sock.setsockopt_string(zmq.SUBSCRIBE, args.topic)
    sock.connect(args.endpoint)
    print(f"subscribed endpoint={args.endpoint} topic={args.topic or '*'}", flush=True)

    try:
        while True:
            parts = sock.recv_multipart()
            if len(parts) >= 2:
                topic = parts[0].decode("utf-8", "replace")
                message = parts[1].decode("utf-8", "replace")
                print(f"[{topic}] {message}", flush=True)
            elif parts:
                print(parts[0].decode("utf-8", "replace"), flush=True)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
