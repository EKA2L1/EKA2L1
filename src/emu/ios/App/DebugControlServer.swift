import Foundation
import Network
import UIKit

// Navigation and session signals shared by the frontend and the simulator-only
// control server. All are posted on the main queue.
extension Notification.Name {
    // userInfo["uid"]: UInt32. ContentView pushes the emulator screen for it.
    static let eka2l1RemoteLaunchApp = Notification.Name("eka2l1RemoteLaunchApp")
    // EmulatorView closes the running app and pops itself.
    static let eka2l1RemoteCloseApp = Notification.Name("eka2l1RemoteCloseApp")
    // userInfo["uid"]: UInt32, ["success"]: Bool.
    static let eka2l1GuestAppLaunched = Notification.Name("eka2l1GuestAppLaunched")
    // userInfo["uid"]: UInt32, ["fatal"]: String (absent for a clean exit).
    static let eka2l1GuestAppExited = Notification.Name("eka2l1GuestAppExited")
}

/// HTTP control API for simulator builds.
///
/// Simulator builds serve a small HTTP/JSON API on `127.0.0.1:8765`. It covers
/// what the UI can do, so automation does not have to drive the UI tap by tap.
/// The simulator shares the host's network stack, so plain
/// `curl localhost:8765/...` works from the Mac. Device builds do not compile
/// the server.
///
/// - Port: launch argument `-DebugServerPort <n>`. `0` turns the server off.
///   Give each booted simulator its own port.
/// - The server starts once the emulator core is up, which is the first thing
///   the home screen does. `GET /v1/status` answers as soon as it is listening.
/// - Paths in request bodies are **host paths**. A simulator app can read
///   `~/Documents/...` directly, so packages do not need to be copied in first.
///
/// ## Conventions
///
/// - Every response is JSON except screenshots, icons, file downloads and the log.
/// - Successful responses carry the resource itself, with no envelope.
/// - Failures use a non-2xx status and
///   `{"error": {"code": "...", "message": "..."}}`, so `curl -f` fails on them:
///
///   | Code | Status | Meaning |
///   |---|---|---|
///   | `bad_request` | 400 | Malformed or missing input |
///   | `not_found` | 404 | No such device, app, file, job or route |
///   | `busy` | 409 | Another emulator operation holds the session |
///   | `no_device` | 409 | No device is booted |
///   | `no_screen` | 409 | The emulator screen is not up |
///   | `not_running` | 409 | No app is running |
///   | `*_failed` | 422 | The emulator refused the operation (`install_failed`, `boot_failed`, `mount_failed`, …) |
///   | `timeout` | 504 | A wait or launch did not complete in time |
///
/// - UIDs are accepted as `0x101FD40B` or decimal, and are always returned as
///   `0x%08X`. A device is named by its index or its firmware code (`rh-29`,
///   case-insensitive).
/// - **Jobs.** Long operations (device install/boot/delete/rescan, app install,
///   card mount) run as jobs. By default the request blocks until the job ends
///   and returns its result. With `?async=1` the request returns `202` with the
///   job record right away; poll it at `GET /v1/jobs/{id}`. The record has
///   `state` (`running|succeeded|failed`), `progress`, `result` and `error`.
/// - State changes go through the same `DeviceStore`/navigation paths as the
///   UI, so the screen always matches what the API reports.
///
/// ## Endpoints
///
/// | Method & path | Body / query | Notes |
/// |---|---|---|
/// | `GET /v1` | | Endpoint list |
/// | `GET /v1/status` | | Booted device, card, session, guest screen geometry, frame count, running jobs, `documents` and `log` host paths |
/// | `GET /v1/devices` | | |
/// | `POST /v1/devices` | `{"rom", "rpkg"?}` or `{"archive"}`, `"isolateDrives"?`, `"boot"?` (default true) | Job. Boots the new device |
/// | `POST /v1/devices/rescan` | | Job |
/// | `POST /v1/devices/{id}/boot` | | Job. Closes a running app first |
/// | `PATCH /v1/devices/{id}` | `{"name"}` | Rename |
/// | `DELETE /v1/devices/{id}` | | Job |
/// | `GET /v1/apps` | `?system=1` | Includes ROM apps |
/// | `POST /v1/apps` | `{"path"}` or `{"paths": [...]}`, `"type"?` | Job. Type comes from the path: `.sis/.sisx` → `sis`; `.n-gage` → `ngage2` (staged onto `E:\n-gage`); anything else → `ngage` (N-Gage 1.0 card folder/archive). The result lists `added` apps |
/// | `DELETE /v1/apps/{uid}` | | Uninstall |
/// | `GET /v1/apps/{uid}/icon` | `?size=96` | PNG |
/// | `POST /v1/apps/{uid}/launch` | `{"timeoutMs"?}`, `?wait=0` | Closes the current app, pushes the emulator screen and waits for the launch result |
/// | `GET /v1/session` | | `state`: `none`, `launching`, `running`, `launch_failed`, `exited`, `crashed` or `closed`; plus `lastExit` (`fatal` holds the panic text) |
/// | `DELETE /v1/session` | | Close the running app (the in-game "Exit" menu item) |
/// | `GET/PUT /v1/session/screen-mode` | `{"mode"}` | |
/// | `GET/PUT /v1/session/frame-limit` | `{"limit"}` | 0 = unlimited |
/// | `GET /v1/card` | | |
/// | `PUT /v1/card` | `{"path"}` | Job. Mount a card folder or zip on E: |
/// | `DELETE /v1/card` | | Eject |
/// | `GET /v1/keys` | | Key names → scan codes |
/// | `POST /v1/input/key` | `{"key" \| "keys": [...], "action": "tap\|down\|up", "holdMs": 80, "repeat": 1, "gapMs": 80}` | Names include `a`…`z` (case-insensitive), digits and the names from `/v1/keys`; numbers or hex strings send scan codes directly. `keys` presses a chord. `down` keys stay held until `up` or `/v1/input/release` |
/// | `POST /v1/input/touch` | `{"action": "tap\|down\|move\|up\|swipe", "x", "y", "toX"?, "toY"?, "durationMs"?, "holdMs"?, "id"?, "space"?}` | Coordinates are guest pixels by default, the same grid as `/v1/screenshot`. `space: "normalized"` uses 0…1; `"surface"` uses raw render-surface pixels. `id` keeps multi-touch pointers apart |
/// | `POST /v1/input/text` | `{"text", "intervalMs"?}` | Taps letter/digit keys; the guest input method decides what they produce |
/// | `GET/PUT/DELETE /v1/input/accelerometer` | `{"x", "y", "z", "unit"?}` | Holds a fixed accelerometer reading until `DELETE`, `/v1/input/release` or the app exits. See *Accelerometer* below |
/// | `POST /v1/input/sequence` | `{"steps": [...]}` | Each step is a key, touch (`{"touch": {...}}`), text, accelerometer (`{"accelerometer": {...} \| null}`) or `{"waitMs"}` object. A step's own `waitMs` runs after it |
/// | `POST /v1/input/release` | | Releases every key and pointer held through the API and drops the accelerometer reading |
/// | `GET /v1/screenshot` | `?region=guest\|screen&scale=1` | PNG. `guest` (default while a game is up) is the guest picture at its own resolution; `screen` is the whole host window in points. `X-Frame-Count` header |
/// | `GET /v1/log` | `?tail=200`, `?since=<offset>`, `?grep=<regex>` | Text. `X-Log-Offset` gives the position to pass as `since` next time |
/// | `POST /v1/wait` | `{"until": "running\|exited\|idle\|frames\|log\|sleep", "timeoutMs": 30000, "count"?, "pattern"?, "since"?}` | `frames` waits for `count` new frames. `log` waits for a line matching `pattern` written after the call (or after `since`) |
/// | `GET/PATCH /v1/config` | partial config object | Same keys as the settings snapshot |
/// | `GET/PUT /v1/language` | `{"code"}` | |
/// | `GET /v1/files` | `?path=` | Downloads a file, or lists a directory as JSON |
/// | `PUT /v1/files` | `?path=`, raw body | Creates parent directories |
/// | `DELETE /v1/files` | `?path=`, `&recursive=1` | |
/// | `POST /v1/emulator/pause` / `resume` | | |
/// | `GET /v1/jobs`, `GET /v1/jobs/{id}` | | The last 50 jobs |
///
/// ## File paths
///
/// - `C:\...` or `C:/...` is drive C, D or E of the booted device. E is the
///   device's own storage, not a mounted card. Each component matches
///   case-insensitively, the way the guest sees it.
/// - `docs:/...` is the app's Documents folder.
/// - A bare absolute path is a host path.
///
/// URL-encode backslashes, or use forward slashes.
///
/// ## Keys and text
///
/// Letter keys send Symbian keyboard scan codes (`a` → `0x41`, …, `z` → `0x5A`),
/// including on Series 80 devices. They do not select the letters printed on a
/// phone's number pad. Uppercase names select the same physical keys as
/// lowercase names; hold `shift` in a chord when a modifier is needed. The
/// guest decides how to interpret the resulting events. This also applies to
/// `/v1/input/text`, which sends key presses rather than inserting text directly.
///
/// Menu screens often ignore a second key that arrives within a few hundred
/// milliseconds. Put a `waitMs` of about 500 between menu steps.
///
/// ## Touches
///
/// API touches are injected straight into the emulator with their own pointer
/// ids; they do not go through UIKit. A bug in the UITouch path needs real HID
/// touches (for example AXe).
///
/// ## Accelerometer
///
/// `PUT /v1/input/accelerometer` replaces every motion source (the simulator's
/// stand-in phone lying flat, a game controller) with a fixed reading, which
/// the accelerometer and rotation channels report until it is cleared. Values
/// are in m/s², or in g with `"unit": "g"`. The axes are those of the emulated
/// phone in its natural orientation (its first screen mode), whatever the
/// current screen mode or host rotation: x to the right, y towards the top, z
/// out of the screen. Each axis reads the reaction to gravity, so the side
/// facing up reads positive: flat face up is `{"x": 0, "y": 0, "z": 9.81}` and
/// held upright is `{"x": 0, "y": 9.81, "z": 0}`. Every input response reports
/// the held reading as `acceleration` (`null` when none is held).
///
/// ## Examples
///
/// ```sh
/// api=localhost:8765/v1
/// curl -s -X POST $api/devices/rh-29/boot
/// curl -s -X PUT $api/card -d '{"path":"'"$HOME"'/Documents/Apps Backup/Nokia/n-gage/SystemRush.zip"}'
/// curl -s -X POST $api/apps/0x101FD40B/launch
/// curl -s -X POST $api/wait -d '{"until":"frames","count":60}'
/// curl -s -X POST $api/input/sequence -d '{"steps":[{"key":"down","waitMs":600},{"key":"ok"}]}'
/// curl -s -X POST $api/input/key -d '{"key":"5","action":"down"}'          # hold accelerate
/// curl -s -X POST $api/input/key -d '{"keys":["5","left"],"holdMs":1500}'  # chord
/// curl -s -X POST $api/input/key -d '{"key":"s","holdMs":500}'            # Series 80 letter key
/// curl -s -X POST $api/input/touch -d '{"action":"swipe","x":180,"y":500,"toX":180,"toY":200}'
/// curl -s -X PUT $api/input/accelerometer -d '{"x":0,"y":-0.35,"z":0.94,"unit":"g"}' # Asphalt 6 on X7: steer left
/// curl -s -X DELETE $api/input/accelerometer
/// curl -s -o shot.png "$api/screenshot?scale=2"
/// curl -s "$api/files?path=C:/system/apps/6R63/gameprogress.bin" -o save.bin
/// curl -s -X PUT --data-binary @save.bin "$api/files?path=C:/system/apps/6R63/gameprogress.bin"
/// curl -s "$api/log?grep=panic|Access violation&tail=50"
/// curl -s -X DELETE $api/session
/// ```
enum DebugControl {
    // No-op outside simulator builds. `-DebugServerPort <n>` picks the port, 0 disables.
    @MainActor static func start(store: DeviceStore) {
        #if targetEnvironment(simulator)
        DebugControlServer.shared.start(store: store)
        #endif
    }
}

#if targetEnvironment(simulator)

private typealias JSON = [String: Any]

private struct HTTPRequest: Sendable {
    let method: String
    let path: [String]
    let query: [String: String]
    let headers: [String: String]
    let body: Data
}

private struct HTTPResponse: Sendable {
    var status: Int
    var contentType: String
    var body: Data
    var headers: [String: String] = [:]
}

private struct APIError: Error {
    let status: Int
    let code: String
    let message: String

    static func badRequest(_ message: String) -> APIError { APIError(status: 400, code: "bad_request", message: message) }
    static func notFound(_ message: String) -> APIError { APIError(status: 404, code: "not_found", message: message) }
    static func conflict(_ code: String, _ message: String) -> APIError { APIError(status: 409, code: code, message: message) }
    static func failed(_ code: String, _ message: String) -> APIError { APIError(status: 422, code: code, message: message) }
    static let busy = APIError(status: 409, code: "busy", message: "another emulator operation is in flight")
    static let noDevice = APIError(status: 409, code: "no_device", message: "no device is booted")
    static let noScreen = APIError(status: 409, code: "no_screen", message: "the emulator screen is not up")
}

// MARK: - HTTP transport

// One request per connection (the response closes it), which is all curl needs.
private final class HTTPConnection: @unchecked Sendable {
    private static let maxRequestSize = 1 << 30

    private let connection: NWConnection
    private let queue: DispatchQueue
    private let handler: @Sendable (HTTPRequest) async -> HTTPResponse
    private var buffer = Data()
    private var sentContinue = false

    init(connection: NWConnection, queue: DispatchQueue, handler: @escaping @Sendable (HTTPRequest) async -> HTTPResponse) {
        self.connection = connection
        self.queue = queue
        self.handler = handler
    }

    func start() {
        connection.start(queue: queue)
        receive()
    }

    private func receive() {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 1 << 20) { data, _, complete, error in
            if let data { self.buffer.append(data) }
            switch self.parse() {
            case .request(let request):
                Task {
                    let response = await self.handler(request)
                    self.queue.async { self.send(response) }
                }
            case .invalid(let message):
                self.send(DebugControlServer.errorResponse(.badRequest(message)))
            case .incomplete:
                if complete || error != nil {
                    self.connection.cancel()
                } else if self.buffer.count > Self.maxRequestSize {
                    self.send(DebugControlServer.errorResponse(APIError(status: 413, code: "too_large", message: "request too large")))
                } else {
                    self.receive()
                }
            }
        }
    }

    private enum ParseResult {
        case incomplete
        case invalid(String)
        case request(HTTPRequest)
    }

    private func parse() -> ParseResult {
        guard let headerEnd = buffer.range(of: Data("\r\n\r\n".utf8)) else { return .incomplete }
        guard let head = String(data: buffer[..<headerEnd.lowerBound], encoding: .utf8) else {
            return .invalid("headers are not UTF-8")
        }
        let lines = head.components(separatedBy: "\r\n")
        let parts = lines[0].split(separator: " ")
        guard parts.count >= 2 else { return .invalid("malformed request line") }

        var headers: [String: String] = [:]
        for line in lines.dropFirst() {
            guard let colon = line.firstIndex(of: ":") else { continue }
            headers[line[..<colon].lowercased()] = line[line.index(after: colon)...].trimmingCharacters(in: .whitespaces)
        }
        let length = Int(headers["content-length"] ?? "0") ?? 0
        let bodyStart = headerEnd.upperBound
        if buffer.count - bodyStart < length {
            if !sentContinue, headers["expect"]?.lowercased() == "100-continue" {
                sentContinue = true
                connection.send(content: Data("HTTP/1.1 100 Continue\r\n\r\n".utf8), completion: .idempotent)
            }
            return .incomplete
        }

        guard let components = URLComponents(string: "http://localhost" + parts[1]) else {
            return .invalid("malformed request target")
        }
        var query: [String: String] = [:]
        for item in components.queryItems ?? [] {
            query[item.name] = item.value ?? ""
        }
        let path = components.path.split(separator: "/").map(String.init)
        return .request(HTTPRequest(method: String(parts[0]).uppercased(), path: path, query: query,
                                    headers: headers, body: buffer.subdata(in: bodyStart..<(bodyStart + length))))
    }

    private func send(_ response: HTTPResponse) {
        var head = "HTTP/1.1 \(response.status) \(response.status < 400 ? "OK" : "Error")\r\n"
        var headers = response.headers
        headers["Content-Type"] = response.contentType
        headers["Content-Length"] = String(response.body.count)
        headers["Connection"] = "close"
        for (name, value) in headers {
            head += "\(name): \(value)\r\n"
        }
        head += "\r\n"
        var payload = Data(head.utf8)
        payload.append(response.body)
        connection.send(content: payload, completion: .contentProcessed { _ in
            self.connection.cancel()
        })
    }
}

// MARK: - Server

@MainActor
private final class DebugControlServer {
    static let shared = DebugControlServer()
    static let apiVersion = 1

    private var listener: NWListener?
    private let queue = DispatchQueue(label: "eka2l1.debug-control")
    private weak var store: DeviceStore?

    // Session as seen through the frontend's launch/exit signals.
    private var sessionUID: UInt32?
    private var sessionState = "none"
    private var lastExit: JSON?
    private var heldKeys = Set<UInt32>()
    private var heldPointers = Set<Int>()
    private var acceleration: [Double]?
    private var jobs: [Int: Job] = [:]
    private var nextJobID = 1

    func start(store: DeviceStore) {
        self.store = store
        guard listener == nil else { return }
        let configured = UserDefaults.standard.object(forKey: "DebugServerPort") as? Int
            ?? Int(UserDefaults.standard.string(forKey: "DebugServerPort") ?? "")
        let portNumber = configured ?? 8765
        guard portNumber > 0, let port = NWEndpoint.Port(rawValue: UInt16(clamping: portNumber)) else { return }

        let center = NotificationCenter.default
        center.addObserver(forName: .eka2l1GuestAppLaunched, object: nil, queue: .main) { note in
            let uid = note.userInfo?["uid"] as? UInt32
            let success = note.userInfo?["success"] as? Bool ?? false
            MainActor.assumeIsolated { self.sessionDidLaunch(uid: uid, success: success) }
        }
        center.addObserver(forName: .eka2l1GuestAppExited, object: nil, queue: .main) { note in
            let uid = note.userInfo?["uid"] as? UInt32
            let fatal = note.userInfo?["fatal"] as? String
            MainActor.assumeIsolated { self.sessionDidExit(uid: uid, fatal: fatal) }
        }

        let parameters = NWParameters.tcp
        parameters.allowLocalEndpointReuse = true
        parameters.requiredLocalEndpoint = .hostPort(host: "127.0.0.1", port: port)
        do {
            let listener = try NWListener(using: parameters)
            listener.newConnectionHandler = { [queue] connection in
                HTTPConnection(connection: connection, queue: queue) { request in
                    await DebugControlServer.shared.handle(request)
                }.start()
            }
            listener.stateUpdateHandler = { state in
                NSLog("[EKA2L1] debug control server on 127.0.0.1:%d: %@", portNumber, String(describing: state))
            }
            listener.start(queue: queue)
            self.listener = listener
        } catch {
            NSLog("[EKA2L1] debug control server failed to listen on %d: %@", portNumber, String(describing: error))
        }
    }

    private func sessionDidLaunch(uid: UInt32?, success: Bool) {
        sessionUID = uid ?? sessionUID
        sessionState = success ? "running" : "launch_failed"
    }

    private func sessionDidExit(uid: UInt32?, fatal: String?) {
        sessionState = fatal == nil ? "exited" : "crashed"
        var exit: JSON = ["at": ISO8601DateFormatter().string(from: Date())]
        if let uid = uid ?? sessionUID { exit["uid"] = hex(uid) }
        if let fatal { exit["fatal"] = fatal }
        lastExit = exit
        releaseAllInput()
    }

    // MARK: Routing

    func handle(_ request: HTTPRequest) async -> HTTPResponse {
        do {
            return try await route(request)
        } catch let error as APIError {
            return Self.errorResponse(error)
        } catch {
            return Self.errorResponse(APIError(status: 500, code: "internal", message: String(describing: error)))
        }
    }

    nonisolated static func errorResponse(_ error: APIError) -> HTTPResponse {
        let body: JSON = ["error": ["code": error.code, "message": error.message]]
        return HTTPResponse(status: error.status, contentType: "application/json", body: encode(body))
    }

    nonisolated private static func encode(_ object: Any) -> Data {
        var data = (try? JSONSerialization.data(withJSONObject: object,
                                                options: [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes])) ?? Data("{}".utf8)
        data.append(0x0A)
        return data
    }

    private func json(_ object: Any, status: Int = 200) -> HTTPResponse {
        HTTPResponse(status: status, contentType: "application/json", body: Self.encode(object))
    }

    private func route(_ request: HTTPRequest) async throws -> HTTPResponse {
        guard request.path.first == "v1" else {
            if request.path.isEmpty { return json(endpointIndex()) }
            throw APIError.notFound("unknown path; see GET /v1")
        }
        let path = Array(request.path.dropFirst())
        let body = try parseBody(request)

        // A second component naming a device, app or job is matched as `*` and
        // handed over as `id`.
        var key = path
        var id = ""
        if key.count >= 2, ["devices", "apps", "jobs"].contains(key[0]), key[1] != "rescan" {
            id = key[1]
            key[1] = "*"
        }
        switch request.method + " " + key.joined(separator: "/") {
        case "GET ":
            return json(endpointIndex())
        case "GET status":
            return json(status())

        case "GET devices":
            return json(["devices": devicesJSON(), "current": nullable(currentDeviceJSON())])
        case "POST devices":
            return try await runJob("install_device", request) { job in try await self.installDevice(body, job: job) }
        case "POST devices/rescan":
            return try await runJob("rescan_devices", request) { _ in try await self.rescanDevices() }
        case "POST devices/*/boot":
            return try await runJob("boot_device", request) { _ in try await self.bootDevice(id) }
        case "PATCH devices/*":
            return json(try renameDevice(id, body))
        case "DELETE devices/*":
            return try await runJob("delete_device", request) { _ in try await self.deleteDevice(id) }

        case "GET apps":
            return json(["apps": appsJSON(includeSystem: boolParam(request.query["system"]) ?? false)])
        case "POST apps":
            return try await runJob("install_app", request) { _ in try await self.installApps(body) }
        case "DELETE apps/*":
            return json(try uninstallApp(id))
        case "GET apps/*/icon":
            return try appIcon(id, size: UInt(request.query["size"] ?? "") ?? 96)
        case "POST apps/*/launch":
            return json(try await launchApp(id, body, request))

        case "GET session":
            return json(sessionJSON())
        case "DELETE session":
            return json(try await closeApp())
        case "GET session/screen-mode":
            return json(screenModeJSON())
        case "PUT session/screen-mode":
            return json(try await setScreenMode(body))
        case "GET session/frame-limit":
            return json(["limit": EKA2L1Bridge.shared.guestFrameLimit(appUID: try runningUID())])
        case "PUT session/frame-limit":
            let limit = try intField(body, "limit")
            EKA2L1Bridge.shared.setGuestFrameLimit(appUID: try runningUID(), limit: limit)
            return json(["limit": limit])

        case "GET card":
            return json(cardJSON())
        case "PUT card":
            return try await runJob("mount_card", request) { _ in try await self.mountCard(body) }
        case "DELETE card":
            guard try requireIdleStore().ejectCard() else { throw APIError.failed("eject_failed", "no card was ejected") }
            return json(cardJSON())

        case "GET keys":
            return json(["keys": Self.keyNames.mapValues { hex($0) }])
        case "POST input/key", "POST input/touch", "POST input/text":
            try await runInputStep(body, kind: path[1])
            return json(inputStateJSON())
        case "POST input/sequence":
            guard let steps = body["steps"] as? [JSON] else { throw APIError.badRequest("`steps` must be an array of objects") }
            for (index, step) in steps.enumerated() {
                do {
                    try await runInputStep(step, kind: nil)
                } catch let error as APIError {
                    throw APIError(status: error.status, code: error.code, message: "step \(index): \(error.message)")
                }
            }
            return json(inputStateJSON())
        case "POST input/release":
            releaseAllInput()
            return json(inputStateJSON())
        case "GET input/accelerometer":
            return json(inputStateJSON())
        case "PUT input/accelerometer":
            try setAcceleration(body)
            return json(inputStateJSON())
        case "DELETE input/accelerometer":
            clearAcceleration()
            return json(inputStateJSON())

        case "GET screenshot":
            return try screenshot(request.query)
        case "GET log":
            return try log(request.query)
        case "POST wait":
            return json(try await wait(body))

        case "GET config":
            return json(EKA2L1Bridge.shared.currentConfigSnapshot())
        case "PATCH config":
            guard EKA2L1Bridge.shared.applyConfigSnapshot(body) else {
                throw APIError.failed("config_rejected", "the emulator rejected the config change")
            }
            return json(EKA2L1Bridge.shared.currentConfigSnapshot())
        case "GET language":
            return json(languageJSON())
        case "PUT language":
            let code = try intField(body, "code")
            guard EKA2L1Bridge.shared.availableLanguages().contains(where: { $0.code == code }) else {
                throw APIError.badRequest("language \(code) is not shipped by this device")
            }
            EKA2L1Bridge.shared.setSystemLanguage(code: code)
            NotificationCenter.default.post(name: .eka2l1AppListInvalidated, object: nil)
            return json(languageJSON())

        case "GET files", "PUT files", "DELETE files":
            return try files(request)

        case "POST emulator/pause":
            EKA2L1Bridge.shared.pause()
            return json(["paused": true])
        case "POST emulator/resume":
            EKA2L1Bridge.shared.resume()
            return json(["paused": false])

        case "GET jobs":
            return json(["jobs": jobs.keys.sorted().compactMap { jobs[$0]?.json }])
        case "GET jobs/*":
            guard let job = Int(id).flatMap({ jobs[$0] }) else { throw APIError.notFound("no job \(id)") }
            return json(job.json)

        default:
            throw APIError.notFound("no route for \(request.method) /v1/\(path.joined(separator: "/")); see GET /v1")
        }
    }

    private func endpointIndex() -> JSON {
        [
            "api": Self.apiVersion,
            "endpoints": [
                "GET /v1/status",
                "GET /v1/devices", "POST /v1/devices", "POST /v1/devices/rescan",
                "POST /v1/devices/{index|code}/boot", "PATCH /v1/devices/{index|code}", "DELETE /v1/devices/{index|code}",
                "GET /v1/apps?system=1", "POST /v1/apps", "DELETE /v1/apps/{uid}", "GET /v1/apps/{uid}/icon",
                "POST /v1/apps/{uid}/launch",
                "GET /v1/session", "DELETE /v1/session",
                "GET|PUT /v1/session/screen-mode", "GET|PUT /v1/session/frame-limit",
                "GET|PUT|DELETE /v1/card",
                "GET /v1/keys", "POST /v1/input/key", "POST /v1/input/touch", "POST /v1/input/text",
                "POST /v1/input/sequence", "POST /v1/input/release", "GET|PUT|DELETE /v1/input/accelerometer",
                "GET /v1/screenshot", "GET /v1/log", "POST /v1/wait",
                "GET|PATCH /v1/config", "GET|PUT /v1/language",
                "GET|PUT|DELETE /v1/files?path=",
                "POST /v1/emulator/pause", "POST /v1/emulator/resume",
                "GET /v1/jobs", "GET /v1/jobs/{id}",
            ],
        ]
    }

    // MARK: Helpers

    private func parseBody(_ request: HTTPRequest) throws -> JSON {
        guard !request.body.isEmpty, request.path.last != "files" else { return [:] }
        guard let object = try? JSONSerialization.jsonObject(with: request.body), let dict = object as? JSON else {
            throw APIError.badRequest("body must be a JSON object")
        }
        return dict
    }

    private func requireStore() throws -> DeviceStore {
        guard let store else { throw APIError(status: 503, code: "not_ready", message: "the emulator has not started") }
        return store
    }

    private func requireIdleStore() throws -> DeviceStore {
        guard let store = try requireStore().ifIdle() else { throw APIError.busy }
        return store
    }

    private func requireBootedStore() throws -> DeviceStore {
        let store = try requireIdleStore()
        guard store.currentIndex >= 0 else { throw APIError.noDevice }
        return store
    }

    private func nullable(_ value: Any?) -> Any { value ?? NSNull() }

    private func hex(_ uid: UInt32) -> String { String(format: "0x%08X", uid) }

    private func parseUID(_ text: String) throws -> UInt32 {
        let trimmed = text.trimmingCharacters(in: .whitespaces).lowercased()
        let value = trimmed.hasPrefix("0x") ? UInt32(trimmed.dropFirst(2), radix: 16) : UInt32(trimmed)
        guard let value else { throw APIError.badRequest("`\(text)` is not a UID (decimal or 0x hex)") }
        return value
    }

    private func boolParam(_ value: String?) -> Bool? {
        guard let value = value?.lowercased() else { return nil }
        return ["1", "true", "yes", ""].contains(value)
    }

    private func intField(_ body: JSON, _ key: String) throws -> Int {
        if let number = body[key] as? NSNumber { return number.intValue }
        throw APIError.badRequest("`\(key)` must be an integer")
    }

    private func stringField(_ body: JSON, _ key: String) throws -> String {
        guard let value = body[key] as? String, !value.isEmpty else { throw APIError.badRequest("`\(key)` is required") }
        return value
    }

    private func doubleValue(_ value: Any?) -> Double? {
        (value as? NSNumber)?.doubleValue
    }

    private func sleep(ms: Double) async {
        guard ms > 0 else { return }
        try? await Task.sleep(nanoseconds: UInt64(ms * 1_000_000))
    }

    // Polls on the main actor; returns false on timeout.
    private func waitUntil(timeoutMs: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(timeoutMs / 1000)
        while !condition() {
            if Date() >= deadline { return false }
            await sleep(ms: 50)
        }
        return true
    }

    // MARK: Status

    private func status() -> JSON {
        let store = self.store
        var result: JSON = [
            "api": Self.apiVersion,
            "started": store != nil,
            "busy": store?.busy ?? false,
            "device": nullable(currentDeviceJSON()),
            "card": cardJSON(),
            "session": sessionJSON(),
            "frames": EKA2L1Bridge.shared.renderedFrameCount(),
            "runningJobs": jobs.values.filter { $0.state == "running" }.map(\.id).sorted(),
            "documents": documentsRoot(),
            "log": logPath(),
        ]
        if let screen = screenJSON() { result["screen"] = screen }
        return result
    }

    private func deviceJSON(_ device: EKA2L1DeviceItem) -> JSON {
        [
            "index": device.index,
            "code": device.firmwareCode,
            "manufacturer": device.manufacturer,
            "model": device.model,
            "name": device.displayName,
            "current": device.index == store?.currentIndex,
        ]
    }

    private func devicesJSON() -> [JSON] {
        (store?.devices ?? EKA2L1Bridge.shared.installedDevices()).map(deviceJSON)
    }

    private func currentDeviceJSON() -> JSON? {
        guard let store, store.currentIndex >= 0, let device = store.currentDevice else { return nil }
        var result = deviceJSON(device)
        result["touchScreen"] = EKA2L1Bridge.shared.currentDeviceIsTouchScreen()
        result["eka1"] = store.deviceIsEKA1
        return result
    }

    private func resolveDevice(_ id: String) throws -> EKA2L1DeviceItem {
        let store = try requireStore()
        if let index = Int(id), let device = store.devices.first(where: { $0.index == index }) {
            return device
        }
        if let device = store.device(withFirmwareCode: id) { return device }
        throw APIError.notFound("no device `\(id)` (use an index or firmware code from GET /v1/devices)")
    }

    private func appsJSON(includeSystem: Bool) -> [JSON] {
        (store?.apps ?? []).filter { includeSystem || !$0.system }.map {
            ["uid": hex($0.uid), "name": $0.name, "system": $0.system]
        }
    }

    private func cardJSON() -> JSON {
        [
            "mounted": EKA2L1Bridge.shared.isGameCardMounted(),
            "name": store?.mountedCardName ?? NSNull(),
        ]
    }

    // MARK: Devices

    private func installDevice(_ body: JSON, job: Job) async throws -> Any {
        let store = try requireIdleStore()
        let isolate = body["isolateDrives"] as? Bool ?? false
        let boot = body["boot"] as? Bool ?? true
        let progress: @Sendable (Double) -> Void = { fraction in
            Task { @MainActor in job.progress = fraction }
        }

        let result: EKA2L1InstallResult
        if let archive = body["archive"] as? String {
            result = await store.perform {
                EKA2L1Bridge.installDevice(archivePath: archive, isolateDrives: isolate, progress: progress)
            }
        } else if let rom = body["rom"] as? String {
            let rpkg = body["rpkg"] as? String
            result = await store.perform {
                EKA2L1Bridge.installDevice(romPath: rom, rpkgPath: rpkg, isolateDrives: isolate, progress: progress)
            }
        } else {
            throw APIError.badRequest("pass `rom` (+ optional `rpkg`) or `archive` as host paths")
        }
        guard result == .success else {
            throw APIError.failed("install_failed", "device install failed: \(Self.installResultName(result)) (\(result.rawValue))")
        }
        if boot {
            guard await store.bootNewestDevice() else {
                throw APIError.failed("boot_failed", "installed, but booting the new device failed")
            }
        } else {
            store.reloadDevices()
        }
        return ["devices": devicesJSON(), "current": nullable(currentDeviceJSON())]
    }

    private static func installResultName(_ result: EKA2L1InstallResult) -> String {
        let names = ["success", "not_exist", "insufficient", "rpkg_corrupt", "determine_product_failure",
                     "already_exist", "general_failure", "rom_fail_to_copy", "vpl_invalid", "rofs_corrupt",
                     "rom_corrupt", "fpsx_corrupt", "need_rpkg", "cancelled", "archive_corrupt", "archive_no_device"]
        return result.rawValue >= 0 && result.rawValue < names.count ? names[result.rawValue] : "unknown"
    }

    private func rescanDevices() async throws -> Any {
        let store = try requireIdleStore()
        await store.rescanDevices()
        return ["devices": devicesJSON(), "current": nullable(currentDeviceJSON())]
    }

    private func bootDevice(_ id: String) async throws -> Any {
        let store = try requireIdleStore()
        let device = try resolveDevice(id)
        try await closeScreenIfNeeded()
        guard await store.boot(at: device.index) else {
            throw APIError.failed("boot_failed", "booting \(device.firmwareCode) failed")
        }
        return nullable(currentDeviceJSON())
    }

    private func renameDevice(_ id: String, _ body: JSON) throws -> Any {
        let store = try requireIdleStore()
        let device = try resolveDevice(id)
        guard store.renameDevice(device, to: try stringField(body, "name")) else {
            throw APIError.failed("rename_failed", "rename of \(device.firmwareCode) failed")
        }
        return try deviceJSON(resolveDevice(device.firmwareCode))
    }

    private func deleteDevice(_ id: String) async throws -> Any {
        let store = try requireIdleStore()
        let device = try resolveDevice(id)
        guard let offset = store.devices.firstIndex(of: device) else { throw APIError.notFound("device vanished") }
        if device.index == store.currentIndex { try await closeScreenIfNeeded() }
        await store.deleteDevices(at: IndexSet(integer: offset))
        return ["devices": devicesJSON(), "current": nullable(currentDeviceJSON())]
    }

    // MARK: Apps

    // Dispatches on `type` or the path: SIS packages install, .n-gage files are
    // staged onto E:\n-gage, and folders/archives install as N-Gage 1.0 cards.
    private func installApps(_ body: JSON) async throws -> Any {
        let store = try requireBootedStore()
        let paths: [String]
        if let list = body["paths"] as? [String] {
            paths = list
        } else {
            paths = [try stringField(body, "path")]
        }
        let forcedType = body["type"] as? String
        let before = Set(store.apps.map(\.uid))
        var results: [JSON] = []

        for path in paths {
            guard FileManager.default.fileExists(atPath: path) else {
                throw APIError.notFound("no file at \(path) (paths are host paths)")
            }
            let ext = (path as NSString).pathExtension.lowercased()
            let type = forcedType ?? (["sis", "sisx"].contains(ext) ? "sis" : ext == "n-gage" ? "ngage2" : "ngage")
            switch type {
            case "sis":
                let ok = await store.perform { EKA2L1Bridge.installSis(atPath: path) }
                results.append(["path": path, "type": type, "ok": ok])
            case "ngage":
                let report = await store.perform { EKA2L1Bridge.installNGageGame(cardPath: path) }
                results.append(["path": path, "type": type, "ok": report.succeeded,
                                "result": report.result, "gameName": report.gameName])
            case "ngage2":
                let outcome = await store.perform { () -> String? in
                    let driveE = EKA2L1Bridge.currentDeviceDriveEPath()
                        ?? (documentsRoot() as NSString).appendingPathComponent("data/drives/e")
                    let dir = (driveE as NSString).appendingPathComponent("n-gage")
                    let dst = (dir as NSString).appendingPathComponent((path as NSString).lastPathComponent)
                    do {
                        try FileManager.default.createDirectory(atPath: dir, withIntermediateDirectories: true)
                        if FileManager.default.fileExists(atPath: dst) { try FileManager.default.removeItem(atPath: dst) }
                        try FileManager.default.copyItem(atPath: path, toPath: dst)
                        return nil
                    } catch {
                        return error.localizedDescription
                    }
                }
                var entry: JSON = ["path": path, "type": type, "ok": outcome == nil]
                if let outcome { entry["error"] = outcome }
                results.append(entry)
            default:
                throw APIError.badRequest("unknown `type` \(type); use sis, ngage or ngage2")
            }
        }
        store.reloadApps()
        let added = store.apps.filter { !before.contains($0.uid) }.map { ["uid": hex($0.uid), "name": $0.name] }
        let failed = results.filter { ($0["ok"] as? Bool) != true }.count
        let result: JSON = ["results": results, "added": added]
        if failed > 0 {
            throw APIError.failed("install_failed", "\(failed) of \(results.count) installs failed: \(String(data: Self.encode(results), encoding: .utf8) ?? "")")
        }
        return result
    }

    private func uninstallApp(_ text: String) throws -> Any {
        let store = try requireBootedStore()
        let uid = try parseUID(text)
        guard let app = store.apps.first(where: { $0.uid == uid }) else { throw APIError.notFound("no app \(hex(uid))") }
        guard !app.system else { throw APIError.conflict("system_app", "ROM apps cannot be uninstalled") }
        if sessionUID == uid { throw APIError.conflict("running", "close the app first (DELETE /v1/session)") }
        guard EKA2L1Bridge.shared.uninstallApp(uid: uid) else { throw APIError.failed("uninstall_failed", "uninstall of \(hex(uid)) failed") }
        store.reloadApps()
        return ["uninstalled": hex(uid)]
    }

    private func appIcon(_ text: String, size: UInt) throws -> HTTPResponse {
        let uid = try parseUID(text)
        guard let data = EKA2L1Bridge.iconPNGData(uid: uid, sizePx: size) else { throw APIError.notFound("no icon for \(hex(uid))") }
        return HTTPResponse(status: 200, contentType: "image/png", body: data)
    }

    // MARK: Session

    private func renderView() -> UIView? {
        func find(_ view: UIView) -> UIView? {
            if view.layer is CAEAGLLayer, view.window != nil, !view.isHidden { return view }
            for subview in view.subviews {
                if let found = find(subview) { return found }
            }
            return nil
        }
        for scene in UIApplication.shared.connectedScenes {
            guard let scene = scene as? UIWindowScene, scene.screen === UIScreen.main else { continue }
            for window in scene.windows {
                if let view = find(window) { return view }
            }
        }
        return nil
    }

    private var screenVisible: Bool { renderView() != nil }

    private func effectiveSessionState() -> String {
        if sessionState == "running" && !screenVisible { return "closed" }
        return sessionState
    }

    private func sessionJSON() -> JSON {
        var result: JSON = ["state": effectiveSessionState(), "screenVisible": screenVisible]
        if let uid = sessionUID {
            result["uid"] = hex(uid)
            result["name"] = store?.apps.first(where: { $0.uid == uid })?.name ?? NSNull()
        }
        if let lastExit { result["lastExit"] = lastExit }
        return result
    }

    private func runningUID() throws -> UInt32 {
        guard let uid = sessionUID, effectiveSessionState() == "running" else {
            throw APIError.conflict("not_running", "no app is running")
        }
        return uid
    }

    private func closeScreenIfNeeded() async throws {
        guard screenVisible else { return }
        NotificationCenter.default.post(name: .eka2l1RemoteCloseApp, object: nil)
        guard await waitUntil(timeoutMs: 5000, { !screenVisible }) else {
            throw APIError(status: 500, code: "close_timeout", message: "the emulator screen did not close")
        }
        if sessionState == "running" || sessionState == "launching" { sessionState = "closed" }
        releaseAllInput()
        // Let the navigation pop settle before pushing anything else.
        await sleep(ms: 400)
    }

    private func launchApp(_ text: String, _ body: JSON, _ request: HTTPRequest) async throws -> Any {
        let store = try requireBootedStore()
        let uid = try parseUID(text)
        guard store.apps.contains(where: { $0.uid == uid }) else { throw APIError.notFound("no app \(hex(uid)) on this device") }
        try await closeScreenIfNeeded()

        sessionUID = uid
        sessionState = "launching"
        NotificationCenter.default.post(name: .eka2l1RemoteLaunchApp, object: nil, userInfo: ["uid": uid])
        let wait = boolParam(request.query["wait"]) ?? (body["wait"] as? Bool ?? true)
        if wait {
            let timeout = doubleValue(body["timeoutMs"]) ?? 30000
            guard await waitUntil(timeoutMs: timeout, { sessionState != "launching" }) else {
                throw APIError(status: 504, code: "timeout", message: "launch of \(hex(uid)) did not complete")
            }
            if sessionState == "launch_failed" { throw APIError.failed("launch_failed", "launch of \(hex(uid)) failed") }
        }
        return sessionJSON()
    }

    private func closeApp() async throws -> Any {
        guard screenVisible else { throw APIError.noScreen }
        try await closeScreenIfNeeded()
        return sessionJSON()
    }

    private func screenJSON() -> JSON? {
        let snapshot = EKA2L1Bridge.shared.guestScreenModeSnapshotRaw()
        let rect = EKA2L1Bridge.shared.guestDisplayRect
        guard screenVisible || !rect.isEmpty else { return nil }
        return [
            "width": (snapshot["width"] as? NSNumber)?.intValue ?? 0,
            "height": (snapshot["height"] as? NSNumber)?.intValue ?? 0,
            "mode": (snapshot["current"] as? NSNumber)?.intValue ?? -1,
            "modes": snapshot["modes"] ?? [],
            "displayRect": ["x": rect.minX, "y": rect.minY, "width": rect.width, "height": rect.height],
        ]
    }

    private func screenModeJSON() -> JSON {
        let snapshot = EKA2L1Bridge.shared.guestScreenModeSnapshot()
        return ["modes": snapshot.modes, "current": snapshot.current]
    }

    private func setScreenMode(_ body: JSON) async throws -> Any {
        let uid = try runningUID()
        let mode = try intField(body, "mode")
        let selected: Int = await withCheckedContinuation { continuation in
            EKA2L1Bridge.shared.setGuestScreenMode(appUID: uid, mode: mode) { continuation.resume(returning: $0) }
        }
        guard selected >= 0 else { throw APIError.failed("screen_mode_failed", "mode \(mode) was not applied") }
        return ["modes": EKA2L1Bridge.shared.guestScreenModeSnapshot().modes, "current": selected]
    }

    // MARK: Card

    private func mountCard(_ body: JSON) async throws -> Any {
        let store = try requireBootedStore()
        let path = try stringField(body, "path")
        guard FileManager.default.fileExists(atPath: path) else { throw APIError.notFound("no card at \(path)") }
        try await closeScreenIfNeeded()
        let report = await store.mountCard(at: URL(fileURLWithPath: path))
        guard report.succeeded else {
            let names: [EKA2L1MountResult: String] = [.failed: "failed", .pathNotFound: "path_not_found",
                                                      .notArchive: "not_archive", .noSystemFolder: "no_system_folder",
                                                      .archiveCorrupt: "archive_corrupt"]
            throw APIError.failed("mount_failed", "card mount failed: \(names[report.result] ?? "unknown")")
        }
        var result = cardJSON()
        result["mmcId"] = report.mmcId
        result["apps"] = appsJSON(includeSystem: false)
        return result
    }

    // MARK: Input

    static let keyNames: [String: UInt32] = {
        var keys: [String: UInt32] = [
            "lsk": Scan.leftSoft, "softleft": Scan.leftSoft, "menu": Scan.leftSoft,
            "rsk": Scan.rightSoft, "softright": Scan.rightSoft, "back": Scan.rightSoft,
            "ok": Scan.select, "select": Scan.select, "center": Scan.select,
            "up": Scan.up, "down": Scan.down, "left": Scan.left, "right": Scan.right,
            "clear": Scan.clear, "backspace": Scan.clear,
            "call": Scan.call, "green": Scan.call, "end": Scan.end, "red": Scan.end,
            "edit": Scan.edit, "shift": Scan.edit,
            "star": Scan.star, "*": Scan.star, "hash": Scan.hash, "#": Scan.hash,
            "space": 0x05, "tab": 0x02, "enter": 0x03, "escape": 0x04,
            "yes": 0xC4, "no": 0xC5, "applications": 0x94,
        ]
        for digit in 0...9 { keys[String(digit)] = UInt32(0x30 + digit) }
        for letter in UnicodeScalar("a").value...UnicodeScalar("z").value {
            keys[String(UnicodeScalar(letter)!)] = letter - 0x20
        }
        return keys
    }()

    private func scanCode(_ value: Any?) throws -> UInt32 {
        if let number = value as? NSNumber { return number.uint32Value }
        guard let text = (value as? String)?.lowercased() else { throw APIError.badRequest("`key` must be a name or scan code") }
        if let scan = Self.keyNames[text] { return scan }
        if text.hasPrefix("0x"), let scan = UInt32(text.dropFirst(2), radix: 16) { return scan }
        throw APIError.badRequest("unknown key `\(text)`; see GET /v1/keys")
    }

    private func setKey(_ scan: UInt32, pressed: Bool) {
        if pressed { heldKeys.insert(scan) } else { heldKeys.remove(scan) }
        EKA2L1Bridge.shared.submitRawKey(scan, pressed: pressed)
    }

    private func releaseAllInput() {
        for scan in heldKeys { EKA2L1Bridge.shared.submitRawKey(scan, pressed: false) }
        heldKeys.removeAll()
        for id in heldPointers { EKA2L1Bridge.shared.submitPointer(x: 0, y: 0, phase: .cancelled, pointerId: pointerID(id)) }
        heldPointers.removeAll()
        if acceleration != nil { clearAcceleration() }
    }

    private func inputStateJSON() -> JSON {
        [
            "heldKeys": heldKeys.sorted().map { hex($0) },
            "heldPointers": heldPointers.sorted(),
            "acceleration": nullable(acceleration.map { ["x": $0[0], "y": $0[1], "z": $0[2]] }),
        ]
    }

    private func setAcceleration(_ body: JSON) throws {
        let scale: Double
        switch (body["unit"] as? String) ?? "ms2" {
        case "ms2": scale = 1
        case "g": scale = 9.80665
        default: throw APIError.badRequest("accelerometer `unit` must be ms2 or g")
        }
        let values = try ["x", "y", "z"].map { axis -> Double in
            guard let value = doubleValue(body[axis]), value.isFinite else {
                throw APIError.badRequest("accelerometer needs numeric `x`, `y` and `z`")
            }
            return value * scale
        }
        acceleration = values
        EKA2L1Bridge.shared.setAccelerationOverride(values)
    }

    // Hands the guest back to the host's motion (on the simulator, a phone lying flat).
    private func clearAcceleration() {
        acceleration = nil
        EKA2L1Bridge.shared.setAccelerationOverride(nil)
    }

    private func pointerID(_ id: Int) -> UInt { 0xEC0_0000 + UInt(max(0, id)) }

    // Kind is the endpoint (`key`/`touch`/`text`), or nil for a sequence step,
    // which names itself by its fields.
    private func runInputStep(_ step: JSON, kind: String?) async throws {
        let kind = kind ?? (step["key"] != nil || step["keys"] != nil ? "key"
                            : step["touch"] != nil ? "touch"
                            : step["text"] != nil ? "text"
                            : step["accelerometer"] != nil ? "accelerometer"
                            : step["waitMs"] != nil ? "wait" : "")
        switch kind {
        case "key":
            try await runKey(step)
        case "touch":
            try await runTouch((step["touch"] as? JSON) ?? step)
        case "text":
            try await runText(step)
        case "accelerometer":
            if step["accelerometer"] is NSNull {
                clearAcceleration()
            } else {
                guard let values = step["accelerometer"] as? JSON else {
                    throw APIError.badRequest("`accelerometer` must be an object or null")
                }
                try setAcceleration(values)
            }
        case "wait":
            break
        default:
            throw APIError.badRequest("a step needs `key`, `keys`, `touch`, `text`, `accelerometer` or `waitMs`")
        }
        await sleep(ms: doubleValue(step["waitMs"]) ?? 0)
    }

    private func runKey(_ step: JSON) async throws {
        let raw: [Any] = (step["keys"] as? [Any]) ?? [step["key"] as Any]
        let scans = try raw.map(scanCode)
        let action = (step["action"] as? String) ?? "tap"
        let holdMs = doubleValue(step["holdMs"]) ?? 80
        let repeatCount = max(1, (step["repeat"] as? NSNumber)?.intValue ?? 1)
        let gapMs = doubleValue(step["gapMs"]) ?? 80
        switch action {
        case "down":
            scans.forEach { setKey($0, pressed: true) }
        case "up":
            scans.forEach { setKey($0, pressed: false) }
        case "tap":
            for index in 0..<repeatCount {
                if index > 0 { await sleep(ms: gapMs) }
                scans.forEach { setKey($0, pressed: true) }
                await sleep(ms: holdMs)
                scans.reversed().forEach { setKey($0, pressed: false) }
            }
        default:
            throw APIError.badRequest("key `action` must be tap, down or up")
        }
    }

    // Letters and digits map to their scan codes; the guest's own input method
    // decides what they produce (multitap on ITU-T devices).
    private func runText(_ step: JSON) async throws {
        let text = try stringField(step, "text")
        let interval = doubleValue(step["intervalMs"]) ?? 120
        for character in text.lowercased() {
            let name = character == " " ? "space" : character == "\n" ? "enter" : String(character)
            guard let scan = Self.keyNames[name] else { throw APIError.badRequest("no key for `\(character)`") }
            setKey(scan, pressed: true)
            await sleep(ms: 60)
            setKey(scan, pressed: false)
            await sleep(ms: interval)
        }
    }

    // Maps a point to render-surface pixels. `guest` (default) uses the guest
    // screen's own pixels, as in GET /v1/screenshot; `normalized` is 0…1.
    private func surfacePoint(_ x: Double, _ y: Double, space: String) throws -> CGPoint {
        if space == "surface" { return CGPoint(x: x, y: y) }
        let rect = EKA2L1Bridge.shared.guestDisplayRect
        guard !rect.isEmpty else { throw APIError.noScreen }
        var (nx, ny) = (x, y)
        if space == "guest" {
            let snapshot = EKA2L1Bridge.shared.guestScreenModeSnapshotRaw()
            let width = (snapshot["width"] as? NSNumber)?.doubleValue ?? 0
            let height = (snapshot["height"] as? NSNumber)?.doubleValue ?? 0
            guard width > 0, height > 0 else { throw APIError.conflict("screen_busy", "guest screen size unavailable; retry") }
            nx = (x + 0.5) / width
            ny = (y + 0.5) / height
        } else if space != "normalized" {
            throw APIError.badRequest("`space` must be guest, normalized or surface")
        }
        return CGPoint(x: rect.minX + nx * rect.width, y: rect.minY + ny * rect.height)
    }

    private func point(_ object: JSON, xKey: String = "x", yKey: String = "y") throws -> (Double, Double) {
        guard let x = doubleValue(object[xKey]), let y = doubleValue(object[yKey]) else {
            throw APIError.badRequest("`\(xKey)` and `\(yKey)` are required numbers")
        }
        return (x, y)
    }

    private func pointer(_ id: Int, _ location: CGPoint, _ phase: EKA2L1PointerPhase) {
        switch phase {
        case .began: heldPointers.insert(id)
        case .ended, .cancelled: heldPointers.remove(id)
        default: break
        }
        EKA2L1Bridge.shared.submitPointer(x: location.x, y: location.y, phase: phase, pointerId: pointerID(id))
    }

    private func runTouch(_ touch: JSON) async throws {
        let action = (touch["action"] as? String) ?? "tap"
        let space = (touch["space"] as? String) ?? "guest"
        let id = (touch["id"] as? NSNumber)?.intValue ?? 0
        switch action {
        case "tap":
            let (x, y) = try point(touch)
            let location = try surfacePoint(x, y, space: space)
            pointer(id, location, .began)
            await sleep(ms: doubleValue(touch["holdMs"]) ?? 80)
            pointer(id, location, .ended)
        case "down", "move", "up":
            let (x, y) = try point(touch)
            let phase: EKA2L1PointerPhase = action == "down" ? .began : action == "move" ? .moved : .ended
            pointer(id, try surfacePoint(x, y, space: space), phase)
        case "swipe":
            let (x0, y0) = try point(touch)
            let (x1, y1) = try point(touch, xKey: "toX", yKey: "toY")
            let duration = doubleValue(touch["durationMs"]) ?? 300
            let steps = max(2, Int(duration / 16))
            pointer(id, try surfacePoint(x0, y0, space: space), .began)
            for step in 1...steps {
                await sleep(ms: duration / Double(steps))
                let t = Double(step) / Double(steps)
                pointer(id, try surfacePoint(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, space: space), .moved)
            }
            pointer(id, try surfacePoint(x1, y1, space: space), .ended)
        default:
            throw APIError.badRequest("touch `action` must be tap, down, move, up or swipe")
        }
    }

    // MARK: Screenshot

    // region=guest (default while the emulator screen is up) renders the guest
    // picture at its own resolution times `scale`; region=screen renders the
    // whole host window in points times `scale`.
    private func screenshot(_ query: [String: String]) throws -> HTTPResponse {
        let scale = max(0.1, min(8, Double(query["scale"] ?? "") ?? 1))
        let view = renderView()
        let region = query["region"] ?? (view != nil ? "guest" : "screen")
        let image: UIImage
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true

        switch region {
        case "guest":
            guard let view else { throw APIError.noScreen }
            let layerScale = max(view.layer.contentsScale, 0.01)
            let rect = EKA2L1Bridge.shared.guestDisplayRect
            guard !rect.isEmpty else { throw APIError.conflict("no_frame", "no frame presented yet") }
            let snapshot = EKA2L1Bridge.shared.guestScreenModeSnapshotRaw()
            var size = CGSize(width: (snapshot["width"] as? NSNumber)?.doubleValue ?? 0,
                              height: (snapshot["height"] as? NSNumber)?.doubleValue ?? 0)
            if size.width <= 0 || size.height <= 0 { size = rect.size }
            size = CGSize(width: (size.width * scale).rounded(), height: (size.height * scale).rounded())
            // Capture at the surface's own resolution and scale afterwards: drawHierarchy
            // returns a black image once the view is drawn a few times larger than itself.
            let surface = UIGraphicsImageRenderer(size: rect.size, format: format).image { _ in
                view.drawHierarchy(in: CGRect(x: -rect.minX, y: -rect.minY,
                                              width: view.bounds.width * layerScale,
                                              height: view.bounds.height * layerScale),
                                   afterScreenUpdates: false)
            }
            image = UIGraphicsImageRenderer(size: size, format: format).image { _ in
                surface.draw(in: CGRect(origin: .zero, size: size))
            }
        case "screen":
            guard let window = UIApplication.shared.connectedScenes.compactMap({ $0 as? UIWindowScene })
                .flatMap(\.windows).first(where: \.isKeyWindow) else { throw APIError.noScreen }
            let size = CGSize(width: (window.bounds.width * scale).rounded(), height: (window.bounds.height * scale).rounded())
            image = UIGraphicsImageRenderer(size: size, format: format).image { _ in
                window.drawHierarchy(in: CGRect(origin: .zero, size: size), afterScreenUpdates: false)
            }
        default:
            throw APIError.badRequest("`region` must be guest or screen")
        }
        guard let data = image.pngData() else { throw APIError(status: 500, code: "encode_failed", message: "PNG encode failed") }
        return HTTPResponse(status: 200, contentType: "image/png", body: data,
                            headers: ["X-Frame-Count": String(EKA2L1Bridge.shared.renderedFrameCount())])
    }

    // MARK: Log

    private func logPath() -> String {
        (documentsRoot() as NSString).appendingPathComponent("data/EKA2L1.log")
    }

    private func logSize() -> UInt64 {
        ((try? FileManager.default.attributesOfItem(atPath: logPath()))?[.size] as? NSNumber)?.uint64Value ?? 0
    }

    private func readLog(from offset: UInt64, maxBytes: UInt64) -> (Data, UInt64) {
        guard let handle = FileHandle(forReadingAtPath: logPath()) else { return (Data(), 0) }
        defer { try? handle.close() }
        let end = (try? handle.seekToEnd()) ?? 0
        let start = min(offset, end)
        let clampedStart = end - start > maxBytes ? end - maxBytes : start
        try? handle.seek(toOffset: clampedStart)
        return ((try? handle.readToEnd()) ?? Data(), end)
    }

    // ?since=<offset> continues from a previous X-Log-Offset (a log that shrank
    // restarts from 0); ?tail=<lines> (default 200); ?grep=<regex> filters lines.
    private func log(_ query: [String: String]) throws -> HTTPResponse {
        let size = logSize()
        var offset: UInt64
        if let since = query["since"].flatMap(UInt64.init) {
            offset = since > size ? 0 : since
        } else {
            offset = size > 4 << 20 ? size - (4 << 20) : 0
        }
        let (data, end) = readLog(from: offset, maxBytes: 16 << 20)
        offset = end
        var lines = String(decoding: data, as: UTF8.self).components(separatedBy: "\n")
        if lines.last == "" { lines.removeLast() }
        if let pattern = query["grep"], !pattern.isEmpty {
            guard let regex = try? NSRegularExpression(pattern: pattern) else { throw APIError.badRequest("invalid `grep` regex") }
            lines = lines.filter { regex.firstMatch(in: $0, range: NSRange($0.startIndex..., in: $0)) != nil }
        }
        if query["since"] == nil || query["tail"] != nil {
            let tail = Int(query["tail"] ?? "") ?? 200
            if tail > 0, lines.count > tail { lines = Array(lines.suffix(tail)) }
        }
        let text = lines.isEmpty ? "" : lines.joined(separator: "\n") + "\n"
        return HTTPResponse(status: 200, contentType: "text/plain; charset=utf-8", body: Data(text.utf8),
                            headers: ["X-Log-Offset": String(offset)])
    }

    // MARK: Wait

    private func wait(_ body: JSON) async throws -> Any {
        let until = try stringField(body, "until")
        let timeout = doubleValue(body["timeoutMs"]) ?? 30000
        let started = Date()
        let satisfied: Bool
        switch until {
        case "running":
            satisfied = await waitUntil(timeoutMs: timeout) { effectiveSessionState() == "running" }
        case "exited":
            satisfied = await waitUntil(timeoutMs: timeout) { !["running", "launching"].contains(effectiveSessionState()) }
        case "idle":
            satisfied = await waitUntil(timeoutMs: timeout) {
                !(store?.busy ?? true) && !jobs.values.contains { $0.state == "running" }
            }
        case "frames":
            let target = EKA2L1Bridge.shared.renderedFrameCount() + UInt64(max(1, (body["count"] as? NSNumber)?.intValue ?? 1))
            satisfied = await waitUntil(timeoutMs: timeout) { EKA2L1Bridge.shared.renderedFrameCount() >= target }
        case "log":
            let pattern = try stringField(body, "pattern")
            guard let regex = try? NSRegularExpression(pattern: pattern) else { throw APIError.badRequest("invalid `pattern` regex") }
            var offset = (body["since"] as? NSNumber)?.uint64Value ?? logSize()
            var match: String?
            satisfied = await waitUntil(timeoutMs: timeout) {
                let (data, end) = readLog(from: offset, maxBytes: 16 << 20)
                // Only scan complete lines; a partial tail is re-read next round.
                guard let newline = data.lastIndex(of: 0x0A) else { return false }
                offset = end - UInt64(data.count - newline - 1)
                for line in String(decoding: data[...newline], as: UTF8.self).split(separator: "\n") {
                    let text = String(line)
                    if regex.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)) != nil {
                        match = text
                        return true
                    }
                }
                return false
            }
            if satisfied {
                return ["satisfied": true, "elapsedMs": Int(Date().timeIntervalSince(started) * 1000),
                        "line": match ?? "", "logOffset": offset]
            }
        case "sleep":
            await sleep(ms: timeout)
            satisfied = true
        default:
            throw APIError.badRequest("`until` must be running, exited, idle, frames, log or sleep")
        }
        guard satisfied else {
            throw APIError(status: 504, code: "timeout", message: "`\(until)` not reached within \(Int(timeout))ms")
        }
        return ["satisfied": true, "elapsedMs": Int(Date().timeIntervalSince(started) * 1000), "status": status()]
    }

    // MARK: Language

    private func languageJSON() -> JSON {
        [
            "current": EKA2L1Bridge.shared.currentLanguageCode(),
            "available": EKA2L1Bridge.shared.availableLanguages().map { ["code": $0.code, "name": $0.name] },
        ]
    }

    // MARK: Files

    // `C:\...` / `C:/...` address the booted device's C, D or E drive (E is its
    // own storage, not a mounted card); `docs:/...` the app's Documents folder;
    // a bare absolute path the host. Guest-style paths match case-insensitively.
    private func hostPath(_ raw: String) throws -> String {
        let normalized = raw.replacingOccurrences(of: "\\", with: "/")
        if normalized.hasPrefix("/") { return normalized }
        let root: String
        let rest: String
        if normalized.lowercased().hasPrefix("docs:") {
            root = documentsRoot()
            rest = String(normalized.dropFirst(5))
        } else if normalized.count >= 2, normalized[normalized.index(after: normalized.startIndex)] == ":",
                  let letter = normalized.first?.lowercased().utf16.first {
            guard let drive = EKA2L1Emulator.shared().currentDeviceDrivePath(letter) else {
                throw APIError.badRequest("drive \(normalized.prefix(2)) is not available (C, D, E of a booted device)")
            }
            root = drive
            rest = String(normalized.dropFirst(2))
        } else {
            throw APIError.badRequest("`path` must be C:/…, docs:/… or an absolute host path")
        }
        var resolved = root
        for component in rest.split(separator: "/").map(String.init) where !component.isEmpty && component != "." {
            guard component != ".." else { throw APIError.badRequest("`..` is not allowed") }
            let exact = (resolved as NSString).appendingPathComponent(component)
            if FileManager.default.fileExists(atPath: exact) {
                resolved = exact
            } else if let match = (try? FileManager.default.contentsOfDirectory(atPath: resolved))?
                        .first(where: { $0.caseInsensitiveCompare(component) == .orderedSame }) {
                resolved = (resolved as NSString).appendingPathComponent(match)
            } else {
                resolved = exact
            }
        }
        return resolved
    }

    private func files(_ request: HTTPRequest) throws -> HTTPResponse {
        guard let raw = request.query["path"], !raw.isEmpty else { throw APIError.badRequest("`path` query parameter is required") }
        let path = try hostPath(raw)
        let manager = FileManager.default
        var isDirectory: ObjCBool = false
        let exists = manager.fileExists(atPath: path, isDirectory: &isDirectory)

        switch request.method {
        case "GET":
            guard exists else { throw APIError.notFound("no file at \(raw) (\(path))") }
            if isDirectory.boolValue {
                let names = (try? manager.contentsOfDirectory(atPath: path)) ?? []
                let entries: [JSON] = names.sorted().map { name in
                    let full = (path as NSString).appendingPathComponent(name)
                    let attributes = (try? manager.attributesOfItem(atPath: full)) ?? [:]
                    let directory = (attributes[.type] as? FileAttributeType) == .typeDirectory
                    var entry: JSON = ["name": name, "directory": directory]
                    if !directory { entry["size"] = (attributes[.size] as? NSNumber)?.intValue ?? 0 }
                    if let date = attributes[.modificationDate] as? Date {
                        entry["modified"] = ISO8601DateFormatter().string(from: date)
                    }
                    return entry
                }
                return json(["path": raw, "hostPath": path, "entries": entries])
            }
            guard let data = manager.contents(atPath: path) else { throw APIError(status: 500, code: "read_failed", message: "cannot read \(path)") }
            return HTTPResponse(status: 200, contentType: "application/octet-stream", body: data,
                                headers: ["X-Host-Path": path])
        case "PUT":
            guard !isDirectory.boolValue else { throw APIError.conflict("is_directory", "\(raw) is a directory") }
            do {
                try manager.createDirectory(atPath: (path as NSString).deletingLastPathComponent, withIntermediateDirectories: true)
                try request.body.write(to: URL(fileURLWithPath: path), options: .atomic)
            } catch {
                throw APIError(status: 500, code: "write_failed", message: error.localizedDescription)
            }
            return json(["path": raw, "hostPath": path, "size": request.body.count])
        default:
            guard exists else { throw APIError.notFound("no file at \(raw)") }
            guard boolParam(request.query["recursive"]) == true || !isDirectory.boolValue
                    || ((try? manager.contentsOfDirectory(atPath: path)) ?? []).isEmpty else {
                throw APIError.conflict("not_empty", "directory is not empty; pass recursive=1")
            }
            do {
                try manager.removeItem(atPath: path)
            } catch {
                throw APIError(status: 500, code: "delete_failed", message: error.localizedDescription)
            }
            return json(["deleted": raw, "hostPath": path])
        }
    }

    // MARK: Jobs

    @MainActor
    final class Job {
        let id: Int
        let kind: String
        var state = "running"
        var progress: Double?
        var result: Any?
        var error: APIError?
        let started = Date()
        var finished: Date?

        init(id: Int, kind: String) {
            self.id = id
            self.kind = kind
        }

        var json: JSON {
            var object: JSON = ["id": id, "kind": kind, "state": state,
                                "started": ISO8601DateFormatter().string(from: started)]
            if let progress { object["progress"] = progress }
            if let result { object["result"] = result }
            if let error { object["error"] = ["code": error.code, "message": error.message] }
            if let finished {
                object["finished"] = ISO8601DateFormatter().string(from: finished)
                object["elapsedMs"] = Int(finished.timeIntervalSince(started) * 1000)
            }
            return object
        }
    }

    // Long operations run as jobs. By default the request blocks until the job
    // ends; `?async=1` returns 202 with the job to poll at GET /v1/jobs/{id}.
    private func runJob(_ kind: String, _ request: HTTPRequest,
                        _ work: @escaping @MainActor (Job) async throws -> Any) async throws -> HTTPResponse {
        let job = Job(id: nextJobID, kind: kind)
        nextJobID += 1
        jobs[job.id] = job
        if jobs.count > 50, let oldest = jobs.keys.min() { jobs[oldest] = nil }

        let task = Task { @MainActor in
            do {
                job.result = try await work(job)
                job.state = "succeeded"
            } catch let error as APIError {
                job.error = error
                job.state = "failed"
            } catch {
                job.error = APIError(status: 500, code: "internal", message: String(describing: error))
                job.state = "failed"
            }
            job.finished = Date()
        }
        if boolParam(request.query["async"]) == true {
            return json(job.json, status: 202)
        }
        await task.value
        if let error = job.error {
            var response = Self.errorResponse(error)
            response.headers["X-Job-Id"] = String(job.id)
            return response
        }
        return json(job.result ?? NSNull())
    }
}

private extension DeviceStore {
    func ifIdle() -> DeviceStore? { busy ? nil : self }
}

private extension EKA2L1Bridge {
    func guestScreenModeSnapshotRaw() -> [String: Any] {
        EKA2L1Emulator.shared().guestScreenModeSnapshot()
    }
}

#endif
