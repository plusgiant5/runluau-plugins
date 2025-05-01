# Current plugins for runluau.
Builds directly into the build output folder of [runluau](https://github.com/plusgiant5/runluau) for fast testing.

## How To Use Plugins
Simply put them in the `plugins` folder and they will work. Plugins can do anything to the environment, but all of the plugins here register a global library to access their functions.
A list of the functions the plugins here provide can be found in the [runluau env.d.luau file](https://github.com/plusgiant5/runluau/blob/main/external/env.d.luau).

## How To Create a New Plugin
1. Copy `runluau-template`, and rename it.
2. Rename `runluau-template.vcxproj`, `runluau-template.vcxproj.filters`, and `runluau-template.vcxproj.user` accordingly to the name you chose.
3. Right click the runluau-plugins solution and click `Add -> Existing Project...`.
4. Pick the `.vcxproj` of the new plugin you made.
5. Edit `lib.cpp` to do whatever you want.

# Plugin Descriptions
### betterprint
- Adds a colored print output (by replacing the `print` global) which formats tables and functions
- Adds global `tostringex`, which is tostring but with similar table and function support
- Adds `warn`, which is print except it's all yellow
### filesystem
- Adds the `fs` global for filesystem management
- If the `osunsafe` plugin is loaded, any files can be accessed. If not, all operations are locked in a sandbox folder
### graphics
- Adds the `gfx` global for pixel-by-pixel rendering
- Start by calling `gfx.create_window`. It will return a frame buffer as a Luau `buffer`, then you can simply write to it to draw pixels (32 bit integers, 0xRRGGBB, last byte is ignored)
- Y is inverted, so coordinates 0, 0 are for the bottom left
- Use `gfx.get_window_events` to get key presses and mouse scrolls. Any operation on a window only requires you to pass in its frame buffer as identification
### luau
- Adds the `luau` global for calling some useful Luau C++ API functions
- Supports compiling and parsing Luau source code with various settings
- Parser optionally parses declaration syntax
### net
- Will be used for http requests and websockets... but no work has started yet for some reason
### osunsafe
- Adds less sandboxed functions to the base `os` library
- Includes clipboard functions, environment variables, and `os.execute`
### task
- Adds the `task` global, implementing the [Roblox task library](https://create.roblox.com/docs/reference/engine/libraries/task)

# Plugin Security
If you want to keep Luau sandboxed, take note of which plugins here are safe or unsafe.
Safe:
- betterprint
- luau
- task
Caution:
- graphics (might have extremely rare bugs)
- filesystem (only unsafe if osunsafe is loaded)
Unsafe:
- net
- osunsafe
