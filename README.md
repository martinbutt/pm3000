# PM3000 - Premier Manager 3 Game Add-on

PM3000 is an add-on for Premier Manager 3, which allows you to load your save games to use additional functionality.

The new features are intended to overcome some of the game's annoyances.
- Change Team - This screen allows you to switch to a new team, unlocking the ability to start the game as your favorite team.
- View Squad - This screen shows your current team for ease of visibility
- Scout - A scout that can see the stats of any player
- Free Players - This screen shows all of the out-of-contract players
- Convert Player to Coach - This screen allows you to retire a player and convert them into a coach
- Telephone - This adds a few new features:
-- Advertise for fans - Run an ad campaign and increase your fan base, leading to more attendance
-- Entertain team - Take the team for a night out and boost their morale
-- Arrange a training camp - Increase the stats of your players
-- Appeal red card - Ask the FA to overturn a player ban
-- Build new stadium - Save time by building a whole new stadium

## Screenshots
![Loading Screen](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/loading.png)
![Load Game](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/load-game.png)
![Change Team](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/change-team.png)
![Free Players](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/free-players.png)
![Convert Coach](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/convert-coach.png)
![Telephone](https://raw.githubusercontent.com/martinbutt/pm3/docs/screenshots/telephone.png)

## Building

### Prerequisites

**On Debian/Ubuntu based distributions, use the following command:**

```sh
sudo apt install git build-essential pkg-config cmake cmake-data libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
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

## Acknowledgements
Special thanks to @eb4x for the https://github.com/eb4x/pm3 project. PM3000 would not exist without it.

Thanks to https://github.com/aminosbh/sdl2-image-sample for the SDL2 scaffold to start the project.

### Uses
https://github.com/btzy/nativefiledialog-extended
https://github.com/viznut/unscii
https://www.1001fonts.com/acknowledge-tt-brk-font.html