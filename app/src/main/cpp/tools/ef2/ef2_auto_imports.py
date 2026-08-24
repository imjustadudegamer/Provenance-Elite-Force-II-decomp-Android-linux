#!/usr/bin/env python3
"""
EF2 Auto-Wire Imports

Maps SDK import names to port engine functions and generates wiring code.

Usage:
    python3 ef2_auto_imports.py [--output import_wiring.cpp]
"""

import json
import os
import re
import sys
from pathlib import Path

# Paths
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
ENGINE_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk/dlls/game")
IMPORT_FILE = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/mac_symbols/game_import_t_EF2.txt")

def load_retail_imports():
    """Load the retail import table."""
    imports = []
    with open(IMPORT_FILE) as f:
        for line in f:
            line = line.strip()
            if line.startswith('['):
                parts = line.split(']')
                idx = int(parts[0].strip('[] '))
                rest = parts[1].strip()
                parts2 = rest.split()
                offset = parts2[0]
                name = parts2[1] if len(parts2) > 1 else ''
                imports.append({'index': idx, 'offset': offset, 'name': name})
    return imports

def find_engine_function(name):
    """Find if the engine function exists in the port."""
    # Search in engine code
    for engine_file in ENGINE_DIR.rglob('*.c'):
        try:
            content = engine_file.read_text()
            if f'void {name}' in content or f'int {name}' in content or f'float {name}' in content:
                return engine_file.name
        except:
            pass

    return None

def find_game_import_field(name):
    """Find if the import field exists in game_import_t."""
    g_public = PORT_DIR / 'g_public.h'
    try:
        content = g_public.read_text()
        # Look for the function pointer
        pattern = rf'void\s+\(\*{name}\)'
        if re.search(pattern, content):
            return True
    except:
        pass

    return False

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Auto-wire missing imports')
    parser.add_argument('--output', type=str, help='Output wiring code')
    args = parser.parse_args()

    print("Loading retail imports...")
    imports = load_retail_imports()
    print(f"Retail imports: {len(imports)}")

    # Check which imports are missing
    missing = []
    for imp in imports:
        name = imp['name']
        if not name:
            continue

        # Check if it exists in game_import_t
        if find_game_import_field(name):
            continue

        # Check if it exists in engine
        engine_file = find_engine_function(name)
        if engine_file:
            missing.append({
                'name': name,
                'index': imp['index'],
                'offset': imp['offset'],
                'engine_file': engine_file
            })
        else:
            missing.append({
                'name': name,
                'index': imp['index'],
                'offset': imp['offset'],
                'engine_file': None
            })

    print(f"Missing imports: {len(missing)}")

    # Group by availability
    available = [m for m in missing if m['engine_file']]
    not_available = [m for m in missing if not m['engine_file']]

    print(f"Available in engine: {len(available)}")
    print(f"Not available: {len(not_available)}")

    # Output
    if args.output:
        print(f"\nGenerating wiring code: {args.output}")
        with open(args.output, 'w') as f:
            f.write("// EF2 Auto-Generated Import Wiring\n")
            f.write(f"// Generated: {__import__('datetime').datetime.now()}\n")
            f.write(f"// Available: {len(available)}, Not available: {len(not_available)}\n\n")

            f.write("// Available in engine — can be wired:\n")
            for imp in available:
                f.write(f"// import.{imp['name']} = {imp['name']}; // {imp['engine_file']}\n")

            f.write("\n// Not available in engine — need implementation:\n")
            for imp in not_available:
                f.write(f"// import.{imp['name']} = ???; // NOT FOUND\n")

        print(f"Generated wiring code")
    else:
        print(f"\nAvailable imports:")
        for imp in available[:20]:
            print(f"  {imp['name']} -> {imp['engine_file']}")
        if len(available) > 20:
            print(f"  ... and {len(available) - 20} more")

        print(f"\nNot available imports:")
        for imp in not_available[:20]:
            print(f"  {imp['name']}")
        if len(not_available) > 20:
            print(f"  ... and {len(not_available) - 20} more")

if __name__ == '__main__':
    main()
