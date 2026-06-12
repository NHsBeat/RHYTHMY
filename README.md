# RHYTHMY

[![CI](https://github.com/NHsBeat/RHYTHMY/actions/workflows/ci.yml/badge.svg)](https://github.com/NHsBeat/RHYTHMY/actions/workflows/ci.yml)
[![Release](https://github.com/NHsBeat/RHYTHMY/actions/workflows/release.yml/badge.svg)](https://github.com/NHsBeat/RHYTHMY/actions/workflows/release.yml)

A portable DAW (Digital Audio Workstation) designed for handheld gaming devices like the R36S, built with C++17, SDL2, and miniaudio.

**Download:** grab the latest installers from the [Releases page](https://github.com/NHsBeat/RHYTHMY/releases).

## Features

- Step sequencer with drum and melodic tracks
- Real-time audio synthesis and mixing
- Gamepad-first UI (designed for Xbox/generic controllers)
- Themeable interface
- Runs on ARM Linux (R36S) and desktop Linux

## Supported Devices

- R36S (primary target, ARM Linux)
- Desktop Linux (for development/testing)

## Building

### Linux / WSL

```bash
mkdir build && cd build
cmake ..
make
```

### Cross-compile for R36S (aarch64)

```bash
./build_aarch64.sh
```

See the install scripts (`Install_RHYTHMY_SD1.sh`, `Install_RHYTHMY_SD2.sh`) for SD card setup.

## Controls

Navigate using a standard gamepad (Xbox layout):
- **D-Pad** — move between cells / navigate menus
- **A** — confirm / play
- **B** — back / cancel
- **Start** — main menu

## License

This project is released under the **RHYTHMY Source-Available License**.
See [LICENSE](LICENSE) for full terms.

Key points:
- Free to use, study, and fork
- Forks must be distributed **free of charge** — no selling allowed
- Forks must display a **donation link** to the original author
- No commercial use

## Support the Author

If you enjoy RHYTHMY or use it in your projects, please consider donating:

> **[Donate to the author](https://donate.stream/woozyfromjapan)**
> https://donate.stream/woozyfromjapan

**If you fork this project, you are required by the license to include this donation link visibly in your repository.**

## Contact

haznekhay@gmail.com
