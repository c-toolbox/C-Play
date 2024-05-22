# Deploy(installer maker) guide of C-Play

After building C-Play, with Craft and MPV, you would want to deploy it to a working directory, and optional build an installer.

Too make it standalone, you need to copy/install all dependencies including all DLL:s (For Qt, KF and MPV etc) and QML files alongside the executable.

First, make sure your *CMAKE_INSTALL_PREFIX* is set to the folder you want to copy everything to.

Shared "delay-loading" can be tricky to follow, so some manual try/error can be expected of which DLLs are needed for C-Play to start.
For copying the correct source (as of Craft with 5.15 LTS) and MPV through current guide, simply enable *CPLAY_INSTALL_DEPENDENCIES* in CMake.

The following options become visible below (and executed in this order): 

* *CPLAY_INSTALL_ITEMS_FROM_DATA*: Does copy all files from C-Play data source into the install directory.

* *CPLAY_INSTALL_PLUGINS_FROM_CRAFT* : Copy all plugins (dlls) from Craft build

* *CPLAY_INSTALL_QML_FROM_CRAFT* : Copy all qml files from Craft build

* *CPLAY_INSTALL_PREDEFINED_DLL_LIST_FROM_CRAFT* : Will copy a predefined list of DLLs from Craft build (made manually and then printed with *"dir /b *.dll > craft_dlls.txt*"). The easiest approach of generating your own (if this does not work), is coping all dlls from craft to the bin directory, launch C-Play and then try to delete all dlls. The ones that cannot be deleted is the ones to keep.

* *CPLAY_INSTALL_PRECOMPILIED_JACK_DLL*: Compiling a JACK version for MPV can be quite tricky. As such as small but important library, C-Play include some pre-complied libs for JACK that works with MPV+FFmpeg.

* *CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES*: Runs a cmake command to look for any direct dependencies of C-Play, in the QMAKE/CRAFT and MPV build directories. To complete the pre-defined list (which only fetches from CRAFT/QMAKE directory).

Run *INSTALL* from Visual Studio to trigger a complete install/copy operation for each of the above options.
Then your *CMAKE_INSTALL_PREFIX/bin* should include a working standalone solution of C-Play.

## Build installer

After INSTALL has been run in Visual Studio, it is time to make an installer following this steps:

1. Download latest [Visual Studio Re-dist](https://aka.ms/vs/17/release/vc_redist.x64.exe) to the */bin* folder to be launched during installation of C-Play.

1. Run the "CPlay_Pack.nsi* script, now present in the install directory to make an installer.

1. The installer is now ready...

*Note: It's recommended to also copy the mpv.exe into the /bin folder before making the installer, as this is a useful tool for looking into media specifics.*