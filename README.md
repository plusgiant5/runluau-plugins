# Current plugins for runluau.
Builds directly into the build output folder of [runluau](https://github.com/plusgiant5/runluau) for fast testing.

## How To Create a New Plugin
1. Copy `runluau-template`, and rename it.
2. Rename `runluau-template.vcxproj`, `runluau-template.vcxproj.filters`, and `runluau-template.vcxproj.user` according to the name you chose.
3. Right click the runluau-plugins solution and click `Add -> Existing Project...`.
4. Pick the `.vcxproj` of the new plugin you made.
5. Edit `lib.cpp` to do whatever you want.
