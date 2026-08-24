#!/usr/bin/env python3
"""
EF2 Function Census — build a complete inventory of every function in:
  1. Retail SDK (source)
  2. Port code (source)
  3. IDA decompilations (binary)

Outputs a JSON census file for use by ef2_function_diff.py.
"""
import json, os, sys, collections
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

def census_sdk():
    """Build census of retail SDK functions."""
    print("Scanning retail SDK...", file=sys.stderr)
    funcs_by_file = lib.extract_functions_from_dir(lib.SDK)
    census = {}
    for filename, funcs in funcs_by_file.items():
        for f in funcs:
            key = lib.func_key(f['class_name'], f['method_name'])
            entry = {
                'key': key,
                'class_name': f['class_name'],
                'method_name': f['method_name'],
                'return_type': f['return_type'],
                'params': f['params'],
                'filename': filename,
                'line': f['line'],
                'body_lines': lib.count_lines(f['body']),
                'source': 'sdk',
            }
            census.setdefault(key, []).append(entry)
    return census

def census_port():
    """Build census of port functions."""
    print("Scanning port code...", file=sys.stderr)
    funcs_by_file = lib.extract_functions_from_dir(lib.PORT)
    census = {}
    for filename, funcs in funcs_by_file.items():
        for f in funcs:
            key = lib.func_key(f['class_name'], f['method_name'])
            entry = {
                'key': key,
                'class_name': f['class_name'],
                'method_name': f['method_name'],
                'return_type': f['return_type'],
                'params': f['params'],
                'filename': filename,
                'line': f['line'],
                'body_lines': lib.count_lines(f['body']),
                'source': 'port',
            }
            census.setdefault(key, []).append(entry)
    return census

def census_ida():
    """Build census of IDA decompilation functions."""
    print("Scanning IDA decompilations...", file=sys.stderr)
    census = {}
    for filename in os.listdir(lib.DECOMP):
        if not filename.endswith('.c'):
            continue
        parsed = lib.parse_ida_filename(filename)
        if not parsed:
            continue
        key = lib.func_key(parsed['class_name'], parsed['method_name'])
        entry = {
            'key': key,
            'class_name': parsed['class_name'],
            'method_name': parsed['method_name'],
            'is_const': parsed['is_const'],
            'params_mangled': parsed['params_mangled'],
            'filename': filename,
            'source': 'ida',
        }
        census.setdefault(key, []).append(entry)
    return census

def census_classes():
    """Build census of class declarations."""
    print("Scanning class declarations...", file=sys.stderr)
    sdk_classes = lib.extract_classes_from_dir(lib.SDK)
    port_classes = lib.extract_classes_from_dir(lib.PORT)
    return {
        'sdk': sdk_classes,
        'port': port_classes,
    }

def main():
    output_file = os.path.join(lib.WORK, 'tools', 'ef2_census.json')

    sdk = census_sdk()
    port = census_port()
    ida = census_ida()
    classes = census_classes()

    # Summary
    sdk_keys = set(sdk.keys())
    port_keys = set(port.keys())
    ida_keys = set(ida.keys())

    print(f"\n=== Function Census ===", file=sys.stderr)
    print(f"Retail SDK:  {len(sdk_keys)} unique functions, {sum(len(v) for v in sdk.values())} total", file=sys.stderr)
    print(f"Port code:   {len(port_keys)} unique functions, {sum(len(v) for v in port.values())} total", file=sys.stderr)
    print(f"IDA decomp:  {len(ida_keys)} unique functions, {sum(len(v) for v in ida.values())} total", file=sys.stderr)

    # Overlap
    all_keys = sdk_keys | port_keys | ida_keys
    in_all = sdk_keys & port_keys & ida_keys
    in_sdk_port = sdk_keys & port_keys
    in_sdk_ida = sdk_keys & ida_keys
    in_port_only = port_keys - sdk_keys - ida_keys
    in_sdk_only = sdk_keys - port_keys - ida_keys

    print(f"\nIn all three:     {len(in_all)}", file=sys.stderr)
    print(f"In SDK+Port:      {len(in_sdk_port)}", file=sys.stderr)
    print(f"In SDK+IDA:       {len(in_sdk_ida)}", file=sys.stderr)
    print(f"Port-only (MOHAA?): {len(in_port_only)}", file=sys.stderr)
    print(f"SDK-only (missing): {len(in_sdk_only)}", file=sys.stderr)

    # Class stats
    print(f"\nSDK classes:  {len(classes['sdk'])}", file=sys.stderr)
    print(f"Port classes: {len(classes['port'])}", file=sys.stderr)

    # Save census
    census = {
        'sdk': {k: v for k, v in sdk.items()},
        'port': {k: v for k, v in port.items()},
        'ida': {k: v for k, v in ida.items()},
        'classes': classes,
        'stats': {
            'sdk_count': len(sdk_keys),
            'port_count': len(port_keys),
            'ida_count': len(ida_keys),
            'in_all': len(in_all),
            'in_sdk_port': len(in_sdk_port),
            'in_sdk_ida': len(in_sdk_ida),
            'port_only': len(in_port_only),
            'sdk_only': len(in_sdk_only),
        }
    }

    with open(output_file, 'w') as f:
        json.dump(census, f, indent=2)
    print(f"\nCensus saved to {output_file}", file=sys.stderr)

if __name__ == '__main__':
    main()
