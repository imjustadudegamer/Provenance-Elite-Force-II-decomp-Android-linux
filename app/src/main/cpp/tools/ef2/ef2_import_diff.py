#!/usr/bin/env python3
"""
EF2 Import Table Diff — compare game_import_t between retail binary and port.

The game_import_t is the function table the engine provides to the game DLL.
Retail has 322 entries. The port may have missing or different entries.

Data sources:
  - personal/evidence/mac_symbols/game_import_t_EF2.txt (retail binary, 322 entries)
  - personal/evidence/mac_symbols/game_import_t_MISSING_from_port.txt (101 missing)
  - Port's game_import_t definition (grep for it in engine code)
"""
import re, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

def find_port_import_table():
    """Find and parse the port's game_import_t definition."""
    # Search for game_import_t struct definition in engine code
    search_paths = [
        os.path.join(lib.WORK, 'openmohaa', 'code'),
        os.path.join(lib.WORK, 'openmohaa', 'code', 'qcommon'),
        os.path.join(lib.WORK, 'openmohaa', 'code', 'server'),
    ]

    import_struct = None
    for search_path in search_paths:
        for root, dirs, files in os.walk(search_path):
            for f in files:
                if not f.endswith(('.h', '.cpp')):
                    continue
                filepath = os.path.join(root, f)
                try:
                    with open(filepath, 'r', errors='replace') as fh:
                        content = fh.read()
                except Exception:
                    continue

                # Look for game_import_t struct
                if 'game_import_t' in content and 'struct' in content:
                    # Find the struct definition
                    m = re.search(r'struct\s+game_import_t\s*\{([^}]+)\}', content, re.S)
                    if m:
                        import_struct = m.group(1)
                        return import_struct, filepath
    return None, None

def parse_import_functions(struct_body):
    """Parse function pointers from game_import_t struct."""
    funcs = []
    # Match function pointer declarations: return_type (*name)(params)
    pattern = re.compile(r'(\w[\w\s*]*?)\s*\(\s*\*\s*(\w+)\s*\)\s*\([^)]*\)\s*;')
    for m in pattern.finditer(struct_body):
        return_type = m.group(1).strip()
        name = m.group(2)
        funcs.append({
            'name': name,
            'return_type': return_type,
        })
    return funcs

def main():
    print("=== EF2 Import Table Diff ===\n")

    # Load retail import table
    retail_imports = lib.load_import_table()
    print(f"Retail import table: {len(retail_imports)} entries")

    # Load missing imports
    missing_imports = lib.load_missing_imports()
    print(f"Missing from port: {len(missing_imports)} entries")

    # Find port's import table
    port_struct, port_file = find_port_import_table()
    if port_struct:
        port_funcs = parse_import_functions(port_struct)
        print(f"Port import table: {len(port_funcs)} function pointers in {port_file}")
    else:
        print("Warning: Could not find port's game_import_t definition")
        port_funcs = []

    # Build lookup sets
    retail_names = {imp['name'] for imp in retail_imports}
    missing_names = {imp['name'] for imp in missing_imports}
    port_names = {f['name'] for f in port_funcs}

    # Analysis
    print(f"\n--- Analysis ---")
    print(f"Retail functions: {len(retail_names)}")
    print(f"Port functions:   {len(port_names)}")
    print(f"Missing from port: {len(missing_names)}")

    # Check which missing are actually present in port
    actually_present = missing_names & port_names
    still_missing = missing_names - port_names
    print(f"\nActually present in port (marked missing but found): {len(actually_present)}")
    print(f"Still missing: {len(still_missing)}")

    # Check for port functions not in retail
    port_extra = port_names - retail_names
    print(f"Port-only (not in retail): {len(port_extra)}")

    # Generate report
    report = []
    report.append("# EF2 Import Table Diff Report\n")
    report.append(f"Retail import table: {len(retail_imports)} entries")
    report.append(f"Port import table: {len(port_funcs)} entries")
    report.append(f"Missing from port: {len(still_missing)} entries")
    report.append(f"Port-only: {len(port_extra)} entries\n")

    # Missing imports
    report.append("## Missing Imports (in retail but not in port)\n")
    report.append("These are engine functions the retail game expects but the port doesn't provide.\n")
    report.append("| Index | Offset | Name | Category |")
    report.append("|-------|--------|------|----------|")

    # Categorize missing imports
    categories = {
        'Cvar': ['Cvar_'],
        'Filesystem': ['FS_'],
        'Server': ['SV_'],
        'Client': ['CL_', 'CG_'],
        'Command': ['Cmd_', 'Cbuf_'],
        'Network': ['NET_', 'MSG_'],
        'Renderer': ['RE_', 'R_'],
        'Sound': ['S_'],
        'Physics': ['PF_', 'CM_'],
        'System': ['Sys_', 'Com_'],
        'Other': [],
    }

    def categorize(name):
        for cat, prefixes in categories.items():
            if cat == 'Other':
                continue
            for prefix in prefixes:
                if name.startswith(prefix):
                    return cat
        return 'Other'

    for imp in sorted(still_missing):
        cat = categorize(imp)
        # Find index and offset from retail table
        entry = next((e for e in retail_imports if e['name'] == imp), None)
        if entry:
            report.append(f"| {entry['index']} | +0x{entry['offset']:03x} | `{imp}` | {cat} |")
        else:
            report.append(f"| ? | ? | `{imp}` | {cat} |")

    report.append("")

    # Port-only imports
    if port_extra:
        report.append("## Port-Only Imports (in port but not in retail)\n")
        report.append("These are engine functions the port provides but retail doesn't use.\n")
        report.append("| Name | Category |")
        report.append("|------|----------|")
        for name in sorted(port_extra):
            cat = categorize(name)
            report.append(f"| `{name}` | {cat} |")
        report.append("")

    # Category summary
    report.append("## Missing by Category\n")
    report.append("| Category | Missing |")
    report.append("|----------|---------|")
    cat_counts = collections.Counter()
    for imp in still_missing:
        cat_counts[categorize(imp)] += 1
    for cat, count in cat_counts.most_common():
        report.append(f"| {cat} | {count} |")
    report.append("")

    # Write report
    output_file = os.path.join(lib.WORK, 'EF2_IMPORT_DIFF.md')
    with open(output_file, 'w') as f:
        f.write('\n'.join(report))
    print(f"\nReport saved to {output_file}")

    # Also print summary
    print('\n'.join(report[:60]))

if __name__ == '__main__':
    import collections
    main()
