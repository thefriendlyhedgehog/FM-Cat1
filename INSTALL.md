# Install Cat Piano

Use the matching source archive and `cat-piano.fwsc` from the release.
The following commands run from the extracted source folder. Python 3 is required.

1. Connect the M-VAVE FM-1 by USB and close applications using its MIDI port.
2. Create an environment and install the USB-MIDI dependencies:

   ```sh
   python3 -m venv .venv
   . .venv/bin/activate
   python -m pip install -r requirements-install.txt
   ```

   On Windows, use `py -m venv .venv`, then `.venv\Scripts\activate`
   in Command Prompt. The remaining `python` commands are the same.

3. Check that the device is detected:

   ```sh
   python tools/fm1_install.py --info
   ```

4. Copy the downloaded firmware into this source folder, then install:

   ```sh
   python tools/fm1_install.py cat-piano.fwsc
   ```

   Read and accept the installer's confirmation. Keep USB connected until the
   update completes and the device restarts. This replaces its current firmware.
   The expected identity after installation is `FM-1_963`.

If more than one device is connected, select the intended MIDI port using
`--port` followed by part of its name. If no device is found, check the USB data
cable and close other MIDI applications. If an update was interrupted and the
FM-1 remains in update mode, run the same installer again to finish it.

The included installer checks the package, device model and post-update identity.
Cat Piano has not been installed on an FM-1 yet. Before the first install, check that you can
get back: the FM-1 Firmware Hub or M-VAVE's updater restores the official firmware, and holding
OCT- while switching on starts the USB rescue.

See [README.md](README.md) for controls. No separate browser installer is needed.
