## Installation 

**Arch Linux**

Please read this beforehand: https://wiki.archlinux.org/index.php/fcitx, then install fcitx-bogo from AUR.

```bash
yaourt fcitx-bogo-git
```

**Debian/Ubuntu/Mint**

Build requirements

```bash
# CMake
# Python 2.7 (or higher) development headers
# Fcitx development headers

sudo apt-get install cmake fcitx-libs-dev python2.7-dev build-essential
```

Build and install

```bash
mkdir build; cd build
cmake -DPYTHON_EXECUTABLE=$(which python2.7) ..
make; sudo make install
```

Runtime requirements

```bash
# The --install-recommends flag is IMPORTANT on Mint
sudo apt-get install --install-recommends fcitx
```

Run `im-config` and choose **fcitx** as the active input method. Log out, log in.

## Usage

Use Fcitx's config tool to set BoGo as the default IME and activate it.

> The default hotkey should be <kbd>Ctrl</kbd>+<kbd>Space</kbd>.

![Setup fcitx-bogo](/data/tut_gtk.png)

For the KDE config tool:

![Setup fcitx-bogo](/data/tut.png)

## Known issues

- Can't type with Wine apps.
- Crashes LibreOffice if the **libreoffice-kde** plugin is installed.

## Want to help?

See https://github.com/BoGoEngine/fcitx-bogo/wiki/For-Developers

## Legal stuff

    Copyright © 2014 Trung Ngo et al.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
