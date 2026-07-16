add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("levilamina_version")
    set_default("26.10.14")
    set_showmenu(true)
    set_description("Set the levilamina version to use. Default is 26.10.14.")
option_end()

add_requires("levilamina " .. (get_config("levilamina_version") or "26.10.14"), { configs = { target_type = "server" }})

if not has_config("vs_runtime") then set_runtimes("MD") end

local function get_bedrockdata_version(target)
    local envs = target:pkgenvs()
    if not envs then return nil end

    for _, value in pairs(envs) do
        if type(value) == "string" then
            local version = value:match("packages[/\\]b[/\\]bedrockdata[/\\]v?(%d[%d%.]*)")
            if version then return version end
        end
    end

    return nil
end

local function version_ge(v1, v2)
    local function parse(v)
        local t = {}
        for w in v:gmatch("%d+") do
            t[#t + 1] = tonumber(w)
        end
        return t
    end
    local a, b = parse(v1), parse(v2)
    for i = 1, math.max(#a, #b) do
        local x = a[i] or 0
        local y = b[i] or 0
        if x > y then return true end
        if x < y then return false end
    end
    return true
end

target("iInvalidNAKFix")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
    add_defines(
        "NOMINMAX", 
        "UNICODE",
        "_HAS_CXX23=1",
        "_SILENCE_CXX20_IS_ALWAYS_EQUAL_DEPRECATION_WARNING=1"
    )
    add_files("src/**.cpp")
    add_includedirs("src")
    add_packages("levilamina")
    set_exceptions("none")
    set_kind("shared")
    set_languages("cxx20")
    set_symbols("debug")
    set_optimize("aggressive")
    set_strip("all")
    if version_ge(get_config("levilamina_version") or "26.10.14", "26.20.0") then
        set_toolchains("clang-cl")
        add_cxflags(
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            { tools = { "clang_cl" } }
        )
    end

    after_load(function(target)
        target:add("defines", "VERSION_" .. get_bedrockdata_version(target):gsub("%.", "_") .. "=1")
    end)

    before_link(function(target)
        import("lib.detect.find_file")
        import("core.project.config")

        -- 修复莫名其妙的环境变量缺失导致的链接失败
        os.addenvs(target:pkgenvs())
        target:add("shflags", "/DELAYLOAD:bedrock_runtime.dll")

        local libdir = path.join(config.builddir(), ".prelink", "lib")
        if os.exists(libdir) then os.rm(libdir) end
        os.mkdir(libdir)

        local data = assert(find_file("bedrock_runtime_data", {"$(env PATH)"}), "Cannot find bedrock_runtime_data")
        local link = assert(find_file("prelink.exe", {"$(env PATH)"}), "Cannot find prelink.exe")

        os.runv(link, {
            string.format("%s-%s-%s", get_config("target_type"), target:plat(), target:arch()),
            path.join(config.builddir(), ".prelink"),
            data,
            table.unpack(target:objectfiles())
        })

        target:add("linkdirs", libdir)
        target:add("links", "bedrock_runtime_api")
    end)

    after_build(function (target)
        local output_dir = path.join(os.projectdir(), "bin_" .. get_bedrockdata_version(target), target:name())
        local artifact_file = target:targetfile()

        os.rm(output_dir)

        os.vcp(artifact_file, format("%s/", output_dir))
        os.vcp(target:symbolfile(), format("%s/", output_dir))

        import("scripts.generate-manifest", { rootdir = os.projectdir() }).generate_manifest(
            format("%s/manifest.json", output_dir),
            {
                name = target:name(),
                entry = path.basename(artifact_file)
            }
        )

        local pack_path = path.join(os.projectdir(), "bin", target:name() .. "_" .. get_bedrockdata_version(target) .. ".zip")
        if os.isfile(pack_path) then os.rm(pack_path) end
        os.runv("7z.exe", { "a", "-y", "-aoa", "-tzip", pack_path, "-r", output_dir, "-xr!*.pdb" })
    end)

-- cmd /v:on /c "cd /d D:\Github\MiracleForest\iInvalidNAKFix && for %v in (26.20.0 26.10.14 1.9.9 1.8.0-rc.2 1.7.7 1.6.1 1.5.2 1.4.4 1.3.4 1.2.1 1.1.2 1.0.1) do @(echo Building LL=%v && xmake f -c -y --levilamina_version=%v && xmake -r)"