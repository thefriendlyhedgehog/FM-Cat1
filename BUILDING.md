# Build Cat Piano

Prebuilt firmware users can follow [INSTALL.md](INSTALL.md) without compiling.
These source-build steps target macOS or Linux with a C compiler, Git and Python 3.
Docker is required unless using native Linux x86-64. Docker must be running. On Apple silicon it needs
`linux/amd64` emulation, for example colima: `colima start --arch aarch64 --vm-type vz --vz-rosetta --mount "$PWD:w"`.
Authoring new sample banks also needs numpy, ffmpeg and the Rubber Band CLI (see the README); building does not.

## Dependencies

Run from this repository's root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-build.txt
sh tools/get_toolchain.sh "$PWD/tooling/jieli"
git clone --branch AC79NN_SDK_V1.2.1_2023-12-13 --depth 1 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK tooling/ac79-sdk
export JIELI_TOOLCHAIN="$PWD/tooling/jieli/toolchain"
export AC79_SDK="$PWD/tooling/ac79-sdk"
export PYTHON="$PWD/.venv/bin/python"
export X0X_FONTSET=terminus
export JIELI_DOCKER_IMAGE='debian:bookworm-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587'
```

The toolchain downloader uses a mutable official endpoint. The tested version
was `jieli-linux-toolchains-20260730.1`; see [docs/build-versions.json](docs/build-versions.json)
for the compiler checksum and SDK commit. A newer download may produce different
results. The build checks the three packaged SDK file hashes against its reference.

## Build and check

```sh
python tools/import_samples.py --banks samples/rendered/banks.json   # only after changing samples/rendered/
sh tests/run_tests.sh
./build.sh
```

Output: `build/cat-piano.fwsc` (installable package), `build/omni.bin`
(application), and `build/loader/ota.bin` (update loader). Builds and tests do not
contact the FM-1. The package identity is `FM-1_963`.
The build checks entry placement, RAM/app capacity and low-level code boundaries.
The test suite covers sample import, playback, effects, panel/MIDI controls,
sequencer timing, overdubbing, stop/clear, encoders, USB audio, TRS MIDI and storage.
When a firmware package exists, it also checks OTA handling against that package.
