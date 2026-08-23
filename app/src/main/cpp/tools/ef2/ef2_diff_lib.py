#!/usr/bin/env python3
"""
EF2 Diff Library — shared utilities for comparing retail SDK, port, and IDA decompilations.

Data sources:
  - Retail SDK:  personal/evidence/ef2-sdk/ef2-sdk/dlls/game/  (151 .cpp + 131 .h)
  - Port code:   app/src/main/cpp/code/fgame/        (180 .cpp + 164 .h)
  - IDA decomp:  personal/evidence/mac_symbols/game_decomp/     (9025 .c files)
  - Symbol table: personal/evidence/mac_symbols/game_dll_symbols.txt
  - Import table: personal/evidence/mac_symbols/game_import_t_EF2.txt
"""
import os
import re, os, sys, glob, collections

# ── Paths ──────────────────────────────────────────────────────────────────
WORK   = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/work"
SDK    = WORK + "/ef2-sdk/ef2-sdk/dlls/game"
PORT   = WORK + "/openmohaa/code/fgame"
DECOMP = WORK + "/mac_symbols/game_decomp"
SYMTAB = WORK + "/mac_symbols/game_dll_symbols.txt"
IMPORT = WORK + "/mac_symbols/game_import_t_EF2.txt"
IMPORT_MISSING = WORK + "/mac_symbols/game_import_t_MISSING_from_port.txt"

# ── Comment stripping ──────────────────────────────────────────────────────
def strip_comments(src):
    """Remove C and C++ comments from source."""
    src = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    return src

# ── Function extraction from C++ source ────────────────────────────────────
# Matches: [return_type] ClassName::MethodName(params) {
# Also matches: ClassName::ClassName(params) {  (constructor)
# Also matches: ClassName::~ClassName(params) { (destructor)
_FUNC_RE = re.compile(
    r'^([A-Za-z_]\w*)\s*'                  # return type
    r'([*&]?\s*)'                           # optional pointer/reference
    r'((?:~\s*)?[A-Za-z_]\w*(?:::[A-Za-z_~]\w*)*)'  # class::method or just method
    r'\s*\((.*?)\)\s*'                      # params (may span lines, lazy)
    r'(?:const\s*)?'                        # optional const
    r'\{',                                  # opening brace
    re.MULTILINE | re.DOTALL
)

def extract_functions(src, filename="<unknown>"):
    """
    Extract all function definitions from C++ source.
    Returns list of dicts: {name, class_name, method_name, return_type, params, body, line, filename}
    """
    src = strip_comments(src)
    funcs = []
    lines = src.split('\n')

    for m in _FUNC_RE.finditer(src):
        return_type = (m.group(1).strip() + ' ' + m.group(2).strip()).strip()
        full_name = m.group(3).strip()
        params = m.group(4).strip()

        # Skip control flow keywords
        if full_name in ('if', 'for', 'while', 'switch', 'return', 'else', 'do', 'catch'):
            continue

        # Parse class::method
        if '::' in full_name:
            parts = full_name.split('::')
            class_name = '::'.join(parts[:-1])
            method_name = parts[-1]
        else:
            class_name = ''
            method_name = full_name

        # Skip if method_name looks like a type (e.g., "int", "void")
        if method_name in ('int', 'void', 'float', 'char', 'short', 'long', 'double',
                           'unsigned', 'signed', 'bool', 'qboolean', 'byte', 'word',
                           'DWORD', 'NULL', 'true', 'false'):
            continue

        # Find the matching closing brace
        depth = 0
        j = m.end() - 1
        while j < len(src):
            if src[j] == '{':
                depth += 1
            elif src[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1

        if depth != 0:
            continue  # Unmatched braces

        body = src[m.start():j+1]
        line_num = src[:m.start()].count('\n') + 1

        funcs.append({
            'name': full_name,
            'class_name': class_name,
            'method_name': method_name,
            'return_type': return_type,
            'params': params,
            'body': body,
            'line': line_num,
            'filename': filename,
        })

    return funcs

def extract_functions_from_file(filepath):
    """Extract functions from a single file."""
    try:
        with open(filepath, 'r', errors='replace') as f:
            src = f.read()
    except Exception:
        return []
    return extract_functions(src, os.path.basename(filepath))

def extract_functions_from_dir(dirpath, extensions=('.cpp', '.h')):
    """Extract all functions from all files in a directory."""
    all_funcs = {}
    for ext in extensions:
        for filepath in glob.glob(os.path.join(dirpath, '*' + ext)):
            filename = os.path.basename(filepath)
            funcs = extract_functions_from_file(filepath)
            for f in funcs:
                f['filename'] = filename
            all_funcs[filename] = funcs
    return all_funcs

# ── IDA decompilation filename parsing ─────────────────────────────────────
# IDA filenames: MethodName__NClassNameCFParams.c  (const method)
#              : MethodName__NClassNameFParams.c   (non-const method)
# N is the decimal length of the class name. C is optional const qualifier.
# F separates the class name (and optional C) from the mangled parameter list.
_IDA_PREFIX_RE = re.compile(r'^([A-Za-z_]\w*)__(\d+)')
_IDA_STANDALONE_RE = re.compile(r'^([A-Za-z_]\w*)\.c$')

def parse_ida_filename(filename):
    """
    Parse an IDA decompilation filename.
    Returns: {method_name, class_name, class_len, params_mangled, is_const, is_standalone}
    """
    m = _IDA_PREFIX_RE.match(filename)
    if m:
        method_name = m.group(1)
        class_len = int(m.group(2))
        rest = filename[m.end():]
        if len(rest) < class_len:
            return None
        class_name = rest[:class_len]
        rest2 = rest[class_len:]
        is_const = rest2.startswith('C')
        if is_const:
            rest2 = rest2[1:]
        if not rest2.startswith('F'):
            return None
        params = rest2[1:].rstrip('.c')
        return {
            'method_name': method_name,
            'class_name': class_name,
            'class_len': class_len,
            'params_mangled': params,
            'is_const': is_const,
            'is_standalone': False,
            'filename': filename,
        }
    m = _IDA_STANDALONE_RE.match(filename)
    if m:
        return {
            'method_name': m.group(1),
            'class_name': '',
            'class_len': 0,
            'params_mangled': '',
            'is_const': False,
            'is_standalone': True,
            'filename': filename,
        }
    return None

def sdk_func_to_ida_filename(class_name, method_name, is_const=False):
    """
    Generate the expected IDA filename for an SDK function.
    Returns the filename or None if not applicable.
    """
    if class_name:
        c_suffix = 'C' if is_const else ''
        return f"{method_name}__{len(class_name)}{class_name}{c_suffix}F.c"
    else:
        return f"{method_name}.c"

# ── Symbol table loading ──────────────────────────────────────────────────
def load_symbol_table():
    """
    Load the game_dll_symbols.txt file.
    Returns: dict of mangled_name -> {address, size, demangled}
    """
    symbols = {}
    try:
        with open(SYMTAB, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                # Format: 0xADDRESS  SIZE  .MANGLED_NAME  DEMANGLED_NAME
                m = re.match(r'(0x[0-9a-f]+)\s+(\d+)\s+\.(\S+)\s+(.*)', line)
                if m:
                    addr = int(m.group(1), 16)
                    size = int(m.group(2))
                    mangled = m.group(3)
                    demangled = m.group(4).strip()
                    symbols[mangled] = {
                        'address': addr,
                        'size': size,
                        'mangled': mangled,
                        'demangled': demangled,
                    }
    except Exception as e:
        print(f"Warning: Could not load symbol table: {e}", file=sys.stderr)
    return symbols

# ── Import table loading ──────────────────────────────────────────────────
def load_import_table():
    """
    Load the game_import_t_EF2.txt file.
    Returns: list of {index, offset, name}
    """
    imports = []
    try:
        with open(IMPORT, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                # Format: [INDEX] +OFFSET  NAME
                m = re.match(r'\[\s*(\d+)\]\s+\+?(0x[0-9a-f]+)\s+(\S+)', line)
                if m:
                    imports.append({
                        'index': int(m.group(1)),
                        'offset': int(m.group(2), 16),
                        'name': m.group(3),
                    })
    except Exception as e:
        print(f"Warning: Could not load import table: {e}", file=sys.stderr)
    return imports

def load_missing_imports():
    """
    Load the game_import_t_MISSING_from_port.txt file.
    Returns: list of {offset, index, name}
    """
    missing = []
    try:
        with open(IMPORT_MISSING, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                # Format: +OFFSET  [INDEX]  NAME
                m = re.match(r'\+?(0x[0-9a-f]+)\s+\[\s*(\d+)\]\s+(\S+)', line)
                if m:
                    missing.append({
                        'offset': int(m.group(1), 16),
                        'index': int(m.group(2)),
                        'name': m.group(3),
                    })
    except Exception as e:
        print(f"Warning: Could not load missing imports: {e}", file=sys.stderr)
    return missing

# ── IDA decompilation loading ─────────────────────────────────────────────
def load_ida_decomp(filename):
    """
    Load an IDA decompilation file and extract its content.
    Returns: {address, size, body} or None
    """
    filepath = os.path.join(DECOMP, filename)
    if not os.path.exists(filepath):
        return None
    try:
        with open(filepath, 'r', errors='replace') as f:
            content = f.read()
        # Extract address and size from header comment
        m = re.search(r'@0x([0-9a-fA-F]+)\s+size=(\d+)', content)
        addr = int(m.group(1), 16) if m else 0
        size = int(m.group(2)) if m else 0
        return {
            'address': addr,
            'size': size,
            'body': content,
            'filename': filename,
        }
    except Exception:
        return None

# ── Function signature normalization ──────────────────────────────────────
def normalize_sig(sig):
    """Normalize a function signature for comparison."""
    # Remove extra whitespace
    sig = re.sub(r'\s+', ' ', sig).strip()
    # Remove parameter names, keep types
    # This is approximate — good enough for comparison
    return sig

def func_key(class_name, method_name):
    """Generate a unique key for a function."""
    if class_name:
        return f"{class_name}::{method_name}"
    return method_name

# ── Class hierarchy extraction ────────────────────────────────────────────
_CLASS_RE = re.compile(
    r'class\s+(\w+)\s*'
    r'(?::\s*(?:public|protected|private)\s+([^{]+?))?\s*\{',
    re.S
)

def extract_classes_from_file(filepath):
    """Extract class declarations from a header file."""
    try:
        with open(filepath, 'r', errors='replace') as f:
            src = f.read()
    except Exception:
        return []

    src = strip_comments(src)
    classes = []
    for m in _CLASS_RE.finditer(src):
        class_name = m.group(1)
        parent = m.group(2).strip() if m.group(2) else ''
        # Clean up parent (remove multiple inheritance extras)
        parent = parent.split(',')[0].strip()
        classes.append({
            'name': class_name,
            'parent': parent,
            'file': os.path.basename(filepath),
        })
    return classes

def extract_classes_from_dir(dirpath):
    """Extract all class declarations from a directory."""
    all_classes = {}
    for filepath in glob.glob(os.path.join(dirpath, '*.h')):
        classes = extract_classes_from_file(filepath)
        for c in classes:
            all_classes[c['name']] = c
    return all_classes

# ── Utility ───────────────────────────────────────────────────────────────
def count_lines(body):
    """Count lines in a function body."""
    return body.count('\n') + 1

def body_hash(body):
    """Simple hash of a function body for quick comparison."""
    # Normalize whitespace and remove literals
    b = re.sub(r'\s+', ' ', body)
    b = re.sub(r'0x[0-9a-fA-F]+', 'N', b)
    b = re.sub(r'\b\d+\.\d+f?\b', 'F', b)
    b = re.sub(r'\b\d+f\b', 'F', b)
    b = re.sub(r'\b\d+\b', 'N', b)
    b = re.sub(r'"[^"]*"', 'S', b)
    return hash(b)

def similarity(a, b):
    """Compute similarity ratio between two strings."""
    from difflib import SequenceMatcher
    return SequenceMatcher(None, a, b).ratio()
