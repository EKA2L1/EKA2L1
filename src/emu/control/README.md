# Cross-platform control API

The control API uses HTTP/JSON on loopback. The versioned `/v1` contract remains
compatible with the iOS simulator automation. `GET /v1` lists endpoints and
`GET /v1/capabilities` describes frontend-specific features. Screenshots, icons,
logs and file downloads retain their native content types rather than a JSON
encoding.

Qt enables the listener only with `--debug-control-port <1..65535>`. Android
enables it only when a cold-start Activity Intent carries the integer extra
`debug_control_port` in that range. Neither setting is persisted. With no
argument/extra, neither frontend opens a control socket. Android host clients
can reach the listener through `adb forward tcp:<port> tcp:<port>`. Changing the
Android port requires a new process. The iOS simulator retains its default port
8765 and `-DebugServerPort`; physical iOS builds do not start the listener.

The `control` library owns the transport, endpoint index, jobs, input sequences,
key-name mapping, waits and host file/log I/O. Frontend adapters execute atomic operations on the
frontend's control or UI queue. The native backend shares the Qt and Android
emulator operations; iOS retains its DeviceStore/navigation adapter. Neither
HTTP parsing nor input timing belongs in the emulator kernel or guest services.
A second transport can call the service without duplicating task and input
semantics. Frontends depend on `control`; `control_native` depends on the emulator
libraries. Emulator libraries do not depend on either control target. File/log
I/O runs on control workers after the frontend resolves a path, without holding
the UI queue or Android lifecycle mutex.

The service is constructed only when enabled. Timed operations have four slots
separate from ordinary requests and the long-operation worker, so waits cannot
starve session close or job polling. Extra timed requests receive HTTP 503.
The transport limits connections to 32, headers to 16 KiB and aggregate incoming
bodies to 1 GiB. Binary bodies move through the C++ transport without extra
whole-body copies.

Long operations are jobs. The default request waits for the result; `async=1`
returns a job record and HTTP 202. Records include progress when the underlying
operation provides it, completion/error and timing. Only one long operation
owns the frontend at a time. Polling and waits remain available. Input sequences
run in order, release held input on errors, and are cancelled before an API
launch, close or device operation changes the session. A JSON-RPC batch would
not provide this ordering contract.

API touches bypass host UI event delivery. Guest coordinates refer to guest
pixels; normalized coordinates are relative to the guest picture. Qt and Android
support these two spaces. iOS additionally supports render-surface coordinates.
The native screenshot path reads the guest render texture. Only iOS currently
supports a screenshot of the entire host window. Accelerometer injection uses
the guest device's natural axes in m/s², or g with `unit: "g"`. Android and iOS
provide an override; Qt reports this capability as unavailable because its null
sensor backend does not deliver motion samples.

Paths in install requests belong to the machine running the emulator, including
Android's app-accessible filesystem. Clients can upload through `PUT /v1/files`
with a raw body, then pass the resulting `hostPath` to installation. `docs:/`
addresses the frontend's data root; `C:/`, `D:/` and `E:/` address the selected
device's own writable drives, including E underneath a mounted card. Components
of these virtual paths match case-insensitively. `host:` explicitly selects an
absolute host path, including Windows drive letters that would otherwise mean
a guest drive. Other absolute host paths remain accepted. On native frontends,
boot another device before deleting the active one. Config keys returned by a
frontend describe that frontend; iOS-only JIT and performance settings are not
portable.

In Android control mode the host process survives guest exits so that another
API launch can use the same listener. Launches still create EmulatorActivity,
use its profile and wait for its rendering surface. Ordinary Android launches
retain their process-exit behavior.
