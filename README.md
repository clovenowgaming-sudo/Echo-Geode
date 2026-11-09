# Echo (Geode port)

Replay bot "Echo" re-written as a **Geode mod** for **Geometry Dash 2.2081**, **Geode 5.11.2**,
**Android 32-bit and 64-bit**.

The original Echo (a Windows `.dll` built on MinHook + Dear ImGui) cannot run on Android, so the
core features were rewritten against the Geode API and the in-game cocos2d UI.

## Usage
1. Open the pause menu. A round green **E** button appears — tap it to open / close the Echo menu.
2. **Drag** the E button anywhere on the screen. Its position is saved.
3. Menu tabs:
   - **Macro** – mode (Idle / Record / Play), save, load, export plain text, clear.
   - **Hacks** – speed, frame advance (+ draggable *Step* button), hitboxes, info label.
   - **Bot** – autoclicker, clickbot.
4. Pick **Record** or **Play**, close the menu and press *Resume*: the level restarts and the bot starts on frame 0.

## Macro files
Saved in the mod's save folder (`.../Android/data/com.geode.launcher/files/.../echo.echobot/macros`).
Supported: `.echo.json` (default, same layout as Echo), legacy binary `.echo`, plain text `.txt`
(`fps` on the first line, then `frame down player2` per line). Frames of imported macros are rescaled to 240 TPS.

## Clickbot
Drop `click*.ogg|mp3|wav` and `release*.ogg|mp3|wav` into the `clicks` folder next to `macros`.

## Not ported (Windows-only / needs desktop)
Video renderer (ffmpeg), ImGui editor, trajectory / layout mode, file dialogs, keybinds.

## Build
Push to GitHub — the workflow in `.github/workflows/build.yml` builds Android32 + Android64 and
uploads a single `echo.echobot.geode`.
Local: `geode build --platform android64` (needs the Android NDK).

Credits: original Echo authors; matcool, Aly (imgui-hook), andre (cocos headers).
