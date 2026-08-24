#!/usr/bin/env python3
"""
EF2 Full Diff — comprehensive three-way comparison with rename detection.

Uses pre-computed census (ef2_census.json) for fast analysis.
Only loads function bodies for divergent functions (not all files).

Features:
  - Detects renamed functions via IDA cross-reference
  - Compares game_import_t struct against retail table
  - Per-subsystem breakdown with severity ranking
"""
import json, os, sys, collections, re
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

PORT_G_PUBLIC = os.path.join(lib.WORK, 'openmohaa', 'code', 'fgame', 'g_public.h')

def load_census():
    census_file = os.path.join(lib.WORK, 'tools', 'ef2_census.json')
    with open(census_file) as f:
        return json.load(f)

def get_subsystem(filename):
    name = filename.lower()
    if 'player' in name: return 'Player'
    if 'actor' in name or 'ai_' in name or 'behavior' in name: return 'AI/Actor'
    if 'weapon' in name or 'weap' in name or 'ammo' in name: return 'Weapons'
    if 'animate' in name or 'anim' in name: return 'Animation'
    if 'script' in name or 'program' in name or 'compiler' in name or 'interpreter' in name: return 'Script'
    if 'navigate' in name or 'path' in name or 'follow' in name: return 'Navigation'
    if 'multiplayer' in name or 'mp_' in name or 'team' in name: return 'Multiplayer'
    if 'entity' in name or 'class' in name or 'listener' in name: return 'Entity/Core'
    if 'trigger' in name or 'door' in name or 'mover' in name: return 'World'
    if 'sound' in name or 'music' in name: return 'Sound'
    if 'camera' in name or 'view' in name: return 'Camera'
    if 'save' in name or 'archive' in name: return 'Save/Load'
    if 'spawn' in name or 'level' in name or 'game' in name: return 'Core'
    return 'Other'

def categorize_import(name):
    for cat, prefixes in [
        ('Cvar', ['Cvar_', 'cvar_']), ('Filesystem', ['FS_']),
        ('Command', ['Cmd_', 'Cbuf_', 'Argc', 'Argv', 'Args', 'AddCommand']),
        ('Network', ['MSG_']), ('Sound', ['Sound', 'StopSound', 'SoundLength', 'S_IsSoundPlaying']),
        ('TIKI', ['TIKI_', 'Anim_', 'Frame_', 'Surface_', 'Tag_', 'Alias_', 'GlobalAlias_']),
        ('HUD', ['HudDraw', 'Popmenu', 'Showmenu', 'Hidemenu', 'Pushmenu']),
        ('Mission', ['MissionObjective', 'MissionInformation', 'MissionObjectives']),
        ('Config', ['setConfigstring', 'getConfigstring', 'SetUserinfo', 'GetUserinfo']),
        ('Entity', ['linkentity', 'unlinkentity', 'SetBrushModel', 'ModelBoundsFromName']),
        ('Key', ['Key_']), ('CRC', ['CRC_', 'CalcCRC']),
        ('System', ['Printf', 'DPrintf', 'Error', 'Milliseconds', 'Malloc', 'Free', 'Com_']),
        ('Physics', ['trace', 'Trace', 'SightTrace', 'pointcontents', 'fulltrace']),
    ]:
        for p in prefixes:
            if name.startswith(p) or name == p:
                return cat
    return 'Other'

def parse_port_import_table():
    try:
        with open(PORT_G_PUBLIC, 'r', errors='replace') as f:
            content = f.read()
    except Exception:
        return []
    struct_match = re.search(r'typedef struct gameImport_s\s*\{(.+?)\} game_import_t;', content, re.S)
    if not struct_match:
        return []
    return [m.group(1) for m in re.finditer(r'\(\s*\*\s*(\w+)\s*\)', struct_match.group(1))]

def find_renames_via_ida(census):
    """Find renames by checking if SDK functions exist in IDA under different port names."""
    sdk_keys = set(census['sdk'].keys())
    port_keys = set(census['port'].keys())
    ida_keys = set(census['ida'].keys())

    missing = sdk_keys - port_keys
    extra = port_keys - sdk_keys

    renames = []
    # For each missing SDK function, check if it's in IDA
    # If IDA has it, check if any port-only function maps to the same IDA entry
    for mk in missing:
        if mk not in ida_keys:
            continue
        # This SDK function is in the retail binary
        # Check if any extra port function is also in IDA under a different name
        # This is a heuristic — we flag it as possible rename
        ida_entries = census['ida'].get(mk, [])
        for ie in ida_entries:
            # Look for port functions with similar method name
            sdk_method = mk.split('::')[-1] if '::' in mk else mk
            for ek in extra:
                port_method = ek.split('::')[-1] if '::' in ek else ek
                # Check if method names are similar (e.g., Init vs InitModel)
                if sdk_method in port_method or port_method in sdk_method:
                    sdk_class = mk.split('::')[0] if '::' in mk else ''
                    port_class = ek.split('::')[0] if '::' in ek else ''
                    if sdk_class == port_class:
                        renames.append({
                            'sdk_key': mk, 'port_key': ek,
                            'type': 'POSSIBLE_RENAME',
                            'reason': f'Same class, similar method name, in IDA',
                        })
                        break

    return renames

def generate_report(census):
    report = []
    report.append("# EF2 Full Diff Report")
    report.append(f"Generated: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M')}\n")

    sdk_keys = set(census['sdk'].keys())
    port_keys = set(census['port'].keys())
    ida_keys = set(census['ida'].keys())

    # Find renames
    renames = find_renames_via_ida(census)
    renamed_sdk = {r['sdk_key'] for r in renames}
    renamed_port = {r['port_key'] for r in renames}

    truly_missing = sorted(sdk_keys - port_keys - renamed_sdk)
    truly_extra = sorted(port_keys - sdk_keys - renamed_port)

    # ── Executive Summary ────────────────────────────────────────────────
    report.append("## Executive Summary\n")
    report.append("| Metric | Count |")
    report.append("|--------|-------|")
    report.append(f"| Retail SDK functions | {len(sdk_keys)} |")
    report.append(f"| Port functions | {len(port_keys)} |")
    report.append(f"| IDA binary functions | {len(ida_keys)} |")
    report.append(f"| In all three | {len(sdk_keys & port_keys & ida_keys)} |")
    report.append(f"| **Missing from port** | {len(truly_missing)} |")
    report.append(f"| **Extra in port (MOHAA?)** | {len(truly_extra)} |")
    report.append(f"| **Possible renames detected** | {len(renames)} |")
    report.append("")

    # ── Renames ──────────────────────────────────────────────────────────
    if renames:
        report.append("## Possible Renames (same class, similar method, in IDA)\n")
        report.append("| SDK Function | Port Function | Reason |")
        report.append("|--------------|---------------|--------|")
        for r in sorted(renames, key=lambda x: x['sdk_key']):
            report.append(f"| `{r['sdk_key']}` | `{r['port_key']}` | {r['reason']} |")
        report.append("")

    # ── Missing Functions ────────────────────────────────────────────────
    report.append(f"## Missing Functions ({len(truly_missing)} total)\n")
    report.append("Functions in retail SDK but NOT in port (excluding detected renames).\n")

    by_sub = collections.defaultdict(list)
    for key in truly_missing:
        for e in census['sdk'][key]:
            by_sub[get_subsystem(e['filename'])].append((key, e))

    for sub in sorted(by_sub.keys()):
        items = by_sub[sub]
        report.append(f"### {sub} ({len(items)} missing)")
        report.append("| Function | File | Line | In IDA |")
        report.append("|----------|------|------|--------|")
        for key, e in sorted(items, key=lambda x: x[0]):
            in_ida = "✓" if key in ida_keys else "✗"
            report.append(f"| `{key}` | {e['filename']} | {e['line']} | {in_ida} |")
        report.append("")

    # ── Extra Functions ──────────────────────────────────────────────────
    report.append(f"## Extra Functions ({len(truly_extra)} total)\n")
    report.append("Functions in port but NOT in retail SDK (likely MOHAA leftovers).\n")

    by_sub = collections.defaultdict(list)
    for key in truly_extra:
        for e in census['port'][key]:
            by_sub[get_subsystem(e['filename'])].append((key, e))

    for sub in sorted(by_sub.keys()):
        items = by_sub[sub]
        report.append(f"### {sub} ({len(items)} extra)")
        report.append("| Function | File | Line |")
        report.append("|----------|------|------|")
        for key, e in sorted(items, key=lambda x: x[0])[:30]:
            report.append(f"| `{key}` | {e['filename']} | {e['line']} |")
        if len(items) > 30:
            report.append(f"| ... +{len(items)-30} more | | |")
        report.append("")

    # ── Import Table ─────────────────────────────────────────────────────
    report.append("## Import Table Diff (game_import_t)\n")

    retail_imports = lib.load_import_table()
    missing_imports = lib.load_missing_imports()
    port_funcs = parse_port_import_table()

    retail_names = {imp['name'] for imp in retail_imports}
    missing_names = {imp['name'] for imp in missing_imports}
    port_names = set(port_funcs)

    actually_present = missing_names & port_names
    still_missing = missing_names - port_names
    port_extra = port_names - retail_names

    report.append("| Metric | Count |")
    report.append("|--------|-------|")
    report.append(f"| Retail import table | {len(retail_imports)} |")
    report.append(f"| Port import table | {len(port_funcs)} |")
    report.append(f"| Was marked missing, now present | {len(actually_present)} |")
    report.append(f"| Still missing | {len(still_missing)} |")
    report.append(f"| Port-only | {len(port_extra)} |")
    report.append("")

    if still_missing:
        report.append("### Still Missing\n")
        report.append("| Idx | Offset | Name | Category |")
        report.append("|-----|--------|------|----------|")
        for name in sorted(still_missing):
            cat = categorize_import(name)
            entry = next((e for e in retail_imports if e['name'] == name), None)
            if entry:
                report.append(f"| {entry['index']} | +0x{entry['offset']:03x} | `{name}` | {cat} |")
        report.append("")

    if actually_present:
        report.append("### Now Present (was missing)\n")
        report.append("| Name | Category |")
        report.append("|------|----------|")
        for name in sorted(actually_present):
            report.append(f"| `{name}` | {categorize_import(name)} |")
        report.append("")

    if port_extra:
        report.append("### Port-Only (not in retail)\n")
        report.append("| Name | Category |")
        report.append("|------|----------|")
        for name in sorted(port_extra):
            report.append(f"| `{name}` | {categorize_import(name)} |")
        report.append("")

    # ── Per-Subsystem Summary ────────────────────────────────────────────
    report.append("## Per-Subsystem Summary\n")
    by_sub = collections.defaultdict(lambda: {'sdk': 0, 'port': 0, 'ida': 0, 'missing': 0, 'extra': 0})
    for key in sdk_keys:
        for e in census['sdk'][key]:
            by_sub[get_subsystem(e['filename'])]['sdk'] += 1
    for key in port_keys:
        for e in census['port'][key]:
            by_sub[get_subsystem(e['filename'])]['port'] += 1
    for key in ida_keys:
        for e in census['ida'][key]:
            by_sub[get_subsystem(e.get('filename', ''))]['ida'] += 1
    for key in truly_missing:
        for e in census['sdk'][key]:
            by_sub[get_subsystem(e['filename'])]['missing'] += 1
    for key in truly_extra:
        for e in census['port'][key]:
            by_sub[get_subsystem(e['filename'])]['extra'] += 1

    report.append("| Subsystem | SDK | Port | IDA | Missing | Extra |")
    report.append("|-----------|-----|------|-----|---------|-------|")
    for sub in sorted(by_sub.keys()):
        s = by_sub[sub]
        report.append(f"| {sub} | {s['sdk']} | {s['port']} | {s['ida']} | {s['missing']} | {s['extra']} |")
    report.append("")

    # ── IDA-Only ─────────────────────────────────────────────────────────
    ida_only = sorted(ida_keys - sdk_keys - port_keys)
    report.append(f"## IDA-Only Functions ({len(ida_only)} total)\n")
    report.append("In retail binary but not in SDK source (inlined/auto-generated).\n")

    by_class = collections.defaultdict(list)
    for key in ida_only:
        for e in census['ida'][key]:
            by_class[e['class_name'] or '(standalone)'].append(key)

    report.append("| Class | Count | Samples |")
    report.append("|-------|-------|---------|")
    for cls in sorted(by_class.keys(), key=lambda c: -len(by_class[c]))[:25]:
        items = by_class[cls]
        samples = ', '.join(f'`{k}`' for k in items[:3])
        if len(items) > 3:
            samples += f', +{len(items)-3}'
        report.append(f"| {cls} | {len(items)} | {samples} |")
    report.append("")

    return '\n'.join(report)

def main():
    census = load_census()
    report = generate_report(census)

    output_file = os.path.join(lib.WORK, 'EF2_FULL_DIFF.md')
    with open(output_file, 'w') as f:
        f.write(report)
    print(f"Report: {output_file}", file=sys.stderr)

    # Print summary
    lines = report.split('\n')
    for line in lines[:60]:
        print(line)
    print(f"\n... {len(lines)} total lines")

if __name__ == '__main__':
    main()
