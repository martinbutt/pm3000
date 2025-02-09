# PM3000 - Premier Manager 3 Game Addon

**On Debian/Ubuntu based distributions, use the following command:**

```sh
sudo apt install git build-essential pkg-config cmake cmake-data libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

## Build instructions

```sh
# Clone this repo
git clone https://gitlab.com/martinbutt/pm3000.git
cd pm3000

# Create a build folder
mkdir build
cd build

# Build
cmake ..
make

# Run
./pm3000
```

## Acknowledgements
Special thanks to @eb4x for the https://github.com/eb4x/pm3 project. PM3000 would not exist without it.

Thanks to https://github.com/aminosbh/sdl2-image-sample for the SDL2 scaffold to start the project.

### Uses
https://github.com/btzy/nativefiledialog-extended
https://github.com/viznut/unscii
https://www.1001fonts.com/acknowledge-tt-brk-font.html