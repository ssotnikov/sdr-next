#!/usr/bin/env python3
"""Collect notices for a staged package from its Qt 6.10 SPDX inventory."""

import argparse
from collections import defaultdict
import csv
import hashlib
import json
from pathlib import Path
import re
import struct


TIKA_NOTICE_SHA256 = "4cd09bac8d81bd34decf6b95fb23d42fc9b3d6b653ec4be8ef44674b692c97cd"


def field(block, name):
    return re.search(rf"^{name}: (.+)$", block, re.MULTILINE)[1]


def collect_linux_packages(stage, qt_version):
    documents = {}
    namespaces = {}
    for path in sorted((stage / "licenses/qt-sbom").glob("*.spdx")):
        content = path.read_bytes()
        document = content.decode("utf-8")
        name = field(document, "DocumentName")
        if not name.endswith(f"-{qt_version}"):
            raise ValueError(f"Unexpected Qt SPDX version: {name}")
        documents[name] = document
        namespaces[field(document, "DocumentNamespace")] = (
            name, hashlib.sha1(content).hexdigest()
        )

    packages = {}
    owners = defaultdict(set)
    dependencies = defaultdict(set)
    for name, document in documents.items():
        references = {}
        for alias, namespace, digest in re.findall(
            r"^ExternalDocumentRef: (\S+) (\S+) SHA1: (\w+)$", document, re.MULTILINE
        ):
            target, actual_digest = namespaces[namespace]
            if actual_digest != digest:
                raise ValueError(f"Qt SPDX document checksum mismatch: {namespace}")
            references[alias] = target

        files = {
            field(block, "SPDXID"): path.removeprefix("./")
            for path, block in re.findall(
                r"^FileName: ([^\n]+)\n(SPDXID: [^\n]+)", document, re.MULTILINE
            )
        }
        for block in re.split(r"^PackageName: ", document, flags=re.MULTILINE)[1:]:
            packages[name, field(block, "SPDXID")] = block

        for source, relation, target in re.findall(
            r"^Relationship: (\S+) (CONTAINS|DEPENDS_ON) (\S+)$", document, re.MULTILINE
        ):
            if relation == "CONTAINS" and target in files:
                owners[files[target]].add((name, source))
            elif relation == "DEPENDS_ON":
                alias, separator, target_id = target.partition(":")
                dependency = (references[alias], target_id) if separator else (name, target)
                dependencies[name, source].add(dependency)

    deployed = set()
    for directory in ("lib", "plugins", "qml"):
        for path in (stage / directory).rglob("*"):
            if not path.is_file() or path.is_symlink():
                continue
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            relative = path.relative_to(stage).as_posix()
            if len(owners[relative]) != 1:
                raise ValueError(f"Expected one Qt SPDX owner for {relative}")
            deployed.update(owners[relative])

    if not deployed:
        raise ValueError("No deployed Qt ELF files found")

    pending = list(deployed)
    reachable = set()
    while pending:
        package = pending.pop()
        if package not in reachable:
            reachable.add(package)
            pending.extend(dependencies[package])

    return len(deployed), [
        (key[1], packages[key], field(packages[key], "PackageLicenseConcluded"))
        for key in sorted(reachable)
        if "-qt-3rdparty-sources-" in key[1]
    ]


def pe_sha1(data):
    """Compute the pre-signing SHA-1 used by the Windows Qt SDK."""
    header = struct.unpack_from("<I", data, 0x3C)[0]
    if data[:2] != b"MZ" or data[header:header + 4] != b"PE\0\0":
        raise ValueError("Expected a PE binary")
    optional = header + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    certificates = optional + {0x10B: 96, 0x20B: 112}[magic] + 8 * 4
    offset, size = struct.unpack_from("<II", data, certificates)
    if not size or offset < certificates + 8 or offset + size != len(data):
        raise ValueError("Expected a trailing PE certificate table")
    unsigned = bytearray(data[:offset])
    unsigned[certificates:certificates + 8] = bytes(8)
    unsigned[optional + 64:optional + 68] = bytes(4)
    return hashlib.sha1(unsigned).hexdigest()


def collect_windows_packages(stage, qt_version):
    sbom = stage / "licenses/qt-sbom"
    documents = {}
    namespaces = {}
    for path in sorted(sbom.glob("*.spdx.json")):
        doc = json.loads(path.read_bytes())
        name = doc["name"]
        if not name.endswith(f"-{qt_version}"):
            raise ValueError(f"Unexpected Qt SPDX version: {name}")
        documents[name] = doc
        namespaces[doc["documentNamespace"]] = name

    packages = {}
    deployed = {}
    dependencies = defaultdict(list)
    references = {}
    for name, doc in documents.items():
        packages.update({(name, p["SPDXID"]): p for p in doc["packages"]})
        references[name] = {
            ref["externalDocumentId"]: ref for ref in doc.get("externalDocumentRefs", [])
        }
        binaries = {}
        for item in doc.get("files", []):
            spdx_id = item["SPDXID"]
            if not spdx_id.endswith("-RelWithDebInfo"):
                continue
            path = item["fileName"].removeprefix("./")
            if path.lower().endswith(".dll"):
                binaries[spdx_id] = (
                    path.removeprefix("bin/").removeprefix("plugins/"),
                    next(c["checksumValue"] for c in item["checksums"] if c["algorithm"] == "SHA1"),
                )
        for relation in doc["relationships"]:
            key = name, relation["spdxElementId"]
            target = relation["relatedSpdxElement"]
            if relation["relationshipType"] == "DEPENDS_ON":
                dependencies[key].append(target)
            if relation["relationshipType"] == "CONTAINS" and target in binaries:
                path, digest = binaries[target]
                if (stage / path).is_file():
                    value = key, digest
                    if deployed.setdefault(path, value) != value:
                        raise ValueError(f"Multiple Qt SPDX owners for {path}")

    if not deployed:
        raise ValueError("No deployed Qt DLLs found")
    for path, (_, digest) in deployed.items():
        data = (stage / path).read_bytes()
        if hashlib.sha1(data).hexdigest() != digest and pe_sha1(data) != digest:
            raise ValueError(f"Qt DLL checksum differs from SPDX: {path}")

    pending = [key for key, _ in deployed.values()]
    reachable = set()
    verified_refs = set()
    while pending:
        key = pending.pop()
        if key in reachable:
            continue
        if key not in packages:
            raise ValueError(f"Missing Qt SPDX package: {key}")
        reachable.add(key)
        name, _ = key
        for target in dependencies[key]:
            alias, separator, target_id = target.partition(":")
            if not separator:
                pending.append((name, target))
                continue
            reference = references[name][alias]
            target_doc = namespaces[reference["spdxDocument"]]
            if (name, alias) not in verified_refs:
                digest = hashlib.sha1((sbom / f"{target_doc}.spdx").read_bytes()).hexdigest()
                if reference["checksum"] != {"algorithm": "SHA1", "checksumValue": digest}:
                    raise ValueError(f"Qt SPDX document checksum mismatch: {target_doc}")
                verified_refs.add((name, alias))
            pending.append((target_doc, target_id))

    return len(deployed), [
        (key[1], packages[key]["comment"] + "\n" + "\n".join(
            ref["referenceLocator"] for ref in packages[key].get("externalRefs", [])
        ), packages[key]["licenseConcluded"])
        for key in sorted(reachable)
        if "-qt-3rdparty-sources-" in key[1] or "-qt-bundled-3rdparty-module-" in key[1]
    ]


def collect_notices(source, lock, packages, tika_notice, windows):
    notices = {}
    rows = []
    for spdx_id, block, license_id in packages:
        path, index, attribution_id = re.search(
            r"Information extracted from:\s+/src_dir/([^\n]+)\n"
            r"\s+Entry index: (\d+)\n\s+Id: ([^\n]+)", block
        ).groups()
        attribution = (source / path).resolve()
        module = attribution.relative_to(source).parts[0]
        revision = lock["modules"][module]
        reference = re.search(rf"pkg:github/qt/{re.escape(module)}@([a-f0-9]+)\?", block)
        if reference:
            document_revision = reference[1]
        elif windows and module == "qtmultimedia":
            # This Windows SDK document has no revision in its Qt Multimedia PURL.
            document_revision = (source / module / ".tag").read_text().strip()
            if document_revision != revision:
                raise ValueError("Qt Multimedia source archive tag does not match the lock")
        else:
            raise ValueError(f"Missing Qt SPDX revision: {spdx_id}")
        if not revision.startswith(document_revision):
            raise ValueError(f"Qt SPDX revision does not match the lock: {spdx_id}")
        # The pinned upstream metadata contains literal tabs in some strings.
        data = json.loads(attribution.read_text(encoding="utf-8-sig"), strict=False)
        entry = data[int(index)] if isinstance(data, list) else data
        if entry["Id"] != attribution_id or entry["LicenseId"] != license_id:
            raise ValueError(f"Qt attribution differs from SPDX: {spdx_id}")

        references = []
        for key in ("LicenseFile", "LicenseFiles", "CopyrightFile"):
            values = entry.get(key, [])
            for value in [values] if isinstance(values, str) else values:
                references.append((attribution.parent / value).resolve())
        for identifier in re.findall(r"[^ ()]+", license_id):
            if identifier in {"AND", "OR", "WITH"}:
                continue
            for root in (source / module, source):
                generic = root / "LICENSES" / f"{identifier}.txt"
                if generic.is_file():
                    references.append(generic)
                    break
            else:
                if not references:
                    raise ValueError(f"No license text for {spdx_id} ({identifier})")

        included = []
        for path in references:
            relative = path.relative_to(source).as_posix()
            notices[relative] = path.read_bytes()
            included.append(relative)
        if attribution_id == "tlfs":
            header = attribution.with_name("tlsf.h").read_bytes()
            relative = "qtmultimedia/src/3rdparty/tlsf/NOTICE.txt"
            notices[relative] = re.search(rb"/\*.*?\*/", header, re.DOTALL)[0] + b"\n"
            included.append(relative)
        if attribution_id == "tika-mimetypes":
            relative = f"apache-tika/{entry['Version']}/NOTICE.txt"
            content = tika_notice.read_bytes()
            if hashlib.sha256(content).hexdigest() != TIKA_NOTICE_SHA256:
                raise ValueError("Apache Tika NOTICE checksum mismatch")
            notices[relative] = content
            included.append(relative)

        copyright_text = entry.get("Copyright", "")
        if isinstance(copyright_text, list):
            copyright_text = "\n".join(copyright_text)
        rows.append((
            spdx_id, entry["Name"], entry.get("Version", ""), license_id,
            f"{attribution.relative_to(source).as_posix()}#{index}",
            " ".join(copyright_text.split()), "; ".join(sorted(set(included))),
        ))
    return notices, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", type=Path, help="Package staging directory")
    parser.add_argument("qt_source", type=Path, help="matching pinned Qt source directory")
    parser.add_argument("tika_notice", type=Path, help="NOTICE.txt from the pinned Apache Tika revision")
    parser.add_argument("--windows", action="store_true", help="Use the Windows Qt SDK binary SPDX")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    lock = json.loads((repo / "dependencies.lock.json").read_text(encoding="utf-8"))["qt"]
    collect_packages = collect_windows_packages if args.windows else collect_linux_packages
    deployed, packages = collect_packages(args.stage, lock["version"])
    notices, rows = collect_notices(args.qt_source.resolve(), lock, packages, args.tika_notice, args.windows)

    output = args.stage / "licenses/qt-attributions"
    output.mkdir()
    for name, content in sorted(notices.items()):
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
    with (output / "index.tsv").open("w", encoding="utf-8", newline="") as index:
        writer = csv.writer(index, delimiter="\t", lineterminator="\n")
        writer.writerow(("SPDX ID", "Component", "Version", "License", "Qt source", "Copyright", "Texts"))
        writer.writerows(rows)
    print(f"{deployed} deployed Qt SPDX matches, {len(rows)} attribution records, {len(notices)} texts")


if __name__ == "__main__":
    main()
