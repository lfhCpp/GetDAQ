# GetDAQ application icon

`getdaq.png` is the original image supplied by the user. The complete image is
preserved. `getdaq.ico` (Windows) and `getdaq.icns` (macOS) contain square versions
with white padding, without stretching or cropping the diagram.

The main application embeds the PNG through `third_party/application-icons/getdaq.qrc`; qmake embeds
the ICO in the Windows executable and assigns the ICNS to the macOS app bundle.
