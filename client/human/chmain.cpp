#include "HumanClientApp.h"
#include "../../util/OptionsDB.h"
#include "../../util/Directories.h"
#include "../../util/Version.h"
#include "../../util/XMLDoc.h"

#include <OgreCamera.h>
#include <OgreLogManager.h>
#include <OgreRenderSystem.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreViewport.h>

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>
#include <iostream>


#ifdef FREEORION_WIN32
#  define OGRE_INPUT_PLUGIN_NAME "GiGiOgrePlugin_OIS.dll"
#else
#  define OGRE_INPUT_PLUGIN_NAME "libGiGiOgrePlugin_OIS.so"
#endif

int main(int argc, char* argv[])
{
    InitDirs();

    // read and process command-line arguments, if any
    try {
        GetOptionsDB().AddFlag('h', "help", "OPTIONS_DB_HELP");
        GetOptionsDB().AddFlag('g', "generate-config-xml", "OPTIONS_DB_GENERATE_CONFIG_XML");
        GetOptionsDB().AddFlag('m', "music-off", "OPTIONS_DB_MUSIC_OFF");
        GetOptionsDB().Add<std::string>("bg-music", "OPTIONS_DB_BG_MUSIC", "artificial_intelligence_v3.ogg");

        // The false/true parameter below controls whether this option is stored in the XML config file.  On Win32 it is
        // not, because the installed version of FO is run with the command-line flag added in as appropriate.
        GetOptionsDB().AddFlag('f', "fullscreen", "OPTIONS_DB_FULLSCREEN",
#ifdef FREEORION_WIN32
                               false
#else
                               true
#endif
            );
        XMLDoc doc;
        boost::filesystem::ifstream ifs(GetConfigPath());
        doc.ReadDoc(ifs);
        ifs.close();
        GetOptionsDB().SetFromXML(doc);
        GetOptionsDB().SetFromCommandLine(argc, argv);
        bool early_exit = false;
        if (GetOptionsDB().Get<bool>("help")) {
            GetOptionsDB().GetUsage(std::cerr);
            early_exit = true;
        }
        if (GetOptionsDB().Get<bool>("generate-config-xml")) {
            GetOptionsDB().Remove("generate-config-xml");
            boost::filesystem::ofstream ofs(GetConfigPath());
            GetOptionsDB().GetXML().WriteDoc(ofs);
            ofs.close();
        }
        if (early_exit)
            return 0;
    } catch (const std::invalid_argument& e) {
        std::cerr << "main() caught exception(std::invalid_arg): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "main() caught exception(std::runtime_error): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "main() caught exception(std::exception): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (...) {
        std::cerr << "main() caught unknown exception.";
        return 1;
    }

    Ogre::LogManager* log_manager = 0;
    Ogre::Root* root = 0;

    try {
        using namespace Ogre;

        log_manager = new LogManager();
        log_manager->createLog("ogre_log.txt", true, false);

        root = new Root("ogre_plugins.cfg");

        RenderSystemList* renderers_list = root->getAvailableRenderers();
        bool failed = true;
        RenderSystem* selected_render_system = 0;
        for (unsigned int i = 0; i < renderers_list->size(); ++i) {
            selected_render_system = renderers_list->at(i);
            String name = selected_render_system->getName();
            if (name.compare("OpenGL Rendering Subsystem") == 0) {
                failed = false;
                break;
            }
        }
        if (failed)
            throw std::runtime_error("Failed to find an Ogre GL render system.");

        root->setRenderSystem(selected_render_system);

        selected_render_system->setConfigOption("Full Screen", GetOptionsDB().Get<bool>("fullscreen") ? "Yes" : "No");
        std::string video_mode_str =
            boost::io::str(boost::format("%1% x %2% @ %3%-bit colour") %
                           GetOptionsDB().Get<int>("app-width") %
                           GetOptionsDB().Get<int>("app-height") %
                           GetOptionsDB().Get<int>("color-depth"));
        selected_render_system->setConfigOption("Video Mode", video_mode_str);

        // retrieve the config option map
        ConfigOptionMap config_options = selected_render_system->getConfigOptions();
        ConfigOptionMap::const_iterator end_it = config_options.end();
        for (ConfigOptionMap::const_iterator it = config_options.begin(); it != end_it; ++it) {
            String option_name = it->first;
            String current_value = it->second.currentValue;
            StringVector possible_values = it->second.possibleValues;
            for (unsigned int i = 0; i < possible_values.size(); ++i) {
                String value = possible_values.at(i);
            }
        }

        RenderWindow* window = root->initialise(true, "FreeOrion " + FreeOrionVersionString());
        SceneManager* scene_manager = root->createSceneManager("OctreeSceneManager");

        Camera* camera = scene_manager->createCamera("Camera");
        // Position it at 500 in Z direction
        camera->setPosition(Vector3(0, 0, 500));
        // Look back along -Z
        camera->lookAt(Vector3(0, 0, -300));
        camera->setNearClipDistance(5);

        Viewport* viewport = window->addViewport(camera);
        viewport->setBackgroundColour(ColourValue(0, 0, 0));
        camera->setAspectRatio(static_cast<double>(viewport->getActualWidth()) / viewport->getActualHeight());

        HumanClientApp app(root, window, scene_manager, camera, viewport);
        root->loadPlugin(OGRE_INPUT_PLUGIN_NAME);
        app();
    } catch (const HumanClientApp::CleanQuit& e) {
        // do nothing
    } catch (const std::invalid_argument& e) {
        Logger().errorStream() << "main() caught exception(std::invalid_arg): " << e.what();
    } catch (const std::runtime_error& e) {
        Logger().errorStream() << "main() caught exception(std::runtime_error): " << e.what();
    } catch (const  boost::io::format_error& e) {
        Logger().errorStream() << "main() caught exception(boost::io::format_error): " << e.what();
    } catch (const std::exception& e) {
        Logger().errorStream() << "main() caught exception(std::exception): " << e.what();
    }

    if (root) {
        root->unloadPlugin(OGRE_INPUT_PLUGIN_NAME);
        delete root;
    }

    if (log_manager)
        delete log_manager;

    return 0;
}
