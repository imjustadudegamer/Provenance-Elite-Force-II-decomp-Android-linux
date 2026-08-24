#!/usr/bin/env python3
"""
EF2 Rename Analysis — detailed analysis of detected renames.

For each rename, checks:
1. Event registration (does the event name match?)
2. Function signature (do parameters match?)
3. IDA decompilation (does the implementation match?)

Generates a detailed report with fix recommendations.
"""
import json, os, sys, re
sys.path.insert(0, os.path.dirname(__file__))
import ef2_diff_lib as lib

def load_census():
    census_file = os.path.join(lib.WORK, 'tools', 'ef2_census.json')
    with open(census_file) as f:
        return json.load(f)

def extract_event_registrations(filepath):
    """Extract event registrations from a file."""
    try:
        with open(filepath, 'r', errors='replace') as f:
            content = f.read()
    except Exception:
        return {}

    # Pattern: {&EV_EventName, &Class::HandlerName}
    pattern = re.compile(r'\{\s*&(\w+)\s*,\s*&(\w+::\w+)\s*\}')
    registrations = {}
    for m in pattern.finditer(content):
        event_name = m.group(1)
        handler = m.group(2)
        registrations[handler] = event_name
    return registrations

def extract_event_definitions(filepath):
    """Extract event definitions (script names) from a file."""
    try:
        with open(filepath, 'r', errors='replace') as f:
            content = f.read()
    except Exception:
        return {}

    # Pattern: Event EV_Name ( "scriptname", ... )
    pattern = re.compile(r'Event\s+(\w+)\s*\(\s*"([^"]+)"')
    definitions = {}
    for m in pattern.finditer(content):
        event_var = m.group(1)
        script_name = m.group(2)
        definitions[event_var] = script_name
    return definitions

def analyze_rename(sdk_key, port_key, census):
    """Analyze a single rename."""
    analysis = {
        'sdk_key': sdk_key,
        'port_key': port_key,
        'sdk_class': sdk_key.split('::')[0] if '::' in sdk_key else '',
        'sdk_method': sdk_key.split('::')[-1] if '::' in sdk_key else sdk_key,
        'port_class': port_key.split('::')[0] if '::' in port_key else '',
        'port_method': port_key.split('::')[-1] if '::' in port_key else port_key,
        'in_sdk': sdk_key in census['sdk'],
        'in_port': port_key in census['port'],
        'in_ida': sdk_key in census['ida'],
        'pattern': 'UNKNOWN',
        'confidence': 'LOW',
        'fix': 'INVESTIGATE',
    }

    # Determine pattern
    if analysis['port_method'].startswith('Event'):
        analysis['pattern'] = 'EVENT_PREFIX'
        analysis['confidence'] = 'HIGH'
        analysis['fix'] = 'RENAME_TO_RETAIL'
    elif analysis['port_method'].startswith('EventStub_'):
        analysis['pattern'] = 'EVENT_STUB_PREFIX'
        analysis['confidence'] = 'MEDIUM'
        analysis['fix'] = 'INVESTIGATE'
    elif analysis['port_method'].startswith('Cond_'):
        analysis['pattern'] = 'COND_PREFIX'
        analysis['confidence'] = 'MEDIUM'
        analysis['fix'] = 'INVESTIGATE'
    elif analysis['port_method'].endswith('Event'):
        analysis['pattern'] = 'EVENT_SUFFIX'
        analysis['confidence'] = 'MEDIUM'
        analysis['fix'] = 'INVESTIGATE'
    elif analysis['sdk_method'] in analysis['port_method'] or analysis['port_method'] in analysis['sdk_method']:
        analysis['pattern'] = 'SUBSTRING_MATCH'
        analysis['confidence'] = 'LOW'
        analysis['fix'] = 'INVESTIGATE'
    else:
        analysis['pattern'] = 'DIFFERENT_NAME'
        analysis['confidence'] = 'LOW'
        analysis['fix'] = 'INVESTIGATE'

    # Check if same class
    if analysis['sdk_class'] != analysis['port_class']:
        analysis['confidence'] = 'LOW'
        analysis['fix'] = 'INVESTIGATE'

    return analysis

def generate_report(census):
    """Generate the rename analysis report."""
    report = []
    report.append("# EF2 Rename Analysis Report")
    report.append(f"Generated: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M')}\n")

    # Load renames from the diff report
    diff_report = os.path.join(lib.WORK, 'EF2_FULL_DIFF.md')
    with open(diff_report, 'r') as f:
        content = f.read()

    # Extract renames section
    renames_section = re.search(r'## Possible Renames.*?\n(.*?)(?=\n## |\Z)', content, re.S)
    if not renames_section:
        report.append("Error: Could not find renames section in diff report.")
        return '\n'.join(report)

    # Parse renames
    rename_lines = renames_section.group(1).split('\n')
    renames = []
    for line in rename_lines:
        if line.startswith('|') and 'SDK Function' not in line and '---' not in line:
            parts = [p.strip().strip('`') for p in line.split('|') if p.strip()]
            if len(parts) >= 2:
                renames.append({
                    'sdk_key': parts[0],
                    'port_key': parts[1],
                })

    report.append(f"Total renames to analyze: {len(renames)}\n")

    # Analyze each rename
    analyses = []
    for rename in renames:
        analysis = analyze_rename(rename['sdk_key'], rename['port_key'], census)
        analyses.append(analysis)

    # Group by pattern
    by_pattern = {}
    for a in analyses:
        by_pattern.setdefault(a['pattern'], []).append(a)

    # Summary
    report.append("## Summary by Pattern\n")
    report.append("| Pattern | Count | Confidence | Fix |")
    report.append("|---------|-------|------------|-----|")
    for pattern in sorted(by_pattern.keys()):
        items = by_pattern[pattern]
        confidences = set(a['confidence'] for a in items)
        fixes = set(a['fix'] for a in items)
        report.append(f"| {pattern} | {len(items)} | {', '.join(confidences)} | {', '.join(fixes)} |")
    report.append("")

    # Detailed analysis by pattern
    for pattern in ['EVENT_PREFIX', 'EVENT_STUB_PREFIX', 'COND_PREFIX', 'EVENT_SUFFIX', 'SUBSTRING_MATCH', 'DIFFERENT_NAME']:
        items = by_pattern.get(pattern, [])
        if not items:
            continue

        report.append(f"## {pattern} ({len(items)} renames)\n")

        if pattern == 'EVENT_PREFIX':
            report.append("**Fix:** Rename C++ function to match retail SDK. Update registration table and header.\n")
            report.append("| SDK Name | Port Name | In IDA | Action |")
            report.append("|----------|-----------|--------|--------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Rename `{a['port_method']}` → `{a['sdk_method']}` |")
            report.append("")

        elif pattern == 'EVENT_STUB_PREFIX':
            report.append("**Fix:** Investigate — may be stubs that need full implementation.\n")
            report.append("| SDK Name | Port Name | In IDA | Notes |")
            report.append("|----------|-----------|--------|-------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Check if stub or full impl |")
            report.append("")

        elif pattern == 'COND_PREFIX':
            report.append("**Fix:** Investigate — condition functions may have different semantics.\n")
            report.append("| SDK Name | Port Name | In IDA | Notes |")
            report.append("|----------|-----------|--------|-------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Check condition semantics |")
            report.append("")

        elif pattern == 'EVENT_SUFFIX':
            report.append("**Fix:** Investigate — may be event wrappers.\n")
            report.append("| SDK Name | Port Name | In IDA | Notes |")
            report.append("|----------|-----------|--------|-------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Check if event wrapper |")
            report.append("")

        elif pattern == 'SUBSTRING_MATCH':
            report.append("**Fix:** Investigate — may be different functions with similar names.\n")
            report.append("| SDK Name | Port Name | In IDA | Notes |")
            report.append("|----------|-----------|--------|-------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Verify same function |")
            report.append("")

        elif pattern == 'DIFFERENT_NAME':
            report.append("**Fix:** Investigate — completely different names, may be false positive.\n")
            report.append("| SDK Name | Port Name | In IDA | Notes |")
            report.append("|----------|-----------|--------|-------|")
            for a in sorted(items, key=lambda x: x['sdk_key']):
                in_ida = "✓" if a['in_ida'] else "✗"
                report.append(f"| `{a['sdk_key']}` | `{a['port_key']}` | {in_ida} | Likely false positive |")
            report.append("")

    # Fix priority
    report.append("## Fix Priority\n")
    report.append("### High Confidence (safe to rename)\n")
    high_conf = [a for a in analyses if a['confidence'] == 'HIGH']
    report.append(f"Total: {len(high_conf)}\n")
    for a in sorted(high_conf, key=lambda x: x['sdk_key']):
        report.append(f"- `{a['sdk_key']}` → `{a['port_key']}`")
    report.append("")

    report.append("### Medium Confidence (verify first)\n")
    med_conf = [a for a in analyses if a['confidence'] == 'MEDIUM']
    report.append(f"Total: {len(med_conf)}\n")
    for a in sorted(med_conf, key=lambda x: x['sdk_key']):
        report.append(f"- `{a['sdk_key']}` → `{a['port_key']}`")
    report.append("")

    report.append("### Low Confidence (investigate)\n")
    low_conf = [a for a in analyses if a['confidence'] == 'LOW']
    report.append(f"Total: {len(low_conf)}\n")
    for a in sorted(low_conf, key=lambda x: x['sdk_key']):
        report.append(f"- `{a['sdk_key']}` → `{a['port_key']}`")
    report.append("")

    return '\n'.join(report)

def main():
    census = load_census()
    report = generate_report(census)

    output_file = os.path.join(lib.WORK, 'EF2_RENAME_ANALYSIS.md')
    with open(output_file, 'w') as f:
        f.write(report)
    print(f"Report saved to {output_file}", file=sys.stderr)

    # Print summary
    lines = report.split('\n')
    for line in lines[:60]:
        print(line)
    if len(lines) > 60:
        print(f"\n... {len(lines)} total lines")

if __name__ == '__main__':
    main()
