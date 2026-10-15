# Control server

The control server lets another program drive a running emulator: list, install, launch and
kill apps, press keys, touch the screen, take screenshots, pause and quit. It speaks [JSON-RPC 2.0](https://www.jsonrpc.org/specification) over a local socket.

```
eka2l1_qt --control "$XDG_RUNTIME_DIR/eka2l1.sock"
```

Without `--control` nothing of this runs.

## Endpoints

`--control` takes one of:

| Endpoint | Example | Notes |
|---|---|---|
| A socket file path (Linux, macOS) | `$XDG_RUNTIME_DIR/eka2l1.sock` | A socket file left behind by a process that is gone is replaced; one another process still listens on is not, and the emulator does not start. The file is removed when the emulator exits. A relative path is taken from the directory the emulator starts in. |
| A pipe name (Windows) | `\\.\pipe\eka2l1` | Not tested. See the access notes below. |
| Loopback TCP | `tcp:127.0.0.1:7777`, `tcp:[::1]:7777`, `tcp:localhost:7777` | Needs a token of at least 16 bytes in the environment variable `EKA2L1_CONTROL_TOKEN` (`openssl rand -hex 16` makes one); every connection must call `auth` with it first. Any other host is refused. |

Whoever can connect controls the emulator: they can install and run code in the guest, and read
and write host files with the emulator's rights (`package.install` reads any host path).

- A socket file is created readable and writable by its owner only (mode 0600, whatever the
  umask). That mode is the only access control a local socket has, so also put it in a
  directory only you can enter, such as `$XDG_RUNTIME_DIR`, rather than `/tmp`.
- A Windows pipe gets libuv's default security: full access for SYSTEM, Administrators and its
  owner, read access for Everyone and anonymous users, and remote clients are not rejected.
  Read access cannot send requests, but until the pipe gets an owner-only ACL, prefer loopback
  TCP with a token on Windows.
- On loopback TCP, anyone on the machine can connect, so the token is what keeps them out. A
  wrong token closes the connection. On Windows another process can bind the same loopback
  port (libuv does not ask for exclusive use), and a client may then reach it instead of the
  emulator and give it the token.

If the server cannot start (bad endpoint, socket in use, TCP without a token), the emulator
prints why and exits with a non-zero code.

## Messages

- One JSON-RPC 2.0 message per line: a request, a notification or a batch, in UTF-8, ended by
  `\n` (`\r\n` is accepted, blank lines are skipped). A line longer than 4 MiB, or 64 KiB
  before `auth` on a TCP endpoint, closes the connection with a parse error.
- Parameters are passed by name, in an object. A request without `params` is the same as one
  with `{}`.
- Every result is an object; methods with nothing to report answer `{}`.
- A valid request without an `id` (a JSON-RPC notification) runs, but gets no answer. An
  invalid one is answered with `"id": null`, with or without an `id`.
- Batches run their calls in order and answer with an array, without the notifications.

A UID parameter is a number (`3879017519`) or a string of hexadecimal digits after `0x`
(`"0xE7351C2F"`).

## Limits

- At most 8 connections at a time. One more gets a -32003 error line and is closed.
- A connection whose answers and notifications pile up unread past 8 MiB is no longer read or
  served, and misses the notifications sent meanwhile, until its client has read them down to
  4 MiB. Read your socket.
- When the emulator exits, each connection gets two seconds to take what was queued for it;
  then it is closed anyway.

## Threading and ordering

- The server runs on a thread of its own and handles one request at a time, in the order the
  requests arrive, over all connections. A request that waits (`input.key` holding a key down,
  `app.launch` waiting for the frontend) holds up the others.
- Whatever touches the emulated system runs between two emulation slices: on the emulation
  thread right before it schedules the next guest thread, or on the server thread with the
  emulation locked out, whenever the emulation thread is outside its loop (between two slices,
  while paused, or when no device runs). No guest instruction runs at the same time, and the
  device cannot be reset or switched in the middle.
- When a response arrives, what the method did is done in the emulated system: the process
  exists, the killed process is gone, the package is installed and the app list rescanned, the
  input event is queued to the window server. The app handles queued input the next time it
  runs, not before the response.

## Errors

The first five codes are JSON-RPC 2.0's own.

| Code | Name | When |
|---|---|---|
| -32700 | parse error | The line is not JSON, or longer than 4 MiB (64 KiB before `auth`). |
| -32600 | invalid request | Not a JSON-RPC 2.0 request object, or an empty batch. |
| -32601 | method not found | No method by that name. |
| -32602 | invalid params | A parameter is missing, has the wrong type or is out of range; the message names it. |
| -32603 | internal error | Something failed that should not have; the message says what. |
| -32000 | not ready | No device has been booted, or the emulator has no system or graphics driver yet. |
| -32001 | unauthorized | `auth` was not called with the right token (TCP endpoints only). A wrong token also closes the connection. |
| -32002 | not found | The app, package, file or screen named in the request does not exist. |
| -32003 | failed | The emulator tried and could not do it; the message says why, or points to the log. Also sent, with `"id": null`, to a connection past the limit before it is closed. |
| -32004 | shutting down | The emulator is exiting; the request was not run. |

## Methods

### `auth`

| Params | `token`: string |
|---|---|
| Result | `{}` |
| Errors | -32001 when the token is wrong; the connection is then closed |

Needed once per connection, before anything else, on a TCP endpoint. Each connection gets one
try: after a wrong token nothing else on it runs, not even the rest of its batch, and it is
closed once the answer is written. On a local endpoint there is no token and `auth` always
succeeds.

### `emulator.info`

| Params | none |
|---|---|
| Result | `name`: `"EKA2L1"`; `version`: the build's branch and commit; `protocol`: `1`; `paused`: bool; `device`: `{manufacturer, model, firmware, os}` of the running device, or `null` |

`protocol` changes when a method changes in a way an existing client would notice; new methods
and new optional parameters do not change it.

```
{"jsonrpc":"2.0","id":1,"method":"emulator.info"}
{"jsonrpc":"2.0","id":1,"result":{"name":"EKA2L1","version":"dev/control-server-8ba26e4","protocol":1,"paused":false,"device":{"manufacturer":"Nokia","model":"N00","firmware":"RM-469","os":"epoc93fp2"}}}
```

### `emulator.pause`, `emulator.resume`

| Params | none |
|---|---|
| Result | `paused`: `true` (pause) or `false` (resume) |

`emulator.pause` answers once the slice in flight has ended: from then on no guest instruction
runs until `emulator.resume`. Every other method keeps working while paused; input sent while
paused is handled after the resume. The frontend's own pause control shows the same state.

### `emulator.exit`

| Params | `code`: integer 0–255, optional, default 0 |
|---|---|
| Result | `{}` |

Answers, then shuts the emulator down; the process exits with `code`. The server closes every
connection once what it had queued is written.

### `apps.list`

| Params | none |
|---|---|
| Result | `apps`: array of `{uid, name, short_name, executable, drive, hidden, running}` |

Every app the app list server knows. `uid` is a number; `executable` is the guest path;
`drive` the letter it is installed on; `running` is true while a process with that UID exists.

### `app.launch`

| Params | `uid`: UID; `document`: guest path, optional; `args`: string, optional |
|---|---|
| Result | `pid`: the new process's id |
| Errors | -32002 no app with that UID; -32003 the app could not be started |

Starts the app as the frontend does. With `document` it is asked to open that document;
`args` is passed as the tail of its command line. The answer comes once the process exists;
the app has not drawn anything yet.

### `app.kill`

| Params | `uid`: UID |
|---|---|
| Result | `killed`: how many processes were killed |
| Errors | -32002 no app with that UID is running |

Kills every running process of the app (exit type `kill`, reason 0, category `Kill`). The app
gets no chance to clean up, as on a device when it is closed from the task list.

### `package.install`

| Params | `path`: host path of a `.sis`/`.sisx` file; `drive`: drive letter, optional |
|---|---|
| Result | `{}` |
| Errors | -32002 no file at `path`; -32602 `drive` is not a writable drive of the device; -32003 the installation was aborted or the file is not a package the emulator can install |

Installs without asking anyone: where a package would ask a question, the default answer is
taken. Packages embedded in another package, and packages in the old (EPOC Release 5/6) format,
may still ask through the frontend. `drive` defaults to `E` (`D` on Series 80 devices). The app
list is rescanned before the answer, so `apps.list` and `app.launch` see the new apps. Use an
absolute `path`: a relative one is resolved against the emulator's working directory.

### `package.remove`

| Params | `uid`: the package UID |
|---|---|
| Result | `{}` |
| Errors | -32002 no package with that UID is installed; -32003 it could not be removed |

### `input.key`

| Params | `key`: key name, or `scancode`: integer (exactly one of them); `action`: `"tap"` (default), `"press"` or `"release"`; `hold_ms`: integer 0–10000, default 50 |
|---|---|
| Result | `{}` |

Sends the key to the guest as a standard scan code, whatever the frontend's key bindings are.
`tap` presses the key, lets the emulation run for `hold_ms` milliseconds, releases it and
answers after the release. While the emulation is paused the hold passes with no guest time,
so the app gets the press and the release together once it resumes. `press` and `release`
send one half each.

| Name | Scan code | | Name | Scan code |
|---|---|---|---|---|
| `left_softkey` | `0xA4` | | `send` | `0xC4` |
| `right_softkey` | `0xA5` | | `end` | `0xC5` |
| `select` | `0xA7` | | `menu` | `0xB4` |
| `up` | `0x10` | | `edit` | `0x12` |
| `down` | `0x11` | | `clear` | `0x01` |
| `left` | `0x0E` | | `hash` | `0x7F` |
| `right` | `0x0F` | | `star` | `0x2A` |
| `0` … `9` | `0x30` … `0x39` | | | |

### `input.touch`

| Params | `x`, `y`: integers; `action`: `"tap"` (default), `"press"`, `"move"` or `"release"`; `pointer`: integer 0–7, default 0; `hold_ms`: integer 0–10000, default 50 |
|---|---|
| Result | `{}` |

`x` and `y` are guest screen pixels, from the top left corner of the screen, whatever size
the frontend shows the screen at. `move` drags a pressed pointer; `pointer` tells fingers apart
on multi-touch devices. `tap` works as for keys.

### `screen.capture`

| Params | `path`: host file path, optional; `screen`: integer, optional |
|---|---|
| Result | `width`, `height`; and `path` when a path was given, else `png`: the PNG file in base64 |
| Errors | -32000 no graphics driver yet; -32002 no such screen; -32003 nothing has been drawn on the screen yet, reading it back failed, something already exists at `path`, or `path` could not be written |

Takes a PNG of what the emulated screen shows (RGB, no alpha), at the size the emulator renders
it. Without `screen`, the screen that has the focus. It is what the guest drew, without the
frontend's scaling, borders or overlays.

Without `path` the PNG comes back in the answer, and the client writes it where it likes. With
`path` the emulator creates a new file there, with its own rights: it never replaces,
truncates or follows anything already at that path, a symbolic link included, and answers
-32003 naming the path instead. A relative `path` is resolved against the emulator's working
directory; give an absolute one.

## Examples

One request from a shell, with OpenBSD netcat (`-N` ends the stream after the request; the
server still answers it):

```
echo '{"jsonrpc":"2.0","id":1,"method":"apps.list"}' | nc -N -U "$XDG_RUNTIME_DIR/eka2l1.sock"
```

A Python client, standard library only:

```python
import base64, json, os, socket, time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(os.path.join(os.environ["XDG_RUNTIME_DIR"], "eka2l1.sock"))
stream = sock.makefile("rw", encoding="utf-8", newline="\n")
next_id = 0

def call(method, **params):
    global next_id
    next_id += 1
    stream.write(json.dumps({"jsonrpc": "2.0", "id": next_id, "method": method, "params": params}) + "\n")
    stream.flush()
    message = json.loads(stream.readline())
    if "error" in message:
        raise RuntimeError(message["error"])
    return message["result"]

call("package.install", path="/home/me/hello.sisx")
pid = call("app.launch", uid="0xE7351C2F")["pid"]
time.sleep(10)  # the answer comes before the app has drawn anything
call("input.key", key="down")
with open("screen.png", "wb") as screen:
    screen.write(base64.b64decode(call("screen.capture")["png"]))
call("app.kill", uid="0xE7351C2F")
```

## The Qt frontend

- An app started with `app.launch` is treated like one started from the app list: the window
  switches to the screen, and when the app exits the device reboots and the app list comes
  back. `app.launch` waits for such a reboot to finish before it starts the next app, and right
  after start-up for the window to finish loading. Until an app draws again, `screen.capture`
  answers -32003.
- `emulator.pause` and `emulator.resume` flip the same switch as the Pause menu item.
- `emulator.exit` ends the main loop, as closing the main window does.

## Adding the server to another frontend

The server lives in the `control` library and does not depend on Qt. A frontend implements
`control::frontend` (`include/control/frontend.h`): access to the `system`, and pause, resume
and exit done its own way. It then creates a `control::server`, calls `start()` with the
endpoint text, and calls `stop()` before it destroys the system, while its emulation and
graphics threads still run. The emulation thread must keep calling `system::loop()`, or, when
the frontend pauses, stop calling it: queued work then runs on the server thread. Queued work
can run on the server thread between two `loop()` calls while the emulation runs too, so
`control::frontend` callbacks made from it (`on_app_launched`) can come from either thread.
