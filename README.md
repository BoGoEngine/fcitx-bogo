## Installation 

```
mkdir build; cd build
cmake ..
make; sudo make install
```

For now, make sure you have [bogo-python][1] (it will be distributed later with fcitx-bogo):

```
sudo pip install bogo
```

[1]: https://github.com/BoGoEngine/bogo-python

## Usage

Start Fcitx and make it stick to the terminal (non-daemon):

```
fcitx --replace -D
```

Use Fcitx's config tool to set BoGo as the default IME and activate it
(the default hotkey should be <kbd>Ctrl</kbd>+<kbd>Space</kbd>).