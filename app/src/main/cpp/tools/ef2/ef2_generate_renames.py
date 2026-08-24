#!/usr/bin/env python3
"""
EF2 Generate Renames — generate sed commands to rename functions.

For high-confidence EVENT_PREFIX renames, generates:
1. sed commands to rename function definitions
2. sed commands to rename function declarations in headers
3. sed commands to update event registration tables

Usage:
  python3 ef2_generate_renames.py > renames.sh
  # Review renames.sh before running
  bash renames.sh
"""
import json, os, sys, re
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

def load_census():
    census_file = os.path.join(lib.WORK, 'tools', 'ef2_census.json')
    with open(census_file) as f:
        return json.load(f)

def generate_renames():
    """Generate rename commands for high-confidence renames."""
    census = load_census()

    # Load renames from the analysis report
    analysis_file = os.path.join(lib.WORK, 'EF2_RENAME_ANALYSIS.md')
    with open(analysis_file, 'r') as f:
        content = f.read()

    # Extract high confidence section
    high_section = re.search(r'### High Confidence.*?\n(.*?)(?=\n### |\Z)', content, re.S)
    if not high_section:
        print("Error: Could not find high confidence section.", file=sys.stderr)
        return

    # Parse renames
    rename_lines = high_section.group(1).split('\n')
    renames = []
    for line in rename_lines:
        if line.startswith('- ') and '→' in line:
            parts = line[2:].split('→')
            if len(parts) == 2:
                sdk_key = parts[0].strip().strip('`')
                port_key = parts[1].strip().strip('`')
                renames.append({
                    'sdk_key': sdk_key,
                    'port_key': port_key,
                    'sdk_class': sdk_key.split('::')[0] if '::' in sdk_key else '',
                    'sdk_method': sdk_key.split('::')[-1] if '::' in sdk_key else sdk_key,
                    'port_class': port_key.split('::')[0] if '::' in port_key else '',
                    'port_method': port_key.split('::')[-1] if '::' in port_key else port_key,
                })

    print(f"# EF2 Rename Commands — {len(renames)} high-confidence renames")
    print(f"# Generated: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M')}")
    print(f"#")
    print(f"# IMPORTANT: Review each rename before running!")
    print(f"# These are EVENT_PREFIX pattern renames — safe to apply.")
    print(f"#")
    print(f"# The event registration table maps script names to C++ functions.")
    print(f"# Renaming the C++ function doesn't affect script compatibility.")
    print(f"#")
    print(f"# Example: Actor::EventLookAt → Actor::LookAt")
    print(f"#   Event: EV_Actor_LookAt (script name: 'lookat')")
    print(f"#   Handler: Actor::LookAt(Event *ev)")
    print(f"#   Registration: {{&EV_Actor_LookAt, &Actor::LookAt}}")
    print(f"#")
    print(f"# After renaming, update registration table to use new name.")
    print()

    # Group by file
    by_file = {}
    for r in renames:
        # Find which file the port function is in
        if r['port_key'] in census['port']:
            for entry in census['port'][r['port_key']]:
                filename = entry['filename']
                by_file.setdefault(filename, []).append(r)

    print("# Step 1: Rename function definitions in .cpp files")
    print("# Step 2: Rename function declarations in .h files")
    print("# Step 3: Update event registration tables")
    print()

    # Generate sed commands for each file
    for filename in sorted(by_file.keys()):
        file_renames = by_file[filename]
        print(f"# === {filename} ({len(file_renames)} renames) ===")

        for r in file_renames:
            old_name = r['port_method']
            new_name = r['sdk_method']
            class_name = r['port_class']

            # Function definition: void Class::OldName(Event *ev)
            print(f"sed -i 's/\\b{class_name}::{old_name}\\b/{class_name}::{new_name}/g' \\")
            print(f"    {lib.PORT}/{filename}")

        print()

    # Generate header file renames
    print("# === Header file renames ===")
    print("# Need to find and update declarations in .h files")
    print()

    for r in renames:
        old_name = r['port_method']
        new_name = r['sdk_method']
        class_name = r['port_class']

        # Search for declaration in header files
        print(f"# {class_name}::{old_name} → {class_name}::{new_name}")
        print(f"grep -rn '{class_name}::{old_name}' {lib.PORT}/*.h | grep -v '.cpp:' | \\")
        print(f"    while read line; do")
        print(f"        file=$(echo \"$line\" | cut -d: -f1)")
        print(f"        sed -i 's/\\b{class_name}::{old_name}\\b/{class_name}::{new_name}/g' \"$file\"")
        print(f"    done")
        print()

    # Generate registration table updates
    print("# === Event registration table updates ===")
    print("# Update {&EV_Event, &Class::Handler} to use new name")
    print()

    for r in renames:
        old_name = r['port_method']
        new_name = r['sdk_method']
        class_name = r['port_class']

        print(f"# {class_name}::{old_name} → {class_name}::{new_name}")
        print(f"grep -rn '&{class_name}::{old_name}' {lib.PORT}/*.cpp | \\")
        print(f"    while read line; do")
        print(f"        file=$(echo \"$line\" | cut -d: -f1)")
        print(f"        sed -i 's/&{class_name}::{old_name}/\\&{class_name}::{new_name}/g' \"$file\"")
        print(f"    done")
        print()

    # Summary
    print("# === Summary ===")
    print(f"# Total renames: {len(renames)}")
    print(f"# Files affected: {len(by_file)}")
    print(f"#")
    print(f"# After running these commands:")
    print(f"# 1. Rebuild: ninja -C build")
    print(f"# 2. Verify: grep -rn 'Event' {lib.PORT}/*.cpp | grep -v '//' | head -20")
    print(f"# 3. Test: Run the game and verify events still work")

def main():
    generate_renames()

if __name__ == '__main__':
    main()
