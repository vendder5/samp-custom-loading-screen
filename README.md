# samp-custom-loading-screen

lightweight C++20 plugin for San Diarreas Multiplayer **(SA:MP)** that replaces the default loading screen with a custom image.

## build
```bash
cmake -S . -B build -A Win32
```

```bash
cmake --build build --config Release
```

## installation

1.  copy the generated `custom-loading-screen.asi` (from `build/Release/`) to your gta san andreas root directory.

2. make sure you have **silent's asi loader** installed.

3.  place your custom loading image in the same root directory where you installed the plugin. it must be named **“loading_screen.png”**

## screenshots

**without plugin**
![without](images/without_plugin.png)


**with plugin**
![with](images/with_plugin.png)