import Foundation

extension XueHuaLaunchAtStartup {
    // MARK: - LaunchAgent Plist Serialization & Evaluation

    /// Builds a `launchd.plist(5)` dictionary for a GUI user `LaunchAgent`.
    static func buildLaunchAgentPlistDictionary(
        label: String,
        executablePath: String,
        arguments: [String]
    ) -> [String: Any] {
        var programArguments = [executablePath]
        programArguments.append(contentsOf: arguments)
        return [
            "Label": label,
            "ProgramArguments": programArguments,
            "RunAtLoad": true,
            "ProcessType": "Interactive",
        ]
    }

    /// Serializes a `LaunchAgent` dictionary into XML property list bytes.
    static func serializeLaunchAgentPlist(
        label: String,
        executablePath: String,
        arguments: [String]
    ) throws -> Data {
        let plist = buildLaunchAgentPlistDictionary(
            label: label,
            executablePath: executablePath,
            arguments: arguments
        )
        return try PropertyListSerialization.data(
            fromPropertyList: plist,
            format: .xml,
            options: 0
        )
    }

    /// Evaluates raw XML/binary `.plist` bytes from `~/Library/LaunchAgents`.
    static func evaluateLaunchAgentPlistData(
        _ data: Data?
    ) -> (status: String, executablePath: String?) {
        guard let data = data, !data.isEmpty else {
            return ("disabled", nil)
        }
        guard let rawPlist = try? PropertyListSerialization.propertyList(
            from: data,
            options: [],
            format: nil
        ),
            let dict = rawPlist as? [String: Any]
        else {
            return ("disabled", nil)
        }

        let programArgs = dict["ProgramArguments"] as? [String]
        let programPath = programArgs?.first ?? (dict["Program"] as? String)
        guard let resolvedProgram = programPath, !resolvedProgram.isEmpty else {
            return ("disabled", nil)
        }

        if (dict["Disabled"] as? Bool) == true {
            return ("disabledByUser", resolvedProgram)
        }
        if let runAtLoad = dict["RunAtLoad"] as? Bool, !runAtLoad {
            return ("disabled", resolvedProgram)
        }
        return ("enabled", resolvedProgram)
    }

    // MARK: - LaunchAgent File Operations (`~/Library/LaunchAgents`)

    static func defaultLaunchAgentsDirectory() -> URL {
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("LaunchAgents", isDirectory: true)
    }

    static func launchAgentFileURL(
        identifier: String,
        directory: URL? = nil
    ) -> URL {
        let dir = directory ?? defaultLaunchAgentsDirectory()
        return dir.appendingPathComponent("\(identifier).plist", isDirectory: false)
    }

    static func queryLaunchAgentState(
        identifier: String,
        fallbackExecutablePath: String?,
        launchAgentsDirectory: URL? = nil,
        canOpenSettings: Bool = true
    ) -> [String: Any] {
        let fileURL = launchAgentFileURL(identifier: identifier, directory: launchAgentsDirectory)
        let data = try? Data(contentsOf: fileURL)
        let evaluated = evaluateLaunchAgentPlistData(data)
        return makeStateMap(
            isSupported: true,
            status: evaluated.status,
            backend: .launchAgent,
            identifier: identifier,
            executablePath: evaluated.executablePath ?? fallbackExecutablePath,
            canOpenSettings: canOpenSettings
        )
    }

    static func setLaunchAgentEnabled(
        _ enabled: Bool,
        identifier: String,
        executablePath: String?,
        arguments: [String],
        launchAgentsDirectory: URL? = nil,
        canOpenSettings: Bool = true
    ) throws -> [String: Any] {
        let dir = launchAgentsDirectory ?? defaultLaunchAgentsDirectory()
        let fileURL = launchAgentFileURL(identifier: identifier, directory: dir)
        let fm = FileManager.default

        if enabled {
            guard let exe = executablePath, !exe.isEmpty else {
                throw NSError(
                    domain: "XueHuaLaunchAtStartup",
                    code: 2,
                    userInfo: [NSLocalizedDescriptionKey: "Unable to resolve application executable path for LaunchAgent."]
                )
            }
            try fm.createDirectory(at: dir, withIntermediateDirectories: true, attributes: nil)
            let data = try serializeLaunchAgentPlist(
                label: identifier,
                executablePath: exe,
                arguments: arguments
            )
            try data.write(to: fileURL, options: .atomic)
        } else {
            if fm.fileExists(atPath: fileURL.path) {
                try fm.removeItem(at: fileURL)
            }
        }

        return queryLaunchAgentState(
            identifier: identifier,
            fallbackExecutablePath: executablePath,
            launchAgentsDirectory: dir,
            canOpenSettings: canOpenSettings
        )
    }
}
