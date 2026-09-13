#!/usr/bin/env python3
"""Package the pinned FlyBrain graph as group-sorted CSR for Ndless."""

import argparse
from array import array
import gzip
import hashlib
import json
from pathlib import Path
import struct
import sys
import urllib.request

REVISION = "9191824d17871b7851645782d53d23f213ddb938"
SOURCE_SHA256 = "fbf8d440ca1207c7573e1acdd2366f9d0beb9b533c1710f21681264f81b1cc49"
SOURCE_URL = f"https://raw.githubusercontent.com/snedea/flybrain/{REVISION}/data/connectome.bin.gz"
NEURONS, EDGES, GROUPS = 139255, 2698236, 63


def source_bytes(path):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        print(f"Downloading {SOURCE_URL}", flush=True)
        with urllib.request.urlopen(SOURCE_URL, timeout=120) as response:
            compressed = response.read()
        if hashlib.sha256(compressed).hexdigest() != SOURCE_SHA256:
            raise ValueError("Downloaded connectome checksum differs from pinned upstream")
        path.write_bytes(compressed)
    compressed = path.read_bytes()
    if hashlib.sha256(compressed).hexdigest() != SOURCE_SHA256:
        raise ValueError(f"{path}: checksum differs from pinned upstream")
    return gzip.decompress(compressed)


def little_endian_bytes(values):
    if sys.byteorder != "little":
        values = values[:]
        values.byteswap()
    return values.tobytes()


def pack(source, destination):
    raw = source_bytes(source)
    neurons, edges = struct.unpack_from("<II", raw)
    if (neurons, edges) != (NEURONS, EDGES):
        raise ValueError("Unexpected full-connectome dimensions")
    meta_start = 8 + edges * 12
    if len(raw) != meta_start + neurons * 3:
        raise ValueError("Unexpected upstream binary length")
    offsets = array("I", [0]) * (GROUPS + 1)
    neuron_groups = bytearray(neurons)
    for i, (region, group) in enumerate(struct.iter_unpack("<BH", memoryview(raw)[meta_start:])):
        if group >= GROUPS or region >= 4:
            raise ValueError("Invalid upstream neuron metadata")
        neuron_groups[i] = group
        offsets[group + 1] += 1
    for g in range(GROUPS):
        offsets[g + 1] += offsets[g]
    cursor = offsets[:-1]
    remap = array("I", [0]) * neurons
    for original, group in enumerate(neuron_groups):
        remap[original] = cursor[group]
        cursor[group] += 1

    edge_data = memoryview(raw)[8:meta_start]
    rows = array("I", [0]) * (neurons + 1)
    max_weight = 0.0
    for pre, post, weight in struct.iter_unpack("<IIf", edge_data):
        if pre >= neurons or post >= neurons:
            raise ValueError("Invalid upstream edge endpoint")
        rows[remap[pre] + 1] += 1
        max_weight = max(max_weight, abs(weight))
    if max_weight == 0:
        raise ValueError("Connectome has no weighted synapses")
    for i in range(neurons):
        rows[i + 1] += rows[i]
    cursor = rows[:-1]
    targets = array("I", [0]) * edges
    weights = array("f", [0]) * edges
    if targets.itemsize != 4 or weights.itemsize != 4:
        raise RuntimeError("Packaging requires 32-bit unsigned int and IEEE float arrays")
    for pre, post, weight in struct.iter_unpack("<IIf", edge_data):
        source_index = remap[pre]
        at = cursor[source_index]
        targets[at] = remap[post]
        weights[at] = (weight / max_weight) * 0.15
        cursor[source_index] += 1

    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0, compresslevel=9) as stream:
            stream.write(struct.pack("<8sIII", b"NFLYCSR1", neurons, edges, GROUPS))
            for values in (offsets, rows, targets, weights):
                stream.write(little_endian_bytes(values))
    report = {
        "upstream": f"https://github.com/snedea/flybrain/tree/{REVISION}",
        "dataset": "FlyWire FAFB v783",
        "citation": "Dorkenwald et al., Nature 634, 124-138 (2024)",
        "paper": "https://doi.org/10.1038/s41586-024-07558-y",
        "code_license": "GPL-3.0-only; see LICENSE and THIRD_PARTY_NOTICES for the upstream MIT notice",
        "format": {
            "compression": "gzip",
            "endianness": "little",
            "header": "8-byte NFLYCSR1; uint32 neurons, edges, groups",
            "arrays": [
                "uint32 group_offsets[groups+1]",
                "uint32 csr_rows[neurons+1]",
                "uint32 targets[edges]",
                "float32 weights[edges]",
            ],
            "neuron_order": "stable sort of canonical upstream indices by group",
        },
        "simulation": {
            "neural_step_ms": 100,
            "behavior_step_ms": 500,
            "leak": 0.95,
            "threshold": 1.0,
            "refractory_ticks": 3,
            "group_cooldown_ticks": 20,
            "voltage_storage": "float32; leak multiplication uses binary64 as in JavaScript",
            "motor_layer": "upstream virtual VNC driven by descending neural activity and drives",
        },
        "source_sha256": SOURCE_SHA256,
        "output_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "neurons": neurons,
        "edges": edges,
        "groups": GROUPS,
        "group_sizes": [offsets[g + 1] - offsets[g] for g in range(GROUPS)],
        "max_absolute_source_weight": max_weight,
        "packed_bytes": destination.stat().st_size,
        "uncompressed_bytes": 20 + 4 * (GROUPS + 1 + neurons + 1) + edges * 8,
        "neural_array_ram_bytes": 4 * (neurons + 1) + 8 * edges + 7 * neurons,
        "weight_rule": "float32((source_float32 / max_absolute_source_weight) * 0.15)",
    }
    destination.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(".cache/connectome.bin.gz"))
    parser.add_argument("--output", type=Path, default=Path("dist/connectome.tns"))
    args = parser.parse_args()
    pack(args.source, args.output)


if __name__ == "__main__":
    main()
