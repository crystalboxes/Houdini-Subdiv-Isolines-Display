## Houdini-SubdivIsoline-View
![Alt Text](https://media.giphy.com/media/LPxL71hXmRAzPhqPcI/giphy.gif)
![Alt Text](https://media.giphy.com/media/YPbn7xlFftblgubcN7/giphy.gif)

Shows the subdivision surface isolines in the viewport for a geometry and highlights crease weights. Could be useful for SDS modeling. This is an experimental code, which uses OpenSubdiv HDK API, suffers with FPS drops when working with a heavy geometry.
## Requiremenets
 - cmake
 - Xcode/Visual Studio (version depeds on the Houdini installation).
## Building
 - Set the **HOUDINI_HFS_DIR** environment variable to the Houdini
   installation directory (e.g. *C:\Program Files\Side Effects
   Software\Houdini 17.5.229*).
 - Build with cmake.
