scons platform=windows module_mono_enabled=yes
scons platform=windows target=template_release module_mono_enabled=yes
.\bin\mouldog_godot\godot.windows.editor.x86_64.mono.exe --headless --generate-mono-glue modules/mono/glue
./modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin
