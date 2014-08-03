## Installation 

```
mkdir build; cd build
cmake ..
make; sudo make install
```

## Usage

Start Fcitx and make it stick to the terminal (non-daemon):

```
fcitx --replace -D
```

Use Fcitx's config tool to set BoGo as the default IME and activate it.
Watch the log messages as you type.