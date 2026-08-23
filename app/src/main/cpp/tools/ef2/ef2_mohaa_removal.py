#!/usr/bin/env python3
"""
EF2 MOHAA Garbage Removal Script

Identifies functions in the port that are NOT in the retail SDK and have no
retail dependencies. These are safe to remove.

Usage:
    python3 ef2_mohaa_removal.py [--dry-run] [--output removal_script.sh]
"""

import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

# Paths
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game")
CENSUS_FILE = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/tools/ef2_census.json")

def load_census():
    """Load the function census."""
    with open(CENSUS_FILE) as f:
        return json.load(f)

def get_sdk_functions(census):
    """Get all SDK function names."""
    funcs = set()
    for key in census.get('sdk', {}):
        if '::' in key:
            funcs.add(key)
    return funcs

def get_port_functions(census):
    """Get all port function names."""
    funcs = set()
    for key in census.get('port', {}):
        if '::' in key:
            funcs.add(key)
    return funcs

def get_extra_functions(census):
    """Get functions in port but not in SDK (MOHAA candidates)."""
    sdk = get_sdk_functions(census)
    port = get_port_functions(census)
    return sorted(port - sdk)

def check_retail_dependencies(func_name, port_files):
    """Check if any retail function calls this function."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    # Search for references in all port files
    for f in port_files:
        if not str(f).endswith('.cpp') and not str(f).endswith('.h'):
            continue
        try:
            content = f.read_text()
            # Check for direct references (not just comments)
            if method_name in content:
                # Filter out comments and the function definition itself
                lines = content.split('\n')
                for line in lines:
                    if method_name in line and not line.strip().startswith('//'):
                        # Skip the function definition itself
                        if f'{cls}::{method_name}' in line:
                            continue
                        # Skip CLASS_DECLARATION entries
                        if '&Entity::' in line or '&Actor::' in line or '&Player::' in line:
                            continue
                        # Skip event registration entries
                        if '&EV_' in line:
                            continue
                        return True
        except:
            pass

    return False

def find_function_definition(func_name, port_files):
    """Find where a function is defined in the port."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    for f in port_files:
        if not str(f).endswith('.cpp') and not str(f).endswith('.h'):
            continue
        try:
            content = f.read_text()
            # Look for function definition (any return type)
            pattern = rf'\w+\s+{cls}::{method_name}\s*\('
            if re.search(pattern, content):
                return f
        except:
            pass

    return None

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Identify MOHAA garbage for removal')
    parser.add_argument('--dry-run', action='store_true', help='Only show what would be removed')
    parser.add_argument('--output', type=str, help='Output removal script')
    args = parser.parse_args()

    print("Loading census...")
    census = load_census()

    print("Identifying extra functions...")
    extra = get_extra_functions(census)
    print(f"Found {len(extra)} extra functions in port")

    # Group by class
    by_class = defaultdict(list)
    for func in extra:
        cls = func.split('::')[0]
        by_class[cls].append(func)

    print(f"\nExtra functions by class:")
    for cls in sorted(by_class.keys(), key=lambda c: -len(by_class[c])):
        print(f"  {len(by_class[cls]):4d} {cls}")

    # Check dependencies
    print("\nChecking retail dependencies...")
    port_files = list(PORT_DIR.glob('*.cpp')) + list(PORT_DIR.glob('*.h'))

    safe_to_remove = []
    has_dependencies = []

    for func in extra:
        if check_retail_dependencies(func, port_files):
            has_dependencies.append(func)
        else:
            safe_to_remove.append(func)

    print(f"\nSafe to remove: {len(safe_to_remove)}")
    print(f"Has dependencies: {len(has_dependencies)}")

    # Group safe to remove by class
    safe_by_class = defaultdict(list)
    for func in safe_to_remove:
        cls = func.split('::')[0]
        safe_by_class[cls].append(func)

    print(f"\nSafe to remove by class:")
    for cls in sorted(safe_by_class.keys(), key=lambda c: -len(safe_by_class[c])):
        print(f"  {len(safe_by_class[cls]):4d} {cls}")

    if args.dry_run:
        print("\n=== DRY RUN — would remove:")
        for func in safe_to_remove[:50]:
            print(f"  {func}")
        if len(safe_to_remove) > 50:
            print(f"  ... and {len(safe_to_remove) - 50} more")
        return

    # Generate removal script
    if args.output:
        print(f"\nGenerating removal script: {args.output}")
        with open(args.output, 'w') as f:
            f.write("#!/bin/bash\n")
            f.write("# EF2 MOHAA Garbage Removal Script\n")
            f.write(f"# Generated: {__import__('datetime').datetime.now()}\n")
            f.write(f"# Safe to remove: {len(safe_to_remove)} functions\n\n")

            for func in safe_to_remove:
                cls, method = func.split('::', 1)
                method_name = method.split('(')[0]

                # Find the file where this function is defined
                def_file = find_function_definition(func, port_files)
                if def_file:
                    f.write(f"# Remove {func}\n")
                    f.write(f"# Found in: {def_file}\n")
                    f.write(f"sed -i '/{cls}::{method_name}/,/^}}/d' {def_file}\n")
                    f.write(f"sed -i '/{method_name}/d' {def_file.with_suffix('.h')}\n")
                    # Also remove from registration tables
                    glob_pattern = str(PORT_DIR / "*.cpp")
                    f.write(f"sed -i '/{method_name}/d' {glob_pattern}\n\n")

        print(f"Generated removal script with {len(safe_to_remove)} entries")

if __name__ == '__main__':
    main()
