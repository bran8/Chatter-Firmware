#!/usr/bin/env python3
"""
gen_t9_dict.py
--------------
Reads tools/8293.txt (one word per line, most-frequent first) and builds a
frequency-weighted *character* prefix tree (Trie). Each edge of the trie is a
single letter; a complete word ends at the node reached by spelling it out.

The trie is serialised in **breadth-first (BFS)** order so that every node's
children occupy a contiguous run in the output array. Each node carries a
pre-computed ``max_weight`` (the maximum word weight anywhere in its subtree),
which lets the firmware predict the best completion in O(L) time without
scanning the subtree at runtime.

Why a *character* trie (not a digit trie)?
  A digit trie would collapse colliding words ("good"/"home"/"gone"/"hood" all
  map to 4663) onto a single node, forcing a side table of word strings. A
  character trie keeps one letter per edge, so each word is uniquely recovered
  by walking the path from the root — no string table required. T9 lookup
  simply follows, at each step, every child whose letter belongs to the typed
  digit's letter set.

Outputs
───────
  src/t9_dict.h    – extern declarations + size constants (NO array body)
  src/t9_dict.cpp  – the const uint8_t[] blob (lands in Flash .rodata)
  src/t9_dict.bin  – raw binary (handy for inspection / host-side tests)

Binary layout
─────────────
[0..1]  uint16_t  node_count   – total number of nodes (root included)
[2..3]  uint16_t  reserved     – 0
[4..]   node_count × 8 bytes, in BFS order:
            char     character        (1 byte)  letter 'a'..'z', 0 for root
            uint8_t  child_count       (1 byte)
            uint16_t weight            (2 bytes) word weight, 0 if not a word-end
            uint16_t max_weight        (2 bytes) max weight in this subtree
            uint16_t first_child_index (2 bytes) BFS index of first child, 0 if leaf

Weights are frequency proxies derived from the input rank: the most-frequent
word (line 0) gets the highest weight, the least-frequent gets 1. Higher weight
== more frequent, so sorting candidates by weight descending puts the most
likely word first.
"""

import struct
import sys
from collections import deque
from pathlib import Path

# ── Phone key mapping ────────────────────────────────────────────────────────

_DIGIT_LETTERS = {
    '2': 'abc',
    '3': 'def',
    '4': 'ghi',
    '5': 'jkl',
    '6': 'mno',
    '7': 'pqrs',
    '8': 'tuv',
    '9': 'wxyz',
}

CHAR_TO_DIGIT: dict[str, str] = {}
for _digit, _letters in _DIGIT_LETTERS.items():
    for _ch in _letters:
        CHAR_TO_DIGIT[_ch] = _digit


def word_to_digits(word: str) -> str | None:
    """Return the T9 digit string for *word*, or None if any char is unmappable."""
    out = []
    for ch in word.lower():
        d = CHAR_TO_DIGIT.get(ch)
        if d is None:
            return None
        out.append(d)
    return ''.join(out)


# ── Load word list ───────────────────────────────────────────────────────────

def load_words(path: str) -> list[str]:
    """Load unique, purely-alphabetic, lowercase words, preserving rank order."""
    words = []
    seen = set()
    with open(path, encoding='utf-8') as f:
        for line in f:
            word = line.strip().lower()
            if not word or any(ch < 'a' or ch > 'z' for ch in word):
                continue
            if word in seen:
                continue
            seen.add(word)
            words.append(word)
    return words


# ── Build the frequency trie ─────────────────────────────────────────────────

class TrieNode:
    __slots__ = ("char", "weight", "max_weight", "children",
                 "bfs_index", "first_child_index", "ordered_children")

    def __init__(self, char: str = '\0'):
        self.char = char
        self.weight = 0                       # > 0 only when a word ends here
        self.max_weight = 0                   # max weight in this subtree
        self.children: dict[str, "TrieNode"] = {}
        self.bfs_index = -1
        self.first_child_index = 0
        self.ordered_children: list["TrieNode"] = []


def build_trie(words: list[str]) -> tuple[TrieNode, int]:
    """Insert every word, assigning weight = (count - rank) so the most
    frequent word gets the largest weight. Returns (root, word_count)."""
    n = len(words)
    root = TrieNode('\0')
    for rank, word in enumerate(words):
        weight = n - rank                     # rank 0 -> weight n (most frequent)
        node = root
        for ch in word:
            child = node.children.get(ch)
            if child is None:
                child = TrieNode(ch)
                node.children[ch] = child
            node = child
        # If a duplicate spelling somehow survives dedup, keep the higher weight.
        node.weight = max(node.weight, weight)
    return root, n


def compute_max_weight(root: TrieNode) -> None:
    """Post-order pass filling each node's max_weight (iterative to avoid any
    recursion-depth concerns on pathologically long words)."""
    # Build a post-order via an explicit stack.
    order: list[TrieNode] = []
    stack = [root]
    while stack:
        node = stack.pop()
        order.append(node)
        stack.extend(node.children.values())
    # order is a valid reverse-topological listing (parents before children),
    # so processing it in reverse guarantees children are done before parents.
    for node in reversed(order):
        m = node.weight
        for child in node.children.values():
            if child.max_weight > m:
                m = child.max_weight
        node.max_weight = m


def assign_bfs_layout(root: TrieNode) -> list[TrieNode]:
    """Assign BFS indices and first_child_index to every node.

    Children are emitted sorted by max_weight descending (tie-break: letter
    ascending) so a greedy best-first descent naturally meets the most likely
    completion first. Returns the node list in BFS-index order."""
    order: list[TrieNode] = []
    root.bfs_index = 0
    queue = deque([root])
    next_index = 1
    while queue:
        node = queue.popleft()
        order.append(node)
        kids = sorted(node.children.values(),
                      key=lambda c: (-c.max_weight, c.char))
        node.ordered_children = kids
        node.first_child_index = next_index if kids else 0
        for kid in kids:
            kid.bfs_index = next_index
            next_index += 1
            queue.append(kid)
    # Sanity: BFS visit order equals index order.
    assert all(order[i].bfs_index == i for i in range(len(order)))
    return order


# ── Serialise ────────────────────────────────────────────────────────────────

HEADER_STRUCT = struct.Struct('<HH')          # node_count, reserved
NODE_STRUCT = struct.Struct('<BBHHH')         # char, child_count, weight, max_weight, first_child
NODE_SIZE = NODE_STRUCT.size                  # 8 bytes

assert NODE_SIZE == 8, "Node size must be 8 bytes"


def serialise(order: list[TrieNode]) -> bytes:
    node_count = len(order)
    if node_count > 0xFFFF:
        raise ValueError(
            f"Too many nodes: {node_count} (max 65535). first_child_index and "
            f"node_count are uint16. Trim the word list or cap word length."
        )

    body = bytearray()
    for node in order:
        char_byte = ord(node.char) if node.char != '\0' else 0
        body += NODE_STRUCT.pack(
            char_byte,
            len(node.ordered_children),
            node.weight,
            node.max_weight,
            node.first_child_index,
        )

    return HEADER_STRUCT.pack(node_count, 0) + bytes(body)


# ── Emit C source ────────────────────────────────────────────────────────────

def emit_header(node_count: int, blob_size: int, out_path: Path) -> None:
    text = f"""// AUTO-GENERATED by gen_t9_dict.py — do not edit by hand
#pragma once
#include <stdint.h>

#define T9_NODE_COUNT  {node_count}u
#define T9_HEADER_SIZE 4u
#define T9_NODE_SIZE   8u

// The blob lives in src/t9_dict.cpp so the linker places it once, in Flash
// (.rodata), rather than duplicating it into every translation unit that
// includes this header.
extern const uint8_t  t9DictData[];
extern const uint32_t t9DictSize;
"""
    out_path.write_text(text, encoding='utf-8')


def emit_source(blob: bytes, hdr_name: str, out_path: Path) -> None:
    lines = [
        "// AUTO-GENERATED by gen_t9_dict.py — do not edit by hand",
        f'#include "{hdr_name}"',
        "",
        "const uint8_t t9DictData[] = {",
    ]
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines += [
        "};",
        "",
        f"const uint32_t t9DictSize = {len(blob)}u;",
        "",
    ]
    out_path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


# ── Host-side verification (mirrors the firmware lookup) ──────────────────────

def parse_blob(blob: bytes) -> list[tuple[str, int, int, int, int]]:
    """Parse the serialised blob back into a list of node tuples:
    (char, child_count, weight, max_weight, first_child_index)."""
    node_count, _reserved = HEADER_STRUCT.unpack_from(blob, 0)
    nodes = []
    off = HEADER_STRUCT.size
    for _ in range(node_count):
        char_byte, child_count, weight, max_weight, first_child = \
            NODE_STRUCT.unpack_from(blob, off)
        nodes.append((
            chr(char_byte) if char_byte else '\0',
            child_count, weight, max_weight, first_child,
        ))
        off += NODE_SIZE
    return nodes


def blob_exact_matches(nodes, digits: str) -> list[tuple[str, int]]:
    """Return (word, weight) for every word whose T9 encoding is EXACTLY
    `digits`, sorted by weight descending. This is the firmware getMatches
    semantics: the collision group for the typed key sequence."""
    results: list[tuple[str, int]] = []

    def dfs(idx: int, depth: int, prefix: str) -> None:
        _char, child_count, weight, _max_weight, first_child = nodes[idx]
        if depth == len(digits):
            if weight > 0:
                results.append((prefix, weight))
            return
        letters = _DIGIT_LETTERS.get(digits[depth], '')
        for c in range(first_child, first_child + child_count):
            cchar = nodes[c][0]
            if cchar in letters:
                dfs(c, depth + 1, prefix + cchar)

    if digits:
        dfs(0, 0, "")
    results.sort(key=lambda x: -x[1])
    return results


def verify(blob: bytes, words: list[str]) -> bool:
    nodes = parse_blob(blob)
    print("\n-- Verification --------------------------------------")

    # 1) Every word must be recoverable as an exact match of its own digits.
    miss = 0
    for word in words:
        digits = word_to_digits(word)
        if digits is None:
            continue
        group = blob_exact_matches(nodes, digits)
        if word not in (w for w, _ in group):
            if miss < 10:
                print(f"  MISS  '{word}' ({digits}) not in its own group")
            miss += 1
    print(f"  All-words round-trip: {len(words) - miss} ok, {miss} missing")

    # 2) Inspect a few collision groups.
    print("\n  Collision groups (word: weight):")
    for digits in ("843", "4663", "2665", "43556"):
        group = blob_exact_matches(nodes, digits)[:8]
        pretty = ", ".join(f"{w}:{wt}" for w, wt in group)
        print(f"    {digits} -> {pretty}")

    # 3) Spot-check that the highest-weight match for a sequence is sensible.
    spot = {
        "843":   "the",
        "43556": "hello",
    }
    ok = True
    print("\n  Best-match spot checks:")
    for digits, expected in spot.items():
        group = blob_exact_matches(nodes, digits)
        got = group[0][0] if group else None
        status = "OK  " if got == expected else "WARN"
        if got != expected:
            ok = False
        print(f"    {status} {digits} -> '{got}' (expected '{expected}')")

    if miss:
        ok = False
    print()
    return ok


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> int:
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent

    wordlist_path = script_dir / "8293.txt"
    bin_out_path = repo_root / "src" / "t9_dict.bin"
    hdr_out_path = repo_root / "src" / "t9_dict.h"
    src_out_path = repo_root / "src" / "t9_dict.cpp"

    # Optional overrides: gen_t9_dict.py [wordlist] [hdr_out] [src_out] [bin_out]
    if len(sys.argv) >= 2:
        wordlist_path = Path(sys.argv[1])
    if len(sys.argv) >= 3:
        hdr_out_path = Path(sys.argv[2])
    if len(sys.argv) >= 4:
        src_out_path = Path(sys.argv[3])
    if len(sys.argv) >= 5:
        bin_out_path = Path(sys.argv[4])

    print(f"Reading word list: {wordlist_path}")
    if not wordlist_path.exists():
        print(f"ERROR: word list not found at {wordlist_path}", file=sys.stderr)
        return 1

    words = load_words(str(wordlist_path))
    print(f"  Loaded {len(words)} unique words")
    if not words:
        print("ERROR: no usable words loaded", file=sys.stderr)
        return 1

    print("Building frequency trie...")
    root, _count = build_trie(words)
    compute_max_weight(root)
    order = assign_bfs_layout(root)

    node_count = len(order)
    depth = max(len(w) for w in words)
    print(f"  Nodes: {node_count}  (max word length / trie depth: {depth})")
    if node_count > 0xFFFF:
        print(f"ERROR: node_count {node_count} exceeds uint16 max (65535).",
              file=sys.stderr)
        return 1

    blob = serialise(order)
    print(f"  Binary size: {len(blob)} bytes  ({len(blob) / 1024:.1f} KB)")

    src_dir = repo_root / "src"
    src_dir.mkdir(parents=True, exist_ok=True)

    bin_out_path.write_bytes(blob)
    print(f"  Written: {bin_out_path}")

    emit_header(node_count, len(blob), hdr_out_path)
    print(f"  Written: {hdr_out_path}")

    emit_source(blob, hdr_out_path.name, src_out_path)
    print(f"  Written: {src_out_path}")

    ok = verify(blob, words)
    if ok:
        print("  [OK] All verification checks passed")
    else:
        print("  [WARN] Some checks reported warnings - review the output above.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
