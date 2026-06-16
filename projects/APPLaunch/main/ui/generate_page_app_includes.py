#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

import argparse
import os
import sys

PAGE_APP_DIR = "page_app"
DEFAULT_OUTPUT_FILE = "page_app.h"


def generate_includes(output_file=DEFAULT_OUTPUT_FILE):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    page_app_path = os.path.join(script_dir, PAGE_APP_DIR)
    output_path = output_file
    if not os.path.isabs(output_path):
        output_path = os.path.join(script_dir, output_path)

    if not os.path.isdir(page_app_path):
        raise FileNotFoundError(f"Directory '{PAGE_APP_DIR}' not found.")

    hpp_files = sorted([f for f in os.listdir(page_app_path) if f.endswith(".hpp")])
    if not hpp_files:
        raise RuntimeError(f"No .hpp files found in '{PAGE_APP_DIR}'.")

    new_content = """/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

"""
    for hpp_file in hpp_files:
        new_content += f'#include "{PAGE_APP_DIR}/{hpp_file}"\n'

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    if os.path.exists(output_path):
        with open(output_path, "r") as f:
            old_content = f.read()
        if old_content == new_content:
            print(f"{output_path} is already up to date. No changes made.")
            return

    with open(output_path, "w") as f:
        f.write(new_content)

    print(f"Successfully updated {output_path} with {len(hpp_files)} includes:")
    for hpp_file in hpp_files:
        print(f"  - {hpp_file}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=DEFAULT_OUTPUT_FILE)
    args = parser.parse_args()
    generate_includes(args.output)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"generate_page_app_includes.py: {exc}", file=sys.stderr)
        sys.exit(1)
