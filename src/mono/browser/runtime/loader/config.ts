// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

import BuildConfiguration from "consts:configuration";
import WasmEnableThreads from "consts:wasmEnableThreads";

import { type DotnetModuleInternal, type MonoConfigInternal, JSThreadBlockingMode } from "../types/internal";
import type { BootModule, DotnetModuleConfig, MonoConfig, ResourceGroups, ResourceList } from "../types";
import { exportedRuntimeAPI, loaderHelpers, runtimeHelpers } from "./globals";
import { mono_log_error, mono_log_debug } from "./logging";
import { importLibraryInitializers, invokeLibraryInitializers } from "./libraryInitializers";
import { mono_exit } from "./exit";
import { makeURLAbsoluteWithApplicationBase } from "./polyfills";
import { appendUniqueQuery } from "./assets";

export function deep_merge_config (target: MonoConfigInternal, source: MonoConfigInternal): MonoConfigInternal {
    // no need to merge the same object
    if (target === source) return target;

    // If source has collection fields set to null (produced by boot config for example), we should maintain the target values
    const providedConfig: MonoConfigInternal = { ...source };
    if (providedConfig.assets !== undefined && providedConfig.assets !== target.assets) {
        providedConfig.assets = [...(target.assets || []), ...(providedConfig.assets || [])];
    }
    if (providedConfig.resources !== undefined) {
        providedConfig.resources = deep_merge_resources(target.resources || {
            assembly: {},
            jsModuleNative: {},
            jsModuleRuntime: {},
            wasmNative: {}
        }, providedConfig.resources);
    }
    if (providedConfig.environmentVariables !== undefined) {
        providedConfig.environmentVariables = { ...(target.environmentVariables || {}), ...(providedConfig.environmentVariables || {}) };
    }
    if (providedConfig.runtimeOptions !== undefined && providedConfig.runtimeOptions !== target.runtimeOptions) {
        providedConfig.runtimeOptions = [...(target.runtimeOptions || []), ...(providedConfig.runtimeOptions || [])];
    }
    return Object.assign(target, providedConfig);
}

export function deep_merge_module (target: DotnetModuleInternal, source: DotnetModuleConfig): DotnetModuleInternal {
    // no need to merge the same object
    if (target === source) return target;

    const providedConfig: DotnetModuleConfig = { ...source };
    if (providedConfig.config) {
        if (!target.config) target.config = {};
        providedConfig.config = deep_merge_config(target.config, providedConfig.config);
    }
    return Object.assign(target, providedConfig);
}

function deep_merge_resources (target: ResourceGroups, source: ResourceGroups): ResourceGroups {
    // no need to merge the same object
    if (target === source) return target;

    const providedResources: ResourceGroups = { ...source };
    if (providedResources.assembly !== undefined) {
        providedResources.assembly = { ...(target.assembly || {}), ...(providedResources.assembly || {}) };
    }
    if (providedResources.lazyAssembly !== undefined) {
        providedResources.lazyAssembly = { ...(target.lazyAssembly || {}), ...(providedResources.lazyAssembly || {}) };
    }
    if (providedResources.pdb !== undefined) {
        providedResources.pdb = { ...(target.pdb || {}), ...(providedResources.pdb || {}) };
    }
    if (providedResources.jsModuleWorker !== undefined) {
        providedResources.jsModuleWorker = { ...(target.jsModuleWorker || {}), ...(providedResources.jsModuleWorker || {}) };
    }
    if (providedResources.jsModuleNative !== undefined) {
        providedResources.jsModuleNative = { ...(target.jsModuleNative || {}), ...(providedResources.jsModuleNative || {}) };
    }
    if (providedResources.jsModuleDiagnostics !== undefined) {
        providedResources.jsModuleDiagnostics = { ...(target.jsModuleDiagnostics || {}), ...(providedResources.jsModuleDiagnostics || {}) };
    }
    if (providedResources.jsModuleRuntime !== undefined) {
        providedResources.jsModuleRuntime = { ...(target.jsModuleRuntime || {}), ...(providedResources.jsModuleRuntime || {}) };
    }
    if (providedResources.wasmSymbols !== undefined) {
        providedResources.wasmSymbols = { ...(target.wasmSymbols || {}), ...(providedResources.wasmSymbols || {}) };
    }
    if (providedResources.wasmNative !== undefined) {
        providedResources.wasmNative = { ...(target.wasmNative || {}), ...(providedResources.wasmNative || {}) };
    }
    if (providedResources.icu !== undefined) {
        providedResources.icu = { ...(target.icu || {}), ...(providedResources.icu || {}) };
    }
    if (providedResources.satelliteResources !== undefined) {
        providedResources.satelliteResources = deep_merge_dict(target.satelliteResources || {}, providedResources.satelliteResources || {});
    }
    if (providedResources.modulesAfterConfigLoaded !== undefined) {
        providedResources.modulesAfterConfigLoaded = { ...(target.modulesAfterConfigLoaded || {}), ...(providedResources.modulesAfterConfigLoaded || {}) };
    }
    if (providedResources.modulesAfterRuntimeReady !== undefined) {
        providedResources.modulesAfterRuntimeReady = { ...(target.modulesAfterRuntimeReady || {}), ...(providedResources.modulesAfterRuntimeReady || {}) };
    }
    if (providedResources.extensions !== undefined) {
        providedResources.extensions = { ...(target.extensions || {}), ...(providedResources.extensions || {}) };
    }
    if (providedResources.vfs !== undefined) {
        providedResources.vfs = deep_merge_dict(target.vfs || {}, providedResources.vfs || {});
    }
    return Object.assign(target, providedResources);
}

function deep_merge_dict (target: { [key: string]: ResourceList }, source: { [key: string]: ResourceList }) {
    // no need to merge the same object
    if (target === source) return target;

    for (const key in source) {
        target[key] = { ...target[key], ...source[key] };
    }
    return target;
}

// NOTE: this is called before setRuntimeGlobals
export function normalizeConfig () {
    // normalize
    const config = loaderHelpers.config;

    config.environmentVariables = config.environmentVariables || {};
    config.runtimeOptions = config.runtimeOptions || [];
    config.resources = config.resources || {
        assembly: {},
        jsModuleNative: {},
        jsModuleWorker: {},
        jsModuleRuntime: {},
        wasmNative: {},
        vfs: {},
        satelliteResources: {},
    };

    if (config.assets) {
        mono_log_debug("config.assets is deprecated, use config.resources instead");
        for (const asset of config.assets) {
            const resource = {} as ResourceList;
            resource[asset.name] = asset.hash || "";
            const toMerge = {} as ResourceGroups;
            switch (asset.behavior as string) {
                case "assembly":
                    toMerge.assembly = resource;
                    break;
                case "pdb":
                    toMerge.pdb = resource;
                    break;
                case "resource":
                    toMerge.satelliteResources = {};
                    toMerge.satelliteResources[asset.culture!] = resource;
                    break;
                case "icu":
                    toMerge.icu = resource;
                    break;
                case "symbols":
                    toMerge.wasmSymbols = resource;
                    break;
                case "vfs":
                    toMerge.vfs = {};
                    toMerge.vfs[asset.virtualPath!] = resource;
                    break;
                case "dotnetwasm":
                    toMerge.wasmNative = resource;
                    break;
                case "js-module-threads":
                    toMerge.jsModuleWorker = resource;
                    break;
                case "js-module-runtime":
                    toMerge.jsModuleRuntime = resource;
                    break;
                case "js-module-native":
                    toMerge.jsModuleNative = resource;
                    break;
                case "js-module-diagnostics":
                    toMerge.jsModuleDiagnostics = resource;
                    break;
                case "js-module-dotnet":
                    // don't merge loader
                    break;
                default:
                    throw new Error(`Unexpected behavior ${asset.behavior} of asset ${asset.name}`);
            }
            deep_merge_resources(config.resources, toMerge);
        }
    }

    if (config.debugLevel === undefined && BuildConfiguration === "Debug") {
        config.debugLevel = -1;
    }

    if (config.cachedResourcesPurgeDelay === undefined) {
        config.cachedResourcesPurgeDelay = 10000;
    }

    // ActiveIssue https://github.com/dotnet/runtime/issues/75602
    if (WasmEnableThreads) {

        if (!Number.isInteger(config.pthreadPoolInitialSize)) {
            config.pthreadPoolInitialSize = 5;
        }
        if (!Number.isInteger(config.pthreadPoolUnusedSize)) {
            config.pthreadPoolUnusedSize = 1;
        }
        if (config.jsThreadBlockingMode == undefined) {
            config.jsThreadBlockingMode = JSThreadBlockingMode.PreventSynchronousJSExport;
        }
    }

    // this is how long the Mono GC will try to wait for all threads to be suspended before it gives up and aborts the process
    if (WasmEnableThreads && config.environmentVariables["MONO_SLEEP_ABORT_LIMIT"] === undefined) {
        config.environmentVariables["MONO_SLEEP_ABORT_LIMIT"] = "5000";
    }

    if (BuildConfiguration === "Debug" && config.diagnosticTracing === undefined) {
        config.diagnosticTracing = true;
    }

    if (config.applicationCulture) {
        // If a culture is specified via start options use that to initialize the Emscripten \  .NET culture.
        config.environmentVariables!["LANG"] = `${config.applicationCulture}.UTF-8`;
    }

    runtimeHelpers.diagnosticTracing = loaderHelpers.diagnosticTracing = !!config.diagnosticTracing;
    runtimeHelpers.waitForDebugger = config.waitForDebugger;

    runtimeHelpers.enablePerfMeasure = !!config.browserProfilerOptions
        && globalThis.performance
        && typeof globalThis.performance.measure === "function";

    loaderHelpers.maxParallelDownloads = config.maxParallelDownloads || loaderHelpers.maxParallelDownloads;
    loaderHelpers.enableDownloadRetry = config.enableDownloadRetry !== undefined ? config.enableDownloadRetry : loaderHelpers.enableDownloadRetry;
}

let configLoaded = false;
export async function mono_wasm_load_config (module: DotnetModuleInternal): Promise<void> {
    if (configLoaded) {
        await loaderHelpers.afterConfigLoaded.promise;
        return;
    }
    let configFilePath;
    try {
        if (!module.configSrc && (!loaderHelpers.config || Object.keys(loaderHelpers.config).length === 0 || (!loaderHelpers.config.assets && !loaderHelpers.config.resources))) {
            // if config file location nor assets are provided
            // Temporal way for tests to opt-in for using boot.js
            module.configSrc = (globalThis as any)["__DOTNET_INTERNAL_BOOT_CONFIG_SRC"]
                ?? globalThis.window?.document?.documentElement?.getAttribute("data-dotnet_internal_boot_config_src")
                ?? "blazor.boot.json";
        }

        configFilePath = module.configSrc;

        configLoaded = true;
        if (configFilePath) {
            mono_log_debug("mono_wasm_load_config");
            await loadBootConfig(module);
        }

        normalizeConfig();

        // scripts need to be loaded before onConfigLoaded because Blazor calls `beforeStart` export in onConfigLoaded
        await importLibraryInitializers(loaderHelpers.config.resources?.modulesAfterConfigLoaded);
        await invokeLibraryInitializers("onRuntimeConfigLoaded", [loaderHelpers.config]);

        if (module.onConfigLoaded) {
            try {
                await module.onConfigLoaded(loaderHelpers.config, exportedRuntimeAPI);
                normalizeConfig();
            } catch (err: any) {
                mono_log_error("onConfigLoaded() failed", err);
                throw err;
            }
        }

        normalizeConfig();
        loaderHelpers.afterConfigLoaded.promise_control.resolve(loaderHelpers.config);
    } catch (err) {
        const errMessage = `Failed to load config file ${configFilePath} ${err} ${(err as Error)?.stack}`;
        loaderHelpers.config = module.config = Object.assign(loaderHelpers.config, { message: errMessage, error: err, isError: true });
        mono_exit(1, new Error(errMessage));
        throw err;
    }
}

export function isDebuggingSupported (): boolean {
    // Copied from blazor MonoDebugger.ts/attachDebuggerHotkey
    if (!globalThis.navigator) {
        return false;
    }

    return loaderHelpers.isChromium || loaderHelpers.isFirefox;
}

async function loadBootConfig (module: DotnetModuleInternal): Promise<void> {
    const defaultConfigSrc = module.configSrc!;
    const defaultConfigUrl = loaderHelpers.locateFile(defaultConfigSrc);

    let loaderResponse = null;
    if (loaderHelpers.loadBootResource !== undefined) {
        loaderResponse = loaderHelpers.loadBootResource("manifest", defaultConfigSrc, defaultConfigUrl, "", "manifest");
    }

    let loadedConfigResponse: Response | null = null;
    let loadedConfig: MonoConfig;
    if (!loaderResponse) {
        if (defaultConfigUrl.includes(".json")) {
            loadedConfigResponse = await fetchBootConfig(appendUniqueQuery(defaultConfigUrl, "manifest"));
            loadedConfig = await readBootConfigResponse(loadedConfigResponse);
        } else {
            loadedConfig = (await import(appendUniqueQuery(defaultConfigUrl, "manifest"))).config;
        }
    } else if (typeof loaderResponse === "string") {
        if (loaderResponse.includes(".json")) {
            loadedConfigResponse = await fetchBootConfig(makeURLAbsoluteWithApplicationBase(loaderResponse));
            loadedConfig = await readBootConfigResponse(loadedConfigResponse);
        } else {
            loadedConfig = (await import(makeURLAbsoluteWithApplicationBase(loaderResponse))).config;
        }
    } else {
        const loadedResponse = await loaderResponse;
        if (typeof (loadedResponse as Response).json == "function") {
            loadedConfigResponse = loadedResponse as Response;
            loadedConfig = await readBootConfigResponse(loadedConfigResponse);
        } else {
            // If the response doesn't contain .json(), consider it an imported module.
            loadedConfig = (loadedResponse as BootModule).config;
        }
    }

    // Prefer user-defined application environment
    if (loaderHelpers.config.applicationEnvironment) {
        loadedConfig.applicationEnvironment = loaderHelpers.config.applicationEnvironment;
    }

    deep_merge_config(loaderHelpers.config, loadedConfig);

    if (!loaderHelpers.config.applicationEnvironment) {
        loaderHelpers.config.applicationEnvironment = "Production";
    }

    function fetchBootConfig (url: string): Promise<Response> {
        return loaderHelpers.fetch_like(url, {
            method: "GET",
            credentials: "include",
            cache: "no-cache",
        });
    }
}

async function readBootConfigResponse (loadConfigResponse: Response): Promise<MonoConfig> {
    const config = loaderHelpers.config;
    const loadedConfig: MonoConfig = await loadConfigResponse.json();

    if (!config.applicationEnvironment && !loadedConfig.applicationEnvironment) {
        loadedConfig.applicationEnvironment = loadConfigResponse.headers.get("Blazor-Environment") || loadConfigResponse.headers.get("DotNet-Environment") || undefined;
    }

    if (!loadedConfig.environmentVariables)
        loadedConfig.environmentVariables = {};

    const modifiableAssemblies = loadConfigResponse.headers.get("DOTNET-MODIFIABLE-ASSEMBLIES");
    if (modifiableAssemblies) {
        // Configure the app to enable hot reload in Development.
        loadedConfig.environmentVariables["DOTNET_MODIFIABLE_ASSEMBLIES"] = modifiableAssemblies;
    }

    const aspnetCoreBrowserTools = loadConfigResponse.headers.get("ASPNETCORE-BROWSER-TOOLS");
    if (aspnetCoreBrowserTools) {
        // See https://github.com/dotnet/aspnetcore/issues/37357#issuecomment-941237000
        loadedConfig.environmentVariables["__ASPNETCORE_BROWSER_TOOLS"] = aspnetCoreBrowserTools;
    }

    return loadedConfig;
}
