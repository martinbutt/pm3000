# PM3000 - Premier Manager 3 Game Add-on

PM3000 is an add-on for Premier Manager 3, which allows you to load your save games to use additional functionality.

The new features are intended to overcome some of the game's annoyances.
- Change Team - This screen allows you to switch to a new team, unlocking the ability to start the game as your favorite team.
- View Squad - This screen shows your current team for ease of visibility
- Scout - A scout that can see the stats of any player
- Free Players - This screen shows all of the out-of-contract players
- Convert Player to Coach - This screen allows you to retire a player and convert them into a coach
- Telephone - This adds a few new features:
  - Advertise for fans - Run an ad campaign and increase your fan base, leading to more attendance
  - Entertain team - Take the team for a night out and boost their morale
  - Arrange a training camp - Increase the stats of your players
  - Appeal red card - Ask the FA to overturn a player ban
  - Build new stadium - Save time by building a whole new stadium

## Screenshots
![Loading Screen](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/loading.png)
![Load Game](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/load-game.png)
![Change Team](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/change-team.png)
![Free Players](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/free-players.png)
![Convert Coach](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/convert-coach.png)
![Telephone](https://raw.githubusercontent.com/martinbutt/pm3000/refs/heads/main/docs/screenshots/telephone.png)

## Building

### Prerequisites

**On Debian/Ubuntu based distributions, use the following command:**

```sh
sudo apt install git build-essential pkg-config cmake cmake-data libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libgtk-3-dev
```

**On macOS, install the SDL2/SDL2_image/SDL2_ttf bottles so the release binary can load those dylibs from `/opt/homebrew/opt`.**

```sh
brew install sdl2 sdl2_image sdl2_ttf
```

`gtk+3` is only required when building on Linux (see the Debian/Ubuntu instructions).

If you want to build from source locally, also install the build tooling before running `cmake`/`ninja`:

```sh
brew install git cmake ninja
```

### Instructions

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

### Tests

After configuring, you can build and run the lightweight unit tests (currently covering PM3 utility pricing logic):

```sh
# From the repo root (after cmake ..)
cmake --build build --target pm3_utils_tests
cd build && ctest --output-on-failure
```

Add more cases under `tests/` as you extend the utilities.

### SWOS team import tool

The repo now vendors the SWOS TEAM.xxx parser directly (no external checkout needed). A CLI helper ships with the build to import SWOS teams/players into a PM3 save:

```sh
# Build the tool
cmake --build build --target swos_import_tool

# Import TEAM.008 into save slot 1 for a PM3 folder
./build/swos_import_tool --team /path/to/TEAM.008 --pm3 /path/to/PM3 --game 1
```

What it does:
- Matches imported teams to existing GAMEB clubs by name and updates league/manager/kit, renaming players role-for-role.
- Any imported teams not found replace unmatched clubs, assign a generic stadium name, copy kit colors, and rename squad players in place.
> **Note:** SWOS and PM3 have different league structures now, so the import is a best effort—team counts per division still differ and some placements/kits get guessed where the source data no longer lines up perfectly.

#### Refreshing `TEAM.008` from SWOS2020

`TEAM.008` is the Sensible World of Soccer squad file that the importer reads. The SWOS2020 project keeps a curated, up-to-date version:

1. Download the latest package from https://swos2020.com/downloads. Pick the `.zip`/`.lha` bundle for the team database.
2. Extract the archive with your favorite tool (`lha`, `7z`, etc.) until you find `TEAM.008`.
3. Pass that extracted file to `swos_import_tool --team` to import the newest SWOS rosters into your PM3 save.

## Inspecting PM3 data files

`inspect_pm3_data` is now a lightweight dumper that reads `gamedata.dat` and prints the core `gamea` records as plain text for debugging.

```sh
# Build the tool
cmake --build build --target inspect_pm3_data

# Dump the gamedata summary
./build/inspect_pm3_data --pm3 /path/to/PM3
```

This outputs every `club_index`, `top_scorer`, and league table entry in a basic, predictable format that can later be grepped or diffed while keeping manual edits minimal.

## Acknowledgements
Special thanks to [@eb4x](https://www.github.com/eb4x) for the https://github.com/eb4x/pm3 project. PM3000 would not exist without it.

Thanks to https://github.com/aminosbh/sdl2-image-sample for the SDL2 scaffold to start the project.

### Uses
- https://github.com/btzy/nativefiledialog-extended
- https://github.com/viznut/unscii
- https://www.1001fonts.com/acknowledge-tt-brk-font.html
