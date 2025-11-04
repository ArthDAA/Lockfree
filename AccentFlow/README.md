# AccentFlow

AccentFlow is a low-level input daemon for Debian that enables fast cycling between accented characters using the right `Alt` key. The daemon intercepts keyboard events, manages accent variants, and injects the chosen Unicode code point through a virtual keyboard created with `/dev/uinput`.

## Features

- **Right Alt trigger** – hold `Alt_R` to enter AccentFlow mode.
- **Cycle variants** – press the base character repeatedly to cycle through accent options defined in `config.json`.
- **Zero-copy injection** – the final variant is typed into the focused application via the Linux Unicode input sequence (`Ctrl` + `Shift` + `u`).
- **Terminal preview** – a lightweight TUI preview displays the available variants and highlights the selection.
- **Pluggable configuration** – JSON mapping of base characters to accent variants at `/etc/accentflow/config.json`.

## Project layout

```
accentflow/
├── Makefile
├── README.md
├── accentflow.service
├── config/
│   └── config.json
├── include/
│   ├── accentflow.h
│   ├── config.h
│   ├── display.h
│   ├── input_engine.h
│   ├── mapper.h
│   └── utils.h
└── src/
    ├── accentflow.c
    ├── config_loader.c
    ├── display_tui.c
    ├── input_engine.c
    ├── mapper.c
    └── utils.c
```

## Building

```bash
cd accentflow
make
```

This produces the `accentflowd` binary in the project root.

## Installation

```bash
sudo make install PREFIX=/opt/accentflow
sudo install -m 0644 accentflow.service /etc/systemd/system/accentflow.service
sudo useradd --system --no-create-home accentflow || true
sudo systemctl daemon-reload
sudo systemctl enable --now accentflow
```

By default the daemon loads `/etc/accentflow/config.json`. Adjust the `input_device` field to the keyboard device you wish to intercept (check with `sudo libinput list-devices` or `sudo evtest`).

## Configuration

A minimal configuration file looks like:

```json
{
  "input_device": "/dev/input/event0",
  "e": ["é", "è", "ê", "ë"],
  "a": ["à", "â", "ä"],
  "u": ["ù", "û", "ü"]
}
```

You can extend the JSON with any base character. Each value must be an array of UTF-8 strings. The first entry becomes the default variant when you tap the base character while holding `Alt_R`.

Reload the daemon after editing the configuration:

```bash
sudo systemctl restart accentflow
```

## Running manually

```bash
sudo ./accentflowd --config ./config/config.json --device /dev/input/event3
```

Use `--no-grab` if you want to keep the physical keyboard visible to the system (useful for debugging or when running inside a VM). Without `--no-grab`, the daemon acquires exclusive access via `EVIOCGRAB`.

## Testing checklist

1. Verify the daemon can read events using `sudo evtest /dev/input/eventX`.
2. Start AccentFlow and open a text editor.
3. Hold `Alt_R`, press `e` repeatedly, and confirm the preview cycles through `é`, `è`, `ê`, `ë`.
4. Release `Alt_R` and check that the chosen character appears in the editor.
5. Repeat for every mapping defined in `config.json`.
6. Inspect the logs (stdout/stderr or journal) for errors.

## Limitations & notes

- The built-in preview uses standard error output; when run under `systemd`, consult the journal (`journalctl -u accentflow`).
- Unicode injection relies on the Linux `Ctrl`+`Shift`+`u` input method, which must be supported by the desktop environment.
- The configuration parser does not yet support `\uXXXX` escapes.
- GUI overlays and hot reload are planned extensions.

## License

Released under the MIT License.
