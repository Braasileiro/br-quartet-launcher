# br-quartet-launcher
A lightweight, custom C++ written launcher for **BLUE REFLECTION Quartet** designed to prevent startup crashes caused by **RTSS** (Rivatuner Statistics Server) during game initialization.

* **Direct game booting**: Dynamically loads the specific game modules (`B0.dll` through `B4.dll`) depending on the selected arguments.

* **RTSS compatibility**: Prevents startup crashes associated with RTSS and possibly other overlays by managing window creation, game exported procedure callbacks and process lifecycle.

* **Game swap**: Handles internal return codes (`RETURN TO TOP` and `EXIT GAME`).

## Important Information
Tested to resolve the crash issue when RTSS is open, but may also resolve the issue for other similar software.

The launcher creates a log file in the game folder called `br-quartet-launcher.log`.

## Installation
1. Download the latest [release](https://github.com/Braasileiro/br-quartet-launcher/releases/latest).
2. Backup the original executable
   - Go to your game installation directory (e.g., `E:\Games\Steam\steamapps\common\BLUE REFLECTION Quartet`).
   - Locate the original game executable (`BLUE REFLECTION Quartet.exe`).
   - Rename it to any other name (for example, `BLUE REFLECTION Quartet.bak.exe`) to keep it safely as a backup.

3. Install the launcher
   - Place the `BLUE REFLECTION Quartet.exe` (this custom launcher) directly into the game's root directory where the original `.exe` used to be.

4. Run
   - Launching the game normally through Steam will now execute this custom wrapper, opening the default menu (`quartet / B0.dll`).

## Steam Launch Options (Direct Boot)
If you want to boot straight into a specific game in the collection without having to navigate through the main launcher menu, you can configure the launch options in Steam.

1. Open **Steam** and go to your **Library**.
2. Right-click **BLUE REFLECTION Quartet** and select Properties....
3. Under the General tab, look for the **Launch Options** text box.
4. Type your target game into the field (e.g., `tie` as shown below).

### Simple
<p align="center">
  <img src=".github/images/steam_launch_options.png">
</p>

### Linux / Environment Variables
<p align="center">
  <img src=".github/images/steam_launch_options_linux.png">
</p>

## Available Arguments
* `quartet` (Default) — Loads the main collection launcher (`B0.dll`).
* `br` — Loads BLUE REFLECTION (`B1.dll`).
* `ray` — Loads BLUE REFLECTION: Ray (`B2.dll`).
* `sun` — Loads BLUE REFLECTION: Sun (`B3.dll`).
* `tie` — Loads BLUE REFLECTION: Second Light (`B4.dll`).
