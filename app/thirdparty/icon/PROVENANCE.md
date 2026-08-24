# The launcher icon source

ef2-launcher-256.png is the retail EF2 GOG icon, copied verbatim from

    <home>/.local/share/icons/hicolor/256x256/apps/6A09_goggame-2125201406.0.png

(256x256 RGBA).  res/mipmap-*/ic_launcher.png are the LANCZOS downscales at the
standard launcher densities (mdpi 48, hdpi 72, xhdpi 96, xxhdpi 144, xxxhdpi 192).
Regenerate with PIL: src.resize((size,size), Image.LANCZOS).
