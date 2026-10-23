"""Resolve legacy asset layouts from registry and neighboring-file evidence."""
from collections import defaultdict
from pathlib import PurePosixPath
import re


def registry(value):
    paths = defaultdict(set)
    for block in re.findall(r'\bchar_start\b(.*?)\bchar_end\b', value, re.I | re.S):
        folder = re.search(r'\bdir\s+"([^"\r\n]+)"', block, re.I)
        name = re.search(r'\bfile\s+"([^"\r\n]+)"', block, re.I)
        if folder and name:
            directory = folder[1].replace('\\', '/').upper()
            if any(p in {'.', '..'} or ':' in p for p in directory.split('/')):
                continue
            paths[name[1].upper()].add('CHARS/' + directory)
    return paths


def resolve(members, direct, registries, texts):
    """Explicit game paths win. Infer only when registry/folder evidence agrees."""
    known = defaultdict(set)
    for value in registries:
        for name, folders in registry(value).items():
            known[name].update(folders)
    for member, target in direct.items():
        if target.startswith('CHARS/') and target.endswith('.TXT') and target.count('/') >= 2:
            known[PurePosixPath(target).stem].add(str(PurePosixPath(target).parent))
    resolved, evidence = dict(direct), {}
    anchors = defaultdict(set)

    def candidates(member):
        path = PurePosixPath(member.replace('\\', '/'))
        stem = path.stem.upper()
        if path.suffix.upper() == '.GHG':
            stem = re.sub(r'_(?:LR_)?PC$', '', stem)
        options = known.get(stem, set())
        if len(options) > 1:
            parents = [p.upper() for p in path.parts[:-1]]
            matched = {p for p in options if PurePosixPath(p).name in parents}
            return matched if matched else options
        return options

    for member in members:
        path = PurePosixPath(member.replace('\\', '/'))
        options = candidates(member)
        if len(options) == 1 and path.suffix.upper() in {'.TXT', '.GHG'}:
            folder = next(iter(options))
            resolved.setdefault(member, folder + '/' + path.name.upper())
            anchors[str(path.parent).upper()].add(folder)
            if member not in direct:
                evidence[member] = 'character registry filename and source folder'
    # A folder containing a real character config and its matching model is
    # self-describing even for new characters absent from the base registry.
    for member, value in texts.items():
        path = PurePosixPath(member.replace('\\', '/'))
        if member in resolved or not re.search(r'^\s*name_id\s*=', value, re.M | re.I):
            continue
        siblings = {PurePosixPath(m.replace('\\', '/')).name.upper()
                    for m in members if str(PurePosixPath(m.replace('\\', '/')).parent).upper() == str(path.parent).upper()}
        if path.parent.name and path.stem.upper() + '_PC.GHG' in siblings:
            anchors[str(path.parent).upper()].add('CHARS/' + path.parent.name.upper())
    for member in members:
        if member in resolved:
            continue
        path = PurePosixPath(member.replace('\\', '/'))
        options = anchors.get(str(path.parent).upper(), set())
        if path.suffix.upper() == '.TXT' and not re.search(
                r'^\s*(?:name_id|variant)\s*=', texts.get(member, ''), re.M | re.I):
            continue
        # Only character-related formats can inherit a character-folder anchor.
        if len(options) == 1 and path.suffix.upper() in {'.TXT', '.GHG', '.AN3', '.BSA', '.DDS', '.PAK'}:
            resolved[member] = next(iter(options)) + '/' + path.name.upper()
            evidence[member] = 'neighboring character config/model folder'
    return resolved, evidence


def variants(resolved, inferred, selections):
    """Turn conflicting sibling option folders into whole-package choices."""
    by_target = defaultdict(list)
    for member, target in resolved.items():
        by_target[target].append(member)
    groups = {}
    for target, sources in by_target.items():
        if len(sources) < 2:
            continue
        if any(m not in inferred for m in sources):
            raise ValueError('Duplicate destination: ' + target)
        parts = [PurePosixPath(m.replace('\\', '/')).parts for m in sources]
        common = 0
        while all(len(p) > common and p[common].casefold() == parts[0][common].casefold() for p in parts):
            common += 1
        if any(len(p) <= common + 1 for p in parts):
            raise ValueError('Ambiguous inferred destination: ' + target)
        root = '/'.join(parts[0][:common])
        options = {p[common] for p in parts}
        if len({o.casefold() for o in options}) != len(sources):
            raise ValueError('Ambiguous inferred option folders: ' + target)
        if root in groups and groups[root] != options:
            raise ValueError('Inconsistent variant folders under ' + root)
        groups[root] = options
    choices, excluded = [], set()
    if len(groups) > 32:
        raise ValueError('Too many independent variant groups (maximum 32)')
    for index, (root, options) in enumerate(sorted(groups.items())):
        options = sorted(options, key=str.casefold)
        if len(options) > 128:
            raise ValueError('Too many variant options (maximum 128): ' + root)
        key = 'Variant' + str(index)
        selected = max(0, min(len(options) - 1, selections(key)))
        choices.append(dict(key=key, label=root or 'Model variant', options=options, selected=selected))
        prefix = root + '/' if root else ''
        for member in resolved:
            normalized = member.replace('\\', '/')
            if normalized.casefold().startswith(prefix.casefold()):
                rest = normalized[len(prefix):].split('/')
                if len(rest) > 1 and rest[0] in options and rest[0] != options[selected]:
                    excluded.add(member)
    return {m: t for m, t in resolved.items() if m not in excluded}, choices, sorted(excluded)
