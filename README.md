# ComputerGraphicsFinals - CS0045 
- Dr. Hazel San Patilano

**Names:**
Kristian David R. Bautista - 202311645
Angel A. Letada - 2023
Marianne Angelika B. Santos



## Overview
This project is a OpenGL/FreeGLUT demo based on the anime "A Fragrant Flower Blooms With Dignity". The 11th episode was the main inspiration of the scenes, where Waguri, the female lead, invited Rintaro, the male lead to her favorite place, the park (Hiroo North Park).

This also includes a simple subtitle and audio system and a on-screen UI for callback controls.

## Recommended Environment
- Visual Studio Code (Blue) — open the workspace folder and use the integrated terminal (PowerShell) for build and run steps.
- A MinGW/MSYS `g++` toolchain on Windows that matches the architecture of the provided libraries (x64 by default).

## Steps.

1. Open the integrated terminal in the IDE
2. Run the build command below:

```powershell
./app.exe
```

3. Optionally, if it does not run, execute the compile command below, and retry step 2:
```powershell
g++ -DEMBEDDED_MODE src/CS0045_Final_Bautista_Letada_Santos_SCENE1.cpp src/utils.cpp src/CS0045_Final_Bautista_Letada_Santos_SCENE2.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -lwinmm -o app.exe
```

## External Libraries
- FreeGLUT (windowing/input) — FreeGLUT 3.x
- GLEW (OpenGL extension loader) — GLEW 2.1.x
- OpenGL (system `opengl32` / `glu32`) — Windows runtime
- WinMM / MCI (`mmsystem` / `winmm`) — Windows multimedia APIs for audio playback
