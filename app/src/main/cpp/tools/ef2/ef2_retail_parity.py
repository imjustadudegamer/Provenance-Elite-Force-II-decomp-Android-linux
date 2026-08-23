#!/usr/bin/env python3
"""
EF2 Retail Parity Master Script

Orchestrates all mechanical scripts to achieve 100% retail parity.

Usage:
    python3 ef2_retail_parity.py [--phase all|analyze|rename|generate|remove|wire]
"""

import json
import os
import sys
import subprocess
from pathlib import Path
from collections import defaultdict

# Paths
TOOLS_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/tools")
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game")
CENSUS_FILE = TOOLS_DIR / "ef2_census.json"

def load_census():
    with open(CENSUS_FILE) as f:
        return json.load(f)

def analyze():
    """Analyze current state and generate reports."""
    print("=" * 60)
    print("EF2 RETAIL PARITY ANALYSIS")
    print("=" * 60)

    census = load_census()

    sdk_funcs = set()
    port_funcs = set()

    for key in census.get('sdk', {}):
        if '::' in key:
            sdk_funcs.add(key)

    for key in census.get('port', {}):
        if '::' in key:
            port_funcs.add(key)

    missing = sorted(sdk_funcs - port_funcs)
    extra = sorted(port_funcs - sdk_funcs)

    print(f"\nSDK functions: {len(sdk_funcs)}")
    print(f"Port functions: {len(port_funcs)}")
    print(f"Missing from port: {len(missing)}")
    print(f"Extra in port (MOHAA): {len(extra)}")

    # Group missing by class
    missing_by_class = defaultdict(list)
    for func in missing:
        cls = func.split('::')[0]
        missing_by_class[cls].append(func)

    print(f"\nMissing by class (top 20):")
    for cls in sorted(missing_by_class.keys(), key=lambda c: -len(missing_by_class[c]))[:20]:
        print(f"  {len(missing_by_class[cls]):4d} {cls}")

    # Group extra by class
    extra_by_class = defaultdict(list)
    for func in extra:
        cls = func.split('::')[0]
        extra_by_class[cls].append(func)

    print(f"\nExtra by class (top 20):")
    for cls in sorted(extra_by_class.keys(), key=lambda c: -len(extra_by_class[c]))[:20]:
        print(f"  {len(extra_by_class[cls]):4d} {cls}")

    return missing, extra

def run_script(script_name, args=None):
    """Run a tool script."""
    script_path = TOOLS_DIR / script_name
    if not script_path.exists():
        print(f"Script not found: {script_path}")
        return False

    cmd = [sys.executable, str(script_path)]
    if args:
        cmd.extend(args)

    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    return result.returncode == 0

def main():
    import argparse
    parser = argparse.ArgumentParser(description='EF2 Retail Parity Master Script')
    parser.add_argument('--phase', type=str, default='all',
                       choices=['all', 'analyze', 'rename', 'generate', 'remove', 'wire'],
                       help='Phase to run')
    args = parser.parse_args()

    if args.phase in ['all', 'analyze']:
        missing, extra = analyze()

    if args.phase in ['all', 'rename']:
        print("\n" + "=" * 60)
        print("PHASE: AUTO-RENAME")
        print("=" * 60)
        run_script('ef2_auto_rename.py', ['--output', str(TOOLS_DIR / 'rename_script.sh')])

    if args.phase in ['all', 'generate']:
        print("\n" + "=" * 60)
        print("PHASE: AUTO-GENERATE")
        print("=" * 60)
        run_script('ef2_generate_functions.py', ['--output', str(TOOLS_DIR / 'generated_functions.cpp'), '--limit', '50'])

    if args.phase in ['all', 'remove']:
        print("\n" + "=" * 60)
        print("PHASE: MOHAA REMOVAL")
        print("=" * 60)
        run_script('ef2_mohaa_removal.py', ['--output', str(TOOLS_DIR / 'removal_script.sh')])

    if args.phase in ['all', 'wire']:
        print("\n" + "=" * 60)
        print("PHASE: IMPORT WIRING")
        print("=" * 60)
        run_script('ef2_auto_imports.py', ['--output', str(TOOLS_DIR / 'import_wiring.cpp')])

    print("\n" + "=" * 60)
    print("COMPLETE")
    print("=" * 60)

if __name__ == '__main__':
    main()
