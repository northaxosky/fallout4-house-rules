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

-- optional post-build deploy path; set via: xmake f --deploy_dir="<mod folder>"
-- empty (the default, and CI's state) skips the deploy step entirely.
option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("MO2/game mod folder to copy the built DLL + MCM files into after build (empty = skip)")
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
        local deploy_dir = get_config("deploy_dir")
        if not deploy_dir or deploy_dir == "" then
            return
        end
        local plugins_dir = path.join(deploy_dir, "F4SE/Plugins")
        local mcm_dir     = path.join(deploy_dir, "MCM/Config/HouseRules")
        os.mkdir(plugins_dir)
        os.mkdir(mcm_dir)
        os.cp(target:targetfile(), plugins_dir)
        local pdb = path.join(target:targetdir(), target:name() .. ".pdb")
        if os.isfile(pdb) then
            os.cp(pdb, plugins_dir)
        end
        for _, esp in ipairs(os.files("Data/*.esp")) do
            os.cp(esp, deploy_dir)
        end
        os.cp("Data/MCM/Config/HouseRules/*", mcm_dir)
        cprint("${bright green}deploy: ${clear}copied to %s", deploy_dir)
    end)
