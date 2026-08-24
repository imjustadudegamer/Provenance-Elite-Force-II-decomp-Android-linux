#!/usr/bin/env python3
"""
EF2 Function Diff — comprehensive comparison of retail SDK vs port vs IDA decompilations.

Generates a detailed report showing:
  1. Missing functions (in SDK but not in port)
  2. Extra functions (in port but not in SDK — MOHAA leftovers)
  3. Divergent functions (in both but different implementations)
  4. IDA cross-reference (what the retail binary actually contains)
  5. Class hierarchy differences
  6. Per-subsystem breakdown

Requires: ef2_census.json (run ef2_function_census.py first)
"""
import json, os, sys, collections, difflib
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

def load_census():
    """Load the census file."""
    census_file = os.path.join(lib.WORK, 'tools', 'ef2_census.json')
    if not os.path.exists(census_file):
        print("Error: ef2_census.json not found. Run ef2_function_census.py first.", file=sys.stderr)
        sys.exit(1)
    with open(census_file) as f:
        return json.load(f)

def load_function_bodies(dirpath, extensions=('.cpp', '.h')):
    """Load function bodies from a directory."""
    bodies = {}
    for ext in extensions:
        for filepath in __import__('glob').glob(os.path.join(dirpath, '*' + ext)):
            filename = os.path.basename(filepath)
            try:
                with open(filepath, 'r', errors='replace') as f:
                    src = f.read()
            except Exception:
                continue
            funcs = lib.extract_functions(src, filename)
            for func in funcs:
                key = lib.func_key(func['class_name'], func['method_name'])
                bodies.setdefault(key, []).append({
                    'body': func['body'],
                    'filename': filename,
                    'line': func['line'],
                })
    return bodies

def compute_similarity(sdk_body, port_body):
    """Compute similarity between two function bodies."""
    # Normalize both bodies
    def normalize(body):
        b = __import__('re').sub(r'\s+', ' ', body)
        b = __import__('re').sub(r'0x[0-9a-fA-F]+', 'N', b)
        b = __import__('re').sub(r'\b\d+\.\d+f?\b', 'F', b)
        b = __import__('re').sub(r'\b\d+f\b', 'F', b)
        b = __import__('re').sub(r'\b\d+\b', 'N', b)
        b = __import__('re').sub(r'"[^"]*"', 'S', b)
        return b
    return difflib.SequenceMatcher(None, normalize(sdk_body), normalize(port_body)).ratio()

def classify_divergence(ratio):
    """Classify divergence severity."""
    if ratio >= 0.95:
        return 'IDENTICAL'
    elif ratio >= 0.8:
        return 'MINOR'
    elif ratio >= 0.5:
        return 'MODERATE'
    elif ratio >= 0.2:
        return 'MAJOR'
    else:
        return 'COMPLETE'

def get_subsystem(filename):
    """Determine subsystem from filename."""
    name = filename.lower()
    if 'player' in name:
        return 'Player'
    elif 'actor' in name or 'ai_' in name or 'behavior' in name:
        return 'AI/Actor'
    elif 'weapon' in name or 'weap' in name or 'ammo' in name:
        return 'Weapons'
    elif 'animate' in name or 'anim' in name:
        return 'Animation'
    elif 'script' in name or 'program' in name or 'compiler' in name or 'interpreter' in name:
        return 'Script'
    elif 'navigate' in name or 'path' in name or 'follow' in name:
        return 'Navigation'
    elif 'multiplayer' in name or 'mp_' in name or 'team' in name:
        return 'Multiplayer'
    elif 'entity' in name or 'class' in name or 'listener' in name:
        return 'Entity/Core'
    elif 'trigger' in name or 'door' in name or 'mover' in name:
        return 'World'
    elif 'sound' in name or 'music' in name:
        return 'Sound'
    elif 'camera' in name or 'view' in name:
        return 'Camera'
    elif 'save' in name or 'archive' in name:
        return 'Save/Load'
    elif 'spawn' in name or 'level' in name or 'game' in name:
        return 'Core'
    else:
        return 'Other'

def generate_report(census):
    """Generate the comprehensive diff report."""
    report = []
    report.append("# EF2 Function Diff Report — Retail SDK vs Port vs IDA Binary")
    report.append(f"Generated from ef2_census.json\n")

    sdk_keys = set(census['sdk'].keys())
    port_keys = set(census['port'].keys())
    ida_keys = set(census['ida'].keys())

    # ── Summary ──────────────────────────────────────────────────────────
    report.append("## Summary")
    report.append(f"| Source | Unique Functions |")
    report.append(f"|--------|-----------------|")
    report.append(f"| Retail SDK | {len(sdk_keys)} |")
    report.append(f"| Port code | {len(port_keys)} |")
    report.append(f"| IDA binary | {len(ida_keys)} |")
    report.append("")

    in_all = sdk_keys & port_keys & ida_keys
    in_sdk_port = sdk_keys & port_keys
    in_sdk_ida = sdk_keys & ida_keys
    in_port_only = port_keys - sdk_keys - ida_keys
    in_sdk_only = sdk_keys - port_keys - ida_keys
    in_ida_only = ida_keys - sdk_keys - port_keys

    report.append(f"| Overlap | Count |")
    report.append(f"|---------|-------|")
    report.append(f"| In all three | {len(in_all)} |")
    report.append(f"| In SDK+Port | {len(in_sdk_port)} |")
    report.append(f"| In SDK+IDA | {len(in_sdk_ida)} |")
    report.append(f"| Port-only (MOHAA?) | {len(in_port_only)} |")
    report.append(f"| SDK-only (missing from port) | {len(in_sdk_only)} |")
    report.append(f"| IDA-only (not in SDK source) | {len(in_ida_only)} |")
    report.append("")

    # ── Missing Functions (SDK but not Port) ─────────────────────────────
    report.append("## Missing Functions (in Retail SDK but not in Port)")
    report.append("These functions exist in the retail SDK but are NOT implemented in the port.\n")

    missing = sorted(in_sdk_only)
    by_subsystem = collections.defaultdict(list)
    for key in missing:
        entries = census['sdk'][key]
        for e in entries:
            sub = get_subsystem(e['filename'])
            by_subsystem[sub].append((key, e))

    for sub in sorted(by_subsystem.keys()):
        items = by_subsystem[sub]
        report.append(f"### {sub} ({len(items)} missing)")
        report.append("| Function | File | Line |")
        report.append("|----------|------|------|")
        for key, e in sorted(items, key=lambda x: x[0]):
            report.append(f"| `{key}` | {e['filename']} | {e['line']} |")
        report.append("")

    # ── Extra Functions (Port but not SDK) ───────────────────────────────
    report.append("## Extra Functions (in Port but not in Retail SDK)")
    report.append("These functions exist in the port but NOT in the retail SDK. Likely MOHAA leftovers.\n")

    extra = sorted(in_port_only)
    by_subsystem = collections.defaultdict(list)
    for key in extra:
        entries = census['port'][key]
        for e in entries:
            sub = get_subsystem(e['filename'])
            by_subsystem[sub].append((key, e))

    for sub in sorted(by_subsystem.keys()):
        items = by_subsystem[sub]
        report.append(f"### {sub} ({len(items)} extra)")
        report.append("| Function | File | Line |")
        report.append("|----------|------|------|")
        for key, e in sorted(items, key=lambda x: x[0])[:50]:  # Limit to 50 per subsystem
            report.append(f"| `{key}` | {e['filename']} | {e['line']} |")
        if len(items) > 50:
            report.append(f"| ... and {len(items) - 50} more | | |")
        report.append("")

    # ── Divergent Functions (in both SDK and Port) ───────────────────────
    report.append("## Divergent Functions (in both SDK and Port)")
    report.append("These functions exist in both but have different implementations.\n")
    report.append("Loading function bodies for comparison (this may take a moment)...\n")

    # Load bodies
    sdk_bodies = load_function_bodies(lib.SDK)
    port_bodies = load_function_bodies(lib.PORT)

    divergent = []
    for key in sorted(in_sdk_port):
        if key in sdk_bodies and key in port_bodies:
            for sdk_entry in sdk_bodies[key]:
                for port_entry in port_bodies[key]:
                    ratio = compute_similarity(sdk_entry['body'], port_entry['body'])
                    if ratio < 0.95:  # Not identical
                        divergent.append({
                            'key': key,
                            'ratio': ratio,
                            'severity': classify_divergence(ratio),
                            'sdk_file': sdk_entry['filename'],
                            'sdk_line': sdk_entry['line'],
                            'port_file': port_entry['filename'],
                            'port_line': port_entry['line'],
                            'sdk_lines': sdk_entry['body'].count('\n') + 1,
                            'port_lines': port_entry['body'].count('\n') + 1,
                        })

    divergent.sort(key=lambda x: x['ratio'])

    report.append(f"Total divergent: {len(divergent)}\n")

    by_severity = collections.defaultdict(list)
    for d in divergent:
        by_severity[d['severity']].append(d)

    for sev in ['COMPLETE', 'MAJOR', 'MODERATE', 'MINOR']:
        items = by_severity.get(sev, [])
        if not items:
            continue
        report.append(f"### {sev} Divergence ({len(items)} functions)")
        report.append("| Function | Ratio | SDK File:Line | Port File:Line |")
        report.append("|----------|-------|---------------|----------------|")
        for d in items[:100]:  # Limit to 100 per severity
            report.append(f"| `{d['key']}` | {d['ratio']:.3f} | {d['sdk_file']}:{d['sdk_line']} | {d['port_file']}:{d['port_line']} |")
        if len(items) > 100:
            report.append(f"| ... and {len(items) - 100} more | | | |")
        report.append("")

    # ── IDA Cross-Reference ─────────────────────────────────────────────
    report.append("## IDA Binary Cross-Reference")
    report.append("Functions in the IDA decompilation that are NOT in the retail SDK source.\n")
    report.append("These may be inlined, auto-generated, or from linked libraries.\n")

    ida_only = sorted(in_ida_only)
    report.append(f"Total IDA-only: {len(ida_only)}\n")

    # Group by class
    by_class = collections.defaultdict(list)
    for key in ida_only:
        entries = census['ida'][key]
        for e in entries:
            by_class[e['class_name'] or '(standalone)'].append((key, e))

    report.append("| Class | Count | Sample Functions |")
    report.append("|-------|-------|-----------------|")
    for cls in sorted(by_class.keys(), key=lambda c: -len(by_class[c])):
        items = by_class[cls]
        samples = ', '.join(f'`{k}`' for k, _ in items[:3])
        if len(items) > 3:
            samples += f', +{len(items)-3} more'
        report.append(f"| {cls} | {len(items)} | {samples} |")
    report.append("")

    # ── Per-Subsystem Breakdown ─────────────────────────────────────────
    report.append("## Per-Subsystem Breakdown\n")

    all_files = set()
    for key in sdk_keys:
        for e in census['sdk'][key]:
            all_files.add(e['filename'])
    for key in port_keys:
        for e in census['port'][key]:
            all_files.add(e['filename'])

    by_sub = collections.defaultdict(lambda: {'sdk': 0, 'port': 0, 'ida': 0, 'missing': 0, 'extra': 0, 'divergent': 0})
    for key in sdk_keys:
        for e in census['sdk'][key]:
            sub = get_subsystem(e['filename'])
            by_sub[sub]['sdk'] += 1
    for key in port_keys:
        for e in census['port'][key]:
            sub = get_subsystem(e['filename'])
            by_sub[sub]['port'] += 1
    for key in ida_keys:
        for e in census['ida'][key]:
            sub = get_subsystem(e.get('filename', ''))
            by_sub[sub]['ida'] += 1

    for key in in_sdk_only:
        for e in census['sdk'][key]:
            sub = get_subsystem(e['filename'])
            by_sub[sub]['missing'] += 1
    for key in in_port_only:
        for e in census['port'][key]:
            sub = get_subsystem(e['filename'])
            by_sub[sub]['extra'] += 1

    report.append("| Subsystem | SDK | Port | IDA | Missing | Extra | Divergent |")
    report.append("|-----------|-----|------|-----|---------|-------|-----------|")
    for sub in sorted(by_sub.keys()):
        s = by_sub[sub]
        report.append(f"| {sub} | {s['sdk']} | {s['port']} | {s['ida']} | {s['missing']} | {s['extra']} | {s['divergent']} |")
    report.append("")

    # ── Top Divergent by Subsystem ───────────────────────────────────────
    report.append("## Top Divergent Functions by Subsystem\n")
    for sub in sorted(by_sub.keys()):
        sub_divergent = [d for d in divergent if get_subsystem(d['sdk_file']) == sub]
        if not sub_divergent:
            continue
        sub_divergent.sort(key=lambda x: x['ratio'])
        report.append(f"### {sub} (top 20)")
        report.append("| Function | Ratio | Severity | SDK | Port |")
        report.append("|----------|-------|----------|-----|------|")
        for d in sub_divergent[:20]:
            report.append(f"| `{d['key']}` | {d['ratio']:.3f} | {d['severity']} | {d['sdk_file']}:{d['sdk_line']} | {d['port_file']}:{d['port_line']} |")
        report.append("")

    return '\n'.join(report)

def main():
    census = load_census()
    report = generate_report(census)

    output_file = os.path.join(lib.WORK, 'EF2_FUNCTION_DIFF.md')
    with open(output_file, 'w') as f:
        f.write(report)
    print(f"Report saved to {output_file}", file=sys.stderr)

    # Also print summary to stdout
    lines = report.split('\n')
    for line in lines[:50]:
        print(line)
    if len(lines) > 50:
        print(f"... ({len(lines)} total lines)")

if __name__ == '__main__':
    main()
