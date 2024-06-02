#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <bsa/tes4.hpp>
#include <cstdio>
#include <filesystem>
#include <tuple>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "BethesdaGame/BethesdaGame.hpp"
#include "ParallaxGen/ParallaxGen.hpp"
#include "ParallaxGenUtil/ParallaxGenUtil.hpp"
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = std::filesystem;

//todo: blacklist files that can be defined by mods
//todo: ability to specify multiple suffixes for height/env map?
//todo: return types?
//todo: ziping

fs::path getExecutablePath() {
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    return fs::path(buffer);
}

void addArguments(CLI::App& app, int& verbosity, fs::path& data_dir, string& game_type, bool& ignore_parallax, bool& ignore_complex_material) {
    app.add_flag("-v", verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data (warning: TRACE data is very verbose)");
    app.add_option("-d,--data-dir", data_dir, "Manually specify of Skyrim data directory (ie. <Skyrim SE Directory>/Data)");
    app.add_option("-g,--game-type", game_type, "Specify game type [skyrimse, skyrim, or skyrimvr]");
    app.add_flag("--ignore-parallax", ignore_parallax, "Don't generate any parallax meshes");
    app.add_flag("--ignore-complex-material", ignore_complex_material, "Don't generate any complex material meshes");
}

int main(int argc, char** argv) {
    // get program location
    const fs::path output_dir = getExecutablePath().parent_path();

    // Create loggers
    auto console_sink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto basic_sink = make_shared<spdlog::sinks::basic_file_sink_mt>(output_dir.string() + "/ParallaxGen.log", true);
    vector<spdlog::sink_ptr> sinks{console_sink, basic_sink};
    auto logger = make_shared<spdlog::logger>("ParallaxGen", sinks.begin(), sinks.end());

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("Welcome to ParallaxGen version {}!", PARALLAXGEN_VERSION);

    // print mesh output location
    fs::path mesh_output_dir = output_dir / "ParallaxGen_Output";
    spdlog::info("Mesh output directory: {}", mesh_output_dir.string());

    //
    // CLI Arguments
    //
    int verbosity = 0;
    fs::path data_dir;
    string game_type = "skyrimse";
    bool ignore_parallax = false;
    bool ignore_complex_material = false;

    CLI::App app{ "ParallaxGen: Auto convert meshes to parallax meshes" };
    addArguments(app, verbosity, data_dir, game_type, ignore_parallax, ignore_complex_material);
    CLI11_PARSE(app, argc, argv);

    // CLI argument validation
    if (game_type != "skyrimse" && game_type != "skyrim" && game_type != "skyrimvr") {
        spdlog::critical("Invalid game type specified. Please specify 'skyrimse', 'skyrim', or 'skyrimvr'");
        exitWithUserInput(1);
    }

    if (ignore_parallax && ignore_complex_material) {
        spdlog::critical("Both ignore-parallax and ignore-complex-material flags are set. Nothing to do.");
        exitWithUserInput(1);
    }

    // Set logging mode
    if (verbosity >= 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("DEBUG logging enabled");
    }
    
    if (verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::trace("TRACE logging enabled");
    }

    // Create bethesda game object
    BethesdaGame::GameType gameType;
    if (game_type == "skyrimse") {
        gameType = BethesdaGame::GameType::SKYRIM_SE;
    }
    else if (game_type == "skyrim") {
        gameType = BethesdaGame::GameType::SKYRIM;
    }
    else if (game_type == "skyrimvr") {
        gameType = BethesdaGame::GameType::SKYRIM_VR;
    }

	BethesdaGame bg = BethesdaGame(gameType, data_dir);
    ParallaxGenDirectory pgd = ParallaxGenDirectory(bg);
    ParallaxGen pg = ParallaxGen(mesh_output_dir, &pgd);

    // User Input to Continue
    cout << "Press ENTER to start mesh generation...";
    cin.get();

    // Populate file map from data directory
    pgd.populateFileMap();

    // Build file vectors
    vector<fs::path> heightMaps;
    if (!ignore_parallax) {
        heightMaps = pgd.findHeightMaps();
    }

    vector<fs::path> complexMaterialMaps;
    if (!ignore_complex_material) {
        complexMaterialMaps = pgd.findComplexMaterialMaps();
    }

    vector<fs::path> meshes = pgd.findMeshes();

    // Patch meshes
    pg.patchMeshes(meshes, heightMaps, complexMaterialMaps);

    spdlog::info("ParallaxGen has finished generating meshes.");

    // Close Console
	exitWithUserInput(0);
}
