#!/usr/bin/env python3
"""Redacted, dependency-free triage of tracked worktree and reachable Git blobs.

Not a replacement for Gitleaks or human review. Never prints matching values.
"""
import argparse
import collections
import ipaddress
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RULES = {
    'private-key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----'),
    'provider-token': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}|AKIA[A-Z0-9]{16}|sk-[A-Za-z0-9_-]{24,})\b'),
    'credential-assignment': re.compile(rb'(?i)(?:password|passwd|api[_-]?key|auth[_-]?token|access[_-]?token|wifi[_-]?(?:pass|ssid))\s*[=:]\s*[\"\'][^\"\'\r\n]{8,}[\"\']'),
    'local-host': re.compile(rb'(?i)(?:https?://[^\s/\"\'<>]+\.(?:lan|local)\b|/home/(?!USER\b|user\b|example\b)[A-Za-z0-9_-]+/)'),
    'hardware-identity': re.compile(rb'(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b'),
    'personal-email': re.compile(rb'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b'),
    'family-content': re.compile(rb'(?i)\b(?:school\x20pickup|family\x20schedule|bedtime|child(?:ren)?\x27?s|soccer practice|dentist appointment)\b'),
}
IP = re.compile(rb'(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])')

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

def scan(data):
    if b'\x00' in data[:8192]:
        return []
    found = []
    for number, line in enumerate(data.splitlines(), 1):
        for name, pattern in RULES.items():
            if name == 'family-content' and b"'family-content': re.compile" in line:
                continue
            if pattern.search(line):
                found.append((number, name))
        for match in IP.finditer(line):
            try:
                ip = ipaddress.ip_address(match.group().decode())
                if ip.is_private and not ip.is_loopback and not ip.is_unspecified:
                    found.append((number, 'private-network-address'))
                    break
            except ValueError:
                pass
    return found

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--history', action='store_true', help='also scan unique reachable blobs and commit metadata')
    args = parser.parse_args()
    findings, counts = [], collections.Counter()
    paths = git('ls-files', '-z').decode().split('\x00')
    for name in filter(None, paths):
        path = ROOT / name
        if path.is_file():
            counts['worktree_files'] += 1
            for line, rule in scan(path.read_bytes()):
                findings.append(dict(scope='tracked-worktree', path=name, line=line, rule=rule))
    if args.history:
        for record in git('rev-list', '--objects', '--all').decode().splitlines():
            oid, _, name = record.partition(' ')
            if git('cat-file', '-t', oid).strip() != b'blob':
                continue
            counts['history_blobs'] += 1
            for line, rule in scan(git('cat-file', 'blob', oid)):
                findings.append(dict(scope='history', path=name, object=oid[:12], line=line, rule=rule))
        for oid in git('rev-list', '--all').decode().splitlines():
            counts['commits'] += 1
            for line, rule in scan(git('cat-file', 'commit', oid)):
                findings.append(dict(scope='commit-metadata', object=oid[:12], line=line, rule=rule))
    counts['findings'] = len(findings)
    print(json.dumps(dict(tool='scoped-heuristic-triage', counts=counts, findings=findings), indent=2))
    return bool(findings)

if __name__ == '__main__':
    raise SystemExit(main())
