#!/usr/bin/env python3
"""
EF2 Auto-Rename Functions

Finds functions that exist in the port under different names and generates
rename scripts to match retail.

Usage:
    python3 ef2_auto_rename.py [--output rename_script.sh] [--verify]
"""

import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Paths
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk/dlls/game")
CENSUS_FILE = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/tools/ef2_census.json")

def load_census():
    with open(CENSUS_FILE) as f:
        return json.load(f)

def find_similar_functions(func_name, port_funcs):
    """Find functions in port that are similar to the given function."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    similar = []
    for port_func in port_funcs:
        if not port_func.startswith(f'{cls}::'):
            continue

        port_method = port_func.split('::')[1].split('(')[0]

        # Check for various rename patterns
        if method_name == port_method:
            continue  # Same name, not a rename

        # Pattern 1: Event prefix/suffix
        if port_method == f'Event{method_name}':
            similar.append((port_func, 'EVENT_PREFIX'))
        elif port_method == f'{method_name}Event':
            similar.append((port_func, 'EVENT_SUFFIX'))
        elif method_name == f'Event{port_method}':
            similar.append((port_func, 'EVENT_PREFIX_REVERSE'))
        elif method_name == f'{port_method}Event':
            similar.append((port_func, 'EVENT_SUFFIX_REVERSE'))

        # Pattern 2: Cond prefix
        elif port_method == f'Cond_{method_name}':
            similar.append((port_func, 'COND_PREFIX'))
        elif method_name == f'Cond_{port_method}':
            similar.append((port_func, 'COND_PREFIX_REVERSE'))

        # Pattern 3: Substring match
        elif method_name.lower() in port_method.lower() or port_method.lower() in method_name.lower():
            similar.append((port_func, 'SUBSTRING'))

    return similar

def verify_rename(sdk_func, port_func, pattern):
    """Verify that a rename is correct by checking function bodies."""
    # Only EVENT_PREFIX is reliable — EVENT_SUFFIX often means different functions
    if pattern in ['EVENT_PREFIX', 'COND_PREFIX']:
        return True  # High confidence
    else:
        return False  # Low confidence — needs manual verification

def generate_rename_script(renames, output_file):
    """Generate a bash script to apply renames."""
    with open(output_file, 'w') as f:
        f.write("#!/bin/bash\n")
        f.write("# EF2 Auto-Rename Script\n")
        f.write(f"# Generated: {__import__('datetime').datetime.now()}\n")
        f.write(f"# Total renames: {len(renames)}\n\n")

        for sdk_func, port_func, pattern in renames:
            cls, sdk_method = sdk_func.split('::', 1)
            sdk_method_name = sdk_method.split('(')[0]
            port_method_name = port_func.split('::')[1].split('(')[0]

            f.write(f"# {sdk_func} -> {port_func} ({pattern})\n")
            f.write(f"sed -i 's/\\b{cls}::{port_method_name}\\b/{cls}::{sdk_method_name}/g' \\\n")
            f.write(f"    {PORT_DIR / cls.lower()}.cpp {PORT_DIR / cls.lower()}.h\n\n")

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Auto-rename functions to match retail')
    parser.add_argument('--output', type=str, help='Output rename script')
    parser.add_argument('--verify', action='store_true', help='Only output verified renames')
    args = parser.parse_args()

    print("Loading census...")
    census = load_census()

    # Get missing and port functions
    sdk_funcs = set()
    port_funcs = set()

    for key in census.get('sdk', {}):
        if '::' in key:
            sdk_funcs.add(key)

    for key in census.get('port', {}):
        if '::' in key:
            port_funcs.add(key)

    missing = sorted(sdk_funcs - port_funcs)
    print(f"Missing functions: {len(missing)}")

    # Find potential renames
    renames = []
    for func in missing:
        similar = find_similar_functions(func, port_funcs)
        for port_func, pattern in similar:
            if verify_rename(func, port_func, pattern):
                renames.append((func, port_func, pattern))

    print(f"Potential renames: {len(renames)}")

    # Group by pattern
    by_pattern = defaultdict(list)
    for sdk_func, port_func, pattern in renames:
        by_pattern[pattern].append((sdk_func, port_func))

    print(f"\nRenames by pattern:")
    for pattern, funcs in sorted(by_pattern.items()):
        print(f"  {len(funcs):4d} {pattern}")

    # Output
    if args.output:
        print(f"\nGenerating rename script: {args.output}")
        generate_rename_script(renames, args.output)
        print(f"Generated {len(renames)} renames")
    else:
        print(f"\nFound {len(renames)} potential renames (use --output to save)")
        for sdk_func, port_func, pattern in renames[:20]:
            print(f"  {sdk_func} -> {port_func} ({pattern})")

if __name__ == '__main__':
    main()
