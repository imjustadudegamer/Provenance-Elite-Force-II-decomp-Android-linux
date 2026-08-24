#!/usr/bin/env python3
"""
EF2 Smart Function Generator

Reads SDK function bodies and generates port implementations.
Adapts to port's class structure using known mappings.

Usage:
    python3 ef2_smart_generate.py [--class Actor] [--output generated.cpp] [--limit 10]
"""

import json
import os
import re
import sys
from pathlib import Path

# Paths
PORT_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame")
SDK_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk/dlls/game")
IDA_DIR = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/mac_symbols/game_decomp")
CENSUS_FILE = Path(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/tools/ef2_census.json")

# Known member variable mappings (SDK -> Port)
MEMBER_MAPPINGS = {
    'entityVars': 'Vars()',
    'm_Enemy': 'enemyManager->GetCurrentEnemy()',
    'm_bDoAI': 'ai_on',
    'm_bForceAttackPlayer': '_forceAttackPlayer',
    'm_fuzzyEngineActive': '_fuzzyEngineActive',
    'm_archetype': '_archetype',
    '_missionObjective': '_missionObjective',
    '_targetPos': '_targetPos',
    'useData': 'useData',
    'm_pTurnEntity': '_turnEntity',
    'm_scriptBehavior': '_scriptBehavior',
    'm_masterstatemap': '_masterstatemap',
    'm_currentMasterState': '_currentMasterState',
    'm_lastMasterState': '_lastMasterState',
}

# Known function mappings (SDK -> Port)
FUNCTION_MAPPINGS = {
    'entityVars.SetVariable': 'Vars()->SetVariable',
    'entityVars.GetVariable': 'Vars()->GetVariable',
    'entityVars.RemoveVariable': 'Vars()->RemoveVariable',
}

def load_census():
    with open(CENSUS_FILE) as f:
        return json.load(f)

def get_sdk_function_body(func_name):
    """Read the SDK function body."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    # Find the SDK file
    for sdk_file in SDK_DIR.glob('*.cpp'):
        try:
            content = sdk_file.read_text()
            # Look for function definition
            pattern = rf'void\s+{cls}::{method_name}\s*\([^)]*\)\s*\{{'
            match = re.search(pattern, content)
            if match:
                # Extract the function body
                start = match.start()
                brace_count = 0
                in_function = False
                end = start

                for i, char in enumerate(content[start:], start):
                    if char == '{':
                        brace_count += 1
                        in_function = True
                    elif char == '}':
                        brace_count -= 1
                        if in_function and brace_count == 0:
                            end = i + 1
                            break

                return content[start:end]
        except:
            pass

    return None

def check_ida_exists(func_name):
    """Check if function exists in IDA decompilations."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    # Search for IDA file
    for ida_file in IDA_DIR.glob(f'*{method_name}*'):
        if cls.lower() in ida_file.name.lower():
            return ida_file.name

    return None

def adapt_to_port(sdk_body, func_name):
    """Adapt SDK function body to port's class structure."""
    adapted = sdk_body

    # Apply member variable mappings
    for sdk_name, port_name in MEMBER_MAPPINGS.items():
        adapted = adapted.replace(sdk_name, port_name)

    # Apply function mappings
    for sdk_name, port_name in FUNCTION_MAPPINGS.items():
        adapted = adapted.replace(sdk_name, port_name)

    # Fix common patterns
    adapted = adapted.replace('entityVars.', 'Vars()->')
    adapted = adapted.replace('entityVars->', 'Vars()->')

    return adapted

def generate_function(func_name, sdk_body, ida_file=None):
    """Generate a port function implementation."""
    cls, method = func_name.split('::', 1)
    method_name = method.split('(')[0]

    # Adapt the SDK body
    adapted = adapt_to_port(sdk_body, func_name)

    # Generate the implementation
    impl = f"""
// SDK {cls.lower()}.cpp — {func_name}
// IDA: {ida_file or 'NOT FOUND'}
{adapted}
"""

    return impl

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Smart function generator')
    parser.add_argument('--cls', type=str, help='Only process this class')
    parser.add_argument('--output', type=str, help='Output file')
    parser.add_argument('--limit', type=int, default=10, help='Limit number of functions')
    args = parser.parse_args()

    print("Loading census...")
    census = load_census()

    # Get missing functions
    sdk_funcs = set()
    port_funcs = set()

    for key in census.get('sdk', {}):
        if '::' in key:
            sdk_funcs.add(key)

    for key in census.get('port', {}):
        if '::' in key:
            port_funcs.add(key)

    missing = sorted(sdk_funcs - port_funcs)

    if args.cls:
        missing = [f for f in missing if f.startswith(f'{args.cls}::')]

    print(f"Missing functions: {len(missing)}")

    # Process each missing function
    generated = []
    for func in missing[:args.limit]:
        print(f"\nProcessing: {func}")

        # Check IDA
        ida_file = check_ida_exists(func)
        if ida_file:
            print(f"  IDA: {ida_file}")
        else:
            print(f"  IDA: NOT FOUND")

        # Read SDK body
        sdk_body = get_sdk_function_body(func)
        if sdk_body:
            print(f"  SDK: Found ({len(sdk_body)} chars)")

            # Generate implementation
            impl = generate_function(func, sdk_body, ida_file)
            generated.append((func, impl))
        else:
            print(f"  SDK: NOT FOUND")

    # Output
    if args.output:
        print(f"\nWriting {len(generated)} functions to {args.output}")
        with open(args.output, 'w') as f:
            f.write("// EF2 Smart-Generated Missing Functions\n")
            f.write(f"// Generated: {__import__('datetime').datetime.now()}\n")
            f.write(f"// Total: {len(generated)} functions\n\n")

            for func, impl in generated:
                f.write(impl)
                f.write("\n")

        print(f"Generated {len(generated)} functions")
    else:
        print(f"\nGenerated {len(generated)} functions (use --output to save)")

if __name__ == '__main__':
    main()
