#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

import os

PAGE_APP_DIR = "page_app"
OUTPUT_FILE = "page_app.h"

def generate_includes():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    page_app_path = os.path.join(script_dir, PAGE_APP_DIR)
    output_path = os.path.join(script_dir, OUTPUT_FILE)

    if not os.path.isdir(page_app_path):
        print(f"Error: Directory '{PAGE_APP_DIR}' not found.")
        return

    hpp_files = sorted([f for f in os.listdir(page_app_path) if f.endswith(".hpp")])

    if not hpp_files:
        print(f"No .hpp files found in '{PAGE_APP_DIR}'.")
        return

    new_content = """/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

"""
    for hpp_file in hpp_files:
        new_content += f'#include "{PAGE_APP_DIR}/{hpp_file}"\n'

    if os.path.exists(output_path):
        with open(output_path, "r") as f:
            old_content = f.read()
        if old_content == new_content:
            print(f"{OUTPUT_FILE} is already up to date. No changes made.")
            return

    with open(output_path, "w") as f:
        f.write(new_content)

    print(f"Successfully updated {OUTPUT_FILE} with {len(hpp_files)} includes:")
    for hpp_file in hpp_files:
        print(f"  - {hpp_file}")

if __name__ == "__main__":
    generate_includes()
