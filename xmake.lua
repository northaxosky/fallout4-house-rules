-- set minimum xmake version (commonlibf4 requires 3.0.0+)
set_xmakever("3.0.0")

-- enable REX::INI before pulling in CommonLibF4
set_config("commonlib_ini", true)

-- include CommonLibF4 (default: submodule at lib/commonlibf4; override with COMMONLIBF4_PATH env var)
local commonlib_path = os.getenv("COMMONLIBF4_PATH") or "lib/commonlibf4"
includes(commonlib_path)

-- set project
set_project("HouseRules")
set_version("1.2.0")
set_license("MIT")

-- set defaults
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

-- set policies
set_policy("package.requires_lock", true)

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- mod manager folder for the dev deploy, joined onto FO4_DEV_MODS
local dev_mod_folder = "House Rules - Dev"

-- optional post-build deploy path; set via: xmake f --deploy_dir="<mod folder>"
-- overrides FO4_DEV_MODS. With neither set (CI's state) the deploy step is skipped.
option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("MO2/game mod folder to copy the built DLL + MCM files into after build (empty = use FO4_DEV_MODS, else skip)")
option_end()

option("deploy_frontend")
    set_default("mcm")
    set_values("mcm", "dmui")
    set_showmenu(true)
    set_description("Settings frontend to deploy: mcm (default) or dmui")
option_end()

-- targets
target("HouseRules")
    set_kind("shared")
    set_arch("x64")

    add_deps("commonlibf4")

    -- match CommonLibF4's runtime slot count (its xmake define is not public, so REL::ID would silently truncate to 1 slot)
    add_defines("COMMONLIB_RUNTIMECOUNT=3")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    add_extrafiles(".clang-format")

    after_build(function (target)
        local pdb = path.join(target:targetdir(), target:name() .. ".pdb")
        if is_mode("releasedbg") then
            if not os.isfile(pdb) then
                raise(
                    "package: releasedbg build did not produce required symbols at %s",
                    pdb)
            end
            os.execv(
                "python",
                {
                    "tools/package_release.py",
                    "--dll", target:targetfile(),
                    "--pdb", pdb
                })
        end

        local deploy_dir = get_config("deploy_dir")
        if not deploy_dir or deploy_dir == "" then
            local mods_root = os.getenv("FO4_DEV_MODS")
            deploy_dir = mods_root and path.join(mods_root, dev_mod_folder) or ""
        end
        if deploy_dir == "" then
            return
        end
        local plugins_dir = path.join(deploy_dir, "F4SE/Plugins")
        local mcm_dir     = path.join(deploy_dir, "MCM/Config/HouseRules")
        local frontend    = get_config("deploy_frontend") or "mcm"
        if frontend == "dmui" then
            local stale_config = path.join(mcm_dir, "config.json")
            local stale_swf = path.join(mcm_dir, "lib.swf")
            if os.isfile(stale_config) or os.isfile(stale_swf) then
                raise(
                    "deploy: native frontend selected but stale MCM page assets exist in %s; use a clean mod folder or remove config.json/lib.swf manually",
                    mcm_dir)
            end
        end
        os.mkdir(plugins_dir)
        os.mkdir(mcm_dir)
        os.cp(target:targetfile(), plugins_dir)
        if os.isfile(pdb) then
            os.cp(pdb, plugins_dir)
        end
        local esp = "package/core/HouseRules.esp"
        if os.isfile(esp) then
            os.cp(esp, deploy_dir)
        end
        os.cp("package/core/MCM/Config/HouseRules/settings.ini", mcm_dir)
        os.cp(
            path.join(
                "package/frontends",
                frontend,
                "F4SE/Plugins/HouseRules.frontend.ini"),
            plugins_dir)
        if frontend == "mcm" then
            os.cp(
                "package/frontends/mcm/MCM/Config/HouseRules/config.json",
                mcm_dir)
            os.cp(
                "package/frontends/mcm/MCM/Config/HouseRules/lib.swf",
                mcm_dir)
        else
            local presets_dir = path.join(plugins_dir, "HouseRules/Presets")
            os.mkdir(presets_dir)
            os.cp("package/frontends/dmui/F4SE/Plugins/HouseRules/Presets/*.ini", presets_dir)
        end
        cprint(
            "${bright green}deploy: ${clear}copied %s frontend to %s",
            frontend,
            deploy_dir)
    end)

target("HouseRulesRuntimeTests")
    set_kind("binary")
    set_arch("x64")

    add_deps("dearmoddingui-api")
    add_packages("simpleini")
    add_files(
        "tests/runtime_contracts.cpp",
        "src/Configuration/Presets.cpp",
        "src/Configuration/SettingsPersistence.cpp",
        "src/Gameplay/Lifecycle.cpp"
    )
    add_includedirs("src")
