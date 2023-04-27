import AppKit
import Foundation
import NetworkExtension
import SystemExtensions

let app = NSApplication.shared

let START = "--start"
let STOP = "--stop"

let HELP_STRING = "usage: belnet [--start|--stop]"

class BelnetMain: NSObject, NSApplicationDelegate {
    var vpnManager = NETunnelProviderManager()
    var mode = START
    let netextBundleId = "org.belnet.network-extension"

    func applicationDidFinishLaunching(_: Notification) {
        
        if mode == START {
            startNetworkExtension()
        } else if mode == STOP {
            tearDownVPNTunnel()
        } else {
            result(msg: HELP_STRING)
        }
    }

    func bail() {
        app.terminate(self)
    }

    func result(msg: String) {
        NSLog(msg)
        // TODO: does belnet continue after this?
        bail()
    }

    func tearDownVPNTunnel() {
        NSLog("Stopping Belnet")
        NETunnelProviderManager.loadAllFromPreferences { [self] (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                self.result(msg: error.localizedDescription)
                return
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.netextBundleId {
                        manager.isEnabled = false
                        self.result(msg: "Belnet Down")
                        return
                    }
                }
            }
            self.result(msg: "Belnet is not up")
        }
    }

    func startNetworkExtension() {
        #if MACOS_SYSTEM_EXTENSION
            NSLog("Loading Belnet network extension")
            // Start by activating the system extension
            let activationRequest = OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: netextBundleId, queue: .main)
            activationRequest.delegate = self
            OSSystemExtensionManager.shared.submitRequest(activationRequest)
        #else
            setupVPNTunnel()
        #endif
    }

    func setupVPNTunnel() {
        NSLog("Starting up Belnet tunnel")
        NETunnelProviderManager.loadAllFromPreferences { [self] (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                self.result(msg: error.localizedDescription)
                return
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.netextBundleId {
                        NSLog("Found saved VPN Manager")
                        self.vpnManager = manager
                    }
                }
            }
            let providerProtocol = NETunnelProviderProtocol()
            providerProtocol.serverAddress = "beldex.bdx" // Needs to be set to some non-null dummy value
            providerProtocol.username = "anonymous"
            providerProtocol.providerBundleIdentifier = self.netextBundleId
            providerProtocol.enforceRoutes = true
            // macos seems to have trouble when this is true, and reports are that this breaks and
            // doesn't do what it says on the tin in the first place.  Needs more testing.
            providerProtocol.includeAllNetworks = false
            self.vpnManager.protocolConfiguration = providerProtocol
            self.vpnManager.isEnabled = true
            // self.vpnManager.isOnDemandEnabled = true
            self.vpnManager.localizedDescription = "belnet"
            self.vpnManager.saveToPreferences(completionHandler: { [self] error -> Void in
                if error != nil {
                    NSLog("Error saving to preferences")
                    self.result(msg: error!.localizedDescription)
                } else {
                    self.vpnManager.loadFromPreferences(completionHandler: { error in
                        if error != nil {
                            NSLog("Error loading from preferences")
                            self.result(msg: error!.localizedDescription)
                        } else {
                            do {
                                NSLog("Trying to start")
                                self.initializeConnectionObserver()
                                try self.vpnManager.connection.startVPNTunnel()
                            } catch let error as NSError {
                                self.result(msg: error.localizedDescription)
                            } catch {
                                self.result(msg: "There was a fatal error")
                            }
                        }
                    })
                }
            })
        }
    }

    func initializeConnectionObserver() {
        NotificationCenter.default.addObserver(forName: NSNotification.Name.NEVPNStatusDidChange, object: vpnManager.connection, queue: OperationQueue.main) { [self] _ -> Void in
            if self.vpnManager.connection.status == .invalid {
                self.result(msg: "VPN configuration is invalid")
            } else if self.vpnManager.connection.status == .disconnected {
                self.result(msg: "VPN is disconnected.")
            } else if self.vpnManager.connection.status == .connecting {
                NSLog("VPN is connecting...")
            } else if self.vpnManager.connection.status == .reasserting {
                NSLog("VPN is reasserting...")
            } else if self.vpnManager.connection.status == .disconnecting {
                NSLog("VPN is disconnecting...")
            } else if self.vpnManager.connection.status == .connected {
                self.result(msg: "VPN Connected")
            }
        }
    }
}

#if MACOS_SYSTEM_EXTENSION

    extension BelnetMain: OSSystemExtensionRequestDelegate {
        func request(_: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
            guard result == .completed else {
                NSLog("Unexpected result %d for system extension request", result.rawValue)
                return
            }
            NSLog("Belnet system extension loaded")
            setupVPNTunnel()
        }

        func request(_: OSSystemExtensionRequest, didFailWithError error: Error) {
            NSLog("System extension request failed: %@", error.localizedDescription)
        }
        
        func request(_ request: OSSystemExtensionRequest,
                     actionForReplacingExtension existing: OSSystemExtensionProperties,
                     withExtension extension: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction
        {
            NSLog("Replacing extension %@ version %@ with version %@", request.identifier, existing.bundleShortVersion, `extension`.bundleShortVersion)
            return .replace
        }
    }
    
#endif

let args = CommandLine.arguments

if args.count <= 2 {
    let delegate = BelnetMain()
    delegate.mode = args[1]
    delegate.mode = args.count > 1 ? args[1] : START
    app.run()
} else {
    NSLog(HELP_STRING)
}