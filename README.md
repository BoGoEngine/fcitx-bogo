## Installation 

**Arch Linux**

```
yaourt fcitx-bogo-git
```

**Others**

```
mkdir build; cd build
cmake ..
make; sudo make install
```

[1]: https://github.com/BoGoEngine/bogo-python

## Usage

Start Fcitx and make it stick to the terminal (non-daemon):

```
fcitx --replace -D
```

Use Fcitx's config tool to set BoGo as the default IME and activate it
(the default hotkey should be <kbd>Ctrl</kbd>+<kbd>Space</kbd>).

![Setup fcitx-bogo](/data/tut.png)

## Known issues

- Can't type with Skype and Wine apps on x86_64 (need testing on x86).

## Legal stuff

    Copyright © 2014 Trung Ngo et al.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
