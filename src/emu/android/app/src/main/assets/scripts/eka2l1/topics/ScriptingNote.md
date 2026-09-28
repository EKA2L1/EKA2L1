# Note about scripting

## Brief purpose introduction

EKA2L1 provides a *Lua scripting API* for interacting with the emulator, aiding those who like to write app/game specific patches, reverse-engineering or other analysing tasks without touching the core C++ code of the emulator.

The API itself is very barebone for debugging usages on the emulator. If you are interested in suggesting more features after looking at this document, feel free to contact the developers on [Discord](https://discord.gg/5Bm5SJ9).

## Script location

EKA2L1 loads all scripts in the root **scripts** folder, with resides in the default current directory. The current directory of the emulator for each OS.

- **On Windows:** This is the same folder as where the emulator executable lies.

- **On MacOS:** `/Users/<current_user>/Library/Application Support/EKA2L1/`

- **On Linux:** `~/.local/share/EKA2L1/`

The scripts are loaded firstly after all the emulated device's initialisations are done.

## Script lifetime

Script will be unloaded on these occassions:

- On script file modification (delete, move, modify, etc...).

- On emulator exit.

- On device reset/choosing another device.

If you plan on making your script persistent across the run of the emulator, you might need to save your process frequently, or avoid doing time-intensive task.

## Script reload

EKA2L1 now supports reloading script on the fly. If you modify the script while EKA2L1 is running, the old script will be unloaded in favour of the new one, helping those that want to debug live.

Note that this may come with the risk of crash, if the script is not coded properly. This risk is much smaller when the emulator is idle.

## Script executor

EKA2L1 makes use of [**LuaJIT**](https://github.com/LuaJIT/LuaJIT) to execute your scripts, and the scripting part is heavily designed around LuaJIT's FFI feature.

Because of so, your scripts is limited to maximum compability with Lua 5.1, so take that in mind when you write a script.