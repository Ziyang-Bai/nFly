# nFly

[简体中文](README.md) | English

[![Build and Release](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml/badge.svg)](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml)

A fruit fly brain neural network simulator running on TI-Nspire CX

A native Ndless fruit fly neural network simulator for TI-Nspire CX / CX II, ported from [FlyBrain](https://github.com/snedea/flybrain).

On the calculator, neural updates use a 100 ms timestep and behavior updates use a 500 ms timestep, both in simulated time. If computation takes longer than the timestep, the simulation advances more slowly in real time.

## Requirements

- A color-screen TI-Nspire CX / CX II with at least 64 MB of RAM.
- Classic monochrome models and 32 MB models (including CM) are not supported.
- The program and connectome data must be in the same folder on the calculator. If memory is insufficient, restart the calculator before opening the program again.

## Download and installation

Download `nFly.zip` from [GitHub Releases](https://github.com/Ziyang-Bai/nFly/releases). Unreleased builds are available as artifacts from successful runs in [Actions](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml).

The archive contains:

- `nFly.tns`: the program.
- `connectome.tns`: the connectome data read by the program.
- `connectome.json`: source, format, simulation parameters, and checksums.
- `LICENSE`: the GNU General Public License version 3.
- `THIRD_PARTY_NOTICES`: the original upstream MIT copyright and license notice.

After extracting the archive, transfer the files in this order:

1. Create a folder on the calculator, such as `nFly`.
2. Transfer `connectome.tns` first, then `nFly.tns`, keeping both in the same folder.
3. Open **`nFly.tns`** from “My Documents” and wait for the data to load. Do not open `connectome.tns`.

You can keep `connectome.json`, `LICENSE`, and `THIRD_PARTY_NOTICES` on your computer for reference; they do not need to be transferred to the calculator.

## Controls

| Key | Function |
| --- | --- |
| Arrow keys / touchpad directions | Move the scene cursor; select a neuron group in the neuron group view |
| Enter | Place food at the cursor |
| F, then Enter | Move the cursor to the fly and place food there |
| 1 / 2 / 3 / 4 | Touch the head / thorax / abdomen / legs |
| A / Shift+A | Gentle / strong wind; direction is determined by the cursor's position relative to the fly |
| L | Cycle lighting |
| T | Switch temperature |
| D | Toggle danger odor |
| C | Clear food |
| P | Pause / resume |
| B | Switch between the scene and neuron group views |
| H | Show / hide help |
| R | Reset the simulation |
| Esc | Exit and return to the system |

## Building

Set up GNU Make 4.3+, Python 3, and the Ndless SDK in a Linux environment.

Run from the repository root:

```sh
make check
make all release
```

Build outputs:

```text
dist/nFly.tns
dist/connectome.tns
dist/connectome.json
dist/LICENSE
dist/THIRD_PARTY_NOTICES
dist/nFly.zip
```

To specify the input:

```sh
make all release SOURCE=/path/to/connectome.bin.gz
```

## Generating connectome.tns

Use [`tools/pack_connectome.py`](tools/pack_connectome.py).

### Download and convert

```sh
python3 tools/pack_connectome.py
```

### Use offline input

Run:

```sh
python3 tools/pack_connectome.py \
  --source /path/to/connectome.bin.gz \
  --output dist/connectome.tns
```

### Verification

- Upstream repository: [snedea/flybrain](https://github.com/snedea/flybrain).
- Source revision: [`9191824d17871b7851645782d53d23f213ddb938`](https://github.com/snedea/flybrain/tree/9191824d17871b7851645782d53d23f213ddb938).
- Original file: [data/connectome.bin.gz](https://raw.githubusercontent.com/snedea/flybrain/9191824d17871b7851645782d53d23f213ddb938/data/connectome.bin.gz).
- SHA-256:

```text
fbf8d440ca1207c7573e1acdd2366f9d0beb9b533c1710f21681264f81b1cc49
```

## Sources and license

nFly code is licensed under the [GNU General Public License version 3](LICENSE) (`GPL-3.0-only`). The original FlyBrain MIT copyright and license notice is preserved in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES).

The connectome comes from **FlyWire FAFB v783**, obtained through the FlyBrain data file. For data usage and citation requirements, see [FlyWire](https://flywire.ai/) and the following paper:

Dorkenwald et al., *Neuronal wiring diagram of an adult brain*, Nature **634**, 124–138 (2024). [doi:10.1038/s41586-024-07558-y](https://doi.org/10.1038/s41586-024-07558-y)
