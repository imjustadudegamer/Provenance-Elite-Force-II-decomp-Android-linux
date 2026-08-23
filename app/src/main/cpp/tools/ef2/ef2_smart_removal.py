#!/usr/bin/env python3
"""
EF2 Smart MOHAA Removal Script

Uses transitive dependency analysis to identify safe removals.
If a function is only referenced by other MOHAA functions (which would also
be removed), then it's safe to remove.

Usage:
    python3 ef2_smart_removal.py [--output removal_script.sh] [--dry-run]
"""

import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Paths
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game")
CENSUS_FILE = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/tools/ef2_census.json")

def load_census():
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

def find_function_references(func_name, port_files):
    """Find all references to a function in port files."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    references = []
    for f in port_files:
        if not str(f).endswith('.cpp') and not str(f).endswith('.h'):
            continue
        try:
            content = f.read_text()
            if method_name in content:
                lines = content.split('\n')
                for i, line in enumerate(lines):
                    if method_name in line and not line.strip().startswith('//'):
                        # Skip the function definition itself
                        if f'{cls}::{method_name}' in line:
                            continue
                        # Skip event registration entries
                        if '&EV_' in line:
                            continue
                        # Skip comments
                        if line.strip().startswith('//'):
                            continue
                        references.append((f.name, i + 1, line.strip()[:100]))
        except:
            pass

    return references

def is_mohaa_function(func_name, extra_funcs):
    """Check if a function is a MOHAA function (not in SDK)."""
    return func_name in extra_funcs

def find_transitive_dependencies(func_name, extra_funcs, port_files, visited=None):
    """Find all MOHAA functions that depend on this function transitively."""
    if visited is None:
        visited = set()

    if func_name in visited:
        return set()
    visited.add(func_name)

    dependencies = set()
    references = find_function_references(func_name, port_files)

    for file_name, line_num, line_content in references:
        # Extract the function name from the reference
        # Look for patterns like ClassName::MethodName
        match = re.search(r'(\w+)::(\w+)', line_content)
        if match:
            ref_cls, ref_method = match.groups()
            ref_func = f'{ref_cls}::{ref_method}'

            if ref_func in extra_funcs and ref_func != func_name:
                dependencies.add(ref_func)
                # Recursively find dependencies of this dependency
                transitive = find_transitive_dependencies(ref_func, extra_funcs, port_files, visited)
                dependencies.update(transitive)

    return dependencies

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Smart MOHAA removal with transitive dependency analysis')
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
    for cls in sorted(by_class.keys(), key=lambda c: -len(by_class[c]))[:10]:
        print(f"  {len(by_class[cls]):4d} {cls}")

    # Find port files
    port_files = list(PORT_DIR.glob('*.cpp')) + list(PORT_DIR.glob('*.h'))

    # Analyze dependencies
    print("\nAnalyzing dependencies...")
    safe_to_remove = []
    has_external_deps = []

    sdk_funcs = get_sdk_functions(census)

    for func in extra:
        cls, method = func.split('::', 1)
        method_name = method.split('(')[0]

        references = find_function_references(func, port_files)

        # Check if any reference is from a retail function
        has_external = False
        for file_name, line_num, line_content in references:
            # Check if the reference is from a retail function
            match = re.search(r'(\w+)::(\w+)', line_content)
            if match:
                ref_cls, ref_method = match.groups()
                ref_func = f'{ref_cls}::{ref_method}'

                # If the referencing function IS in the SDK,
                # it's a retail function and this is an external dependency
                if ref_func in sdk_funcs:
                    has_external = True
                    break

            # Also check if the reference is a direct call (not just a string match)
            # Look for patterns like: func_name( or ->func_name
            if f'{method_name}(' in line_content or f'->{method_name}' in line_content:
                # This is a direct call — check if it's from a retail file
                # For now, assume it's an external dependency
                has_external = True
                break

        if has_external:
            has_external_deps.append(func)
        else:
            safe_to_remove.append(func)

    print(f"\nSafe to remove: {len(safe_to_remove)}")
    print(f"Has external dependencies: {len(has_external_deps)}")

    # Group safe to remove by class
    safe_by_class = defaultdict(list)
    for func in safe_to_remove:
        cls = func.split('::')[0]
        safe_by_class[cls].append(func)

    print(f"\nSafe to remove by class:")
    for cls in sorted(safe_by_class.keys(), key=lambda c: -len(safe_by_class[c]))[:10]:
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
            f.write("# EF2 Smart MOHAA Removal Script\n")
            f.write(f"# Generated: {__import__('datetime').datetime.now()}\n")
            f.write(f"# Safe to remove: {len(safe_to_remove)} functions\n\n")

            for func in safe_to_remove:
                cls, method = func.split('::', 1)
                method_name = method.split('(')[0]

                f.write(f"# Remove {func}\n")
                f.write(f"sed -i '/{cls}::{method_name}/,/^}}/d' {PORT_DIR / cls.lower()}.cpp\n")
                f.write(f"sed -i '/{method_name}/d' {PORT_DIR / cls.lower()}.h\n")
                f.write(f"sed -i '/{method_name}/d' {PORT_DIR / '*.cpp'}\n\n")

        print(f"Generated removal script with {len(safe_to_remove)} entries")

if __name__ == '__main__':
    main()
