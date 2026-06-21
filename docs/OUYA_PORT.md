# Porting Armagetron Advanced to the OUYA — technical write‑up

This document covers the whole journey of bringing **Armagetron Advanced** to the **OUYA**, from
choosing the source base and cross‑compiling the dependency stack, through the SDL2 Android shell,
the gamepad input redesign, the performance work, fullscreen scaling, and the OUYA packaging
details. It is meant to be read by anyone who wants to understand or reproduce the port.

---

## 1. Target & strategy

| | |
|---|---|
| **Device** | OUYA (NVIDIA **Tegra 3**, Android **4.1 / API 16**) |
| **ABI** | `armeabi-v7a` |
| **Graphics** | **OpenGL ES 2.0** (via gl4es) |
| **NDK** | r23.2.8568313 |
| **JDK / AGP** | JDK 11 / Android Gradle Plugin 7.0.2 |
| **Windowing/audio/input** | SDL2 (2.0.x) + SDL2_image + SDL2_mixer |

Armagetron's renderer is desktop **immediate‑mode OpenGL**. The OUYA only exposes GLES. Rather than
rewrite the renderer, we reuse the **gl4es** translation layer (already proven on previous OUYA
ports) which converts immediate‑mode GL / display lists to GLES2 at runtime. The whole port reuses
the SDL2 + gl4es native stack established for the AssaultCube and Teeworlds OUYA ports.

**The viability rule** the renderer had to satisfy: all rendering goes through the engine's
abstract `rRenderer` (`src/render/rRender.h`); direct `glBegin/glEnd` is forbidden (redefined to
`#error`). There is a single immediate backend, `src/render/rGLRender.cpp`, so gl4es only has to be
correct at that one point. Fonts use `FTGLTextureFont` (textured quads — gl4es‑friendly); the
config forces `FONT_TYPE=sr_fontTexture` to avoid `glDrawPixels`/`glBitmap` font paths gl4es does
not support. Desktop‑only GLEW is stubbed out.

---

## 2. Source layout

The engine source is vendored under [`armagetronad/`](../armagetronad/):

- **~169** `.cpp` translation units across `tools/ tron/ engine/ render/ network/ ui/ resource/
  serverquery/`, compiled into one native library.
- **10 in‑tree thirdparty** files (binreloc, mathexpr, particles, scrap, utf8).
- **25 `.proto`** files in `src/protobuf/` → require a host `protoc` to generate `.pb.cc/.pb.h`
  plus `libprotobuf` at link time.
- The `vParser.ypp` grammar is generated to `vParser.cpp` with **bison 3.8.2**.

The OUYA shell lives under [`android/`](../android/): the Gradle project, the CMake build that
globs the engine sources, hand‑written `aa_config.h`, the generated protobuf units, the SDL2 Java
layer (`org.libsdl.app.*`), the OUYA activities, and the bundled game assets.

---

## 3. Dependencies

All native deps are cross‑compiled for `armeabi-v7a` / `android-16` and kept under
`android/app/src/main/cpp/lib` (+ headers in `.../cpp/include`):

| Dep | Form | Notes |
|-----|------|-------|
| **gl4es** | `libGL.a` | immediate‑mode GL → GLES2 translation; also provides `glu.h` |
| **SDL2** + hidapi | `libSDL2.so`, `libhidapi.so` | window / input / GL context |
| **SDL2_image** | `libSDL2_image.so` | texture loading |
| **SDL2_mixer** 2.6.3 | `libSDL2_mixer.so` | audio (OGG/WAV via built‑in stb_vorbis) |
| **FreeType** | `libfreetype.a` | font rasterizer |
| **FTGL** | `libftgl.a` | font rendering (patched to skip desktop `find_package(OpenGL)`) |
| **libxml2** 2.11.7 | `libxml2.a` | resource / config parsing |
| **protobuf** 3.21.12 | `libprotobuf.a` | network protocol (last version **without abseil**) |
| **glu_shim** | `libglu_shim.a` | hand‑written GLU (see below) |
| **zlib / libpng** | `.a` | PNG screenshots / textures |

<a name="dependencies"></a>**Boost** is used header‑only (shared_ptr, lexical_cast, any, variant,
tuple, foreach); `boost::thread`/`system` fall back to pthread. Because the full header tree is
large, **`boost_1_82_0` is not vendored**. To build, download Boost 1.82 and place it at
`deps-staging/boost_1_82_0` (the CMake variable `BOOST_INC` points there).

### GLU shim

gl4es ships `glu.h` but **not** a GLU implementation, and Armagetron calls 7 GLU functions
(`gluLookAt`, `gluPerspective`, `gluSphere`/`gluNewQuadric`/`gluDeleteQuadric`,
`gluBuild2DMipmaps`, `gluErrorString`). `cpp/glu_shim/glu_shim.c` implements them (matrix math for
LookAt/Perspective via `glMultMatrixf`, a UV sphere for `gluSphere`, `GL_GENERATE_MIPMAP` for
`gluBuild2DMipmaps`) plus `gluTess*` stubs (referenced by FTGL's vectoriser but never reached when
`FONT_TYPE=texture`).

### config.h

`aa_config.h` is written by hand (based on `src/win32/aa_config.h`): `HAVE_LIBSDL_MIXER/FTGL/
LIBXML2`, `ENABLE_ZONESV2`, POSIX `HAVE_*`, **no** `HAVE_BOOST_THREAD` (→ pthread), **no** GLEW,
and `DONTUSEMEMMANAGER` (the engine's custom malloc/free macros otherwise clash with Boost's
`std::free`). Test/demo translation units that carry their own `main()` are excluded from the
CMake glob, along with zones‑v1 (`gWinZone.cpp`; we build zones‑v2).

---

## 4. SDL2 Android shell & API‑16 gotchas

The Java side uses the standard `org.libsdl.app.*` classes (SDL 2.0.14‑era) with backward‑compat
guards for API 16:

- `System.loadLibrary` must load each `.so` **in dependency order** (`c++_shared`, `hidapi`,
  `SDL2`, `SDL2_image`, `SDL2_mixer`, `main` last) — the API‑16 linker does not resolve transitive
  `DT_NEEDED` from the app lib dir.
- Guard API‑18 `BluetoothManager`, API‑19 `StandardCharsets`, API‑24 `Html.fromHtml`.
- An **ES2 GL context** must be requested before `SDL_CreateWindow`; `LIBGL_NOTEST=1` (and other
  `LIBGL_*` env) are set from an early constructor to avoid a Tegra 3 Cg shader‑compiler crash.
- The engine's data/config dirs are pointed at `SDL_AndroidGetExternalStoragePath()` (the engine
  uses std file I/O, so data must exist on the real filesystem). `main()` redirects them via
  `tDirectories::SetData/SetUserData`.
- RTTI **and** exceptions are enabled (`-frtti -fexceptions`) — the network/console subsystem uses
  RTTI heavily.

### Asset export

All game data (`config/ language/ textures/ models/ sound/ music/ resource/ scripts/`) is bundled
in the APK and unpacked to the external files dir on first launch by `AssetExporter`. Export is
gated on a marker (`exported_version.txt` = versionCode) **and** a sentinel file
(`config/settings.cfg`) actually being present, so it survives both a version bump and the user
wiping `Android/data/<pkg>`.

`AssetExporter.resolveBase()` retries `getExternalFilesDir()` (with `mkdirs` + a writability check)
because, right after a manual delete of `Android/data/<pkg>`, Android 4.x / the OUYA can transiently
return `null` or a not‑yet‑recreated directory (an sdcardfs negative‑cache quirk). Without this,
deleting the data dir used to make the game fail to launch.

---

## 5. Gamepad input architecture

Armagetron has **two disjoint input systems**, and getting the OUYA controller to feel right meant
respecting both:

1. **Menus** navigate by reading raw SDL `keysym` (`SDLK_UP/DOWN/LEFT/RIGHT/RETURN/ESCAPE`).
   Joystick events do **not** drive menus natively.
2. **In‑game** uses the **bind** system: every input is a `uInput` with a persistent‑ID string
   bound to a `uAction`. The config token `KEYBOARD` actually binds *any* persistent‑ID string and
   auto‑creates the input — so joystick IDs (`JOYSTICK_1_BUTTON_13`, …) bind directly in config,
   with **no keyboard emulation**.

An earlier always‑on pad→keyboard bridge (synthesising arrow keys from the stick via
`SDL_PushEvent`) was **removed** — it was unstable (analog chatter at the threshold, synthetic‑event
races, double binds). The final design instead:

- **Menus**: a small *menu‑only* in‑place converter (`su_PadToMenuKey` / `su_GetMenuInput`) maps pad
  buttons to nav keys **only on the menu fetch path** (no SDL queue push). In‑game keeps the raw
  input path → native binds.
- **In‑game**: pad buttons are bound to actions in `assets/config/autoexec.cfg`, which is loaded on
  **every** launch (unlike `default.cfg`, which loads only once).
- The joystick's `internalName` is forced to a deterministic `JOYSTICK_<id+1>` on Android, so config
  IDs don't depend on the SDL device‑name string.

### Real hardware mapping (confirmed on device via logcat)

`SDL_NumJoysticks()==2`: id0 = *OUYA Game Controller* (7 axes, **36 buttons, 0 hats**), id1 =
*Android Accelerometer* (a phantom 3‑axis device that must be ignored). On the real controller the
**D‑pad is buttons 11/12/13/14** (not a hat, not arrow keys). SDL's Android button table
(`SDLControllerManager.getButtonMask`) yields: `A=0, B=1, X=2, Y=3, L1=9, R1=10, DPAD U/D/L/R =
11/12/13/14`. The OUYA face buttons map as `O=A(0)`, `A=B(1)`, `U=X(2)`, `Y=Y(3)`.

Final in‑game binds (`autoexec.cfg`):

```
JOYSTICK_1_BUTTON_13 -> CYCLE_TURN_LEFT     # D-pad left
JOYSTICK_1_BUTTON_14 -> CYCLE_TURN_RIGHT    # D-pad right
JOYSTICK_1_BUTTON_12 -> CYCLE_BRAKE         # D-pad down
JOYSTICK_1_BUTTON_0  -> INGAME_MENU         # O
JOYSTICK_1_BUTTON_2  -> SWITCH_VIEW         # X / U (camera mode)
```

The **analog stick is intentionally unbound**: every `JOYAXISMOTION` is dropped, so steering is the
d‑pad only and the accelerometer can never drift the cycle.

> **Known open issue — controller sleep/hotplug.** The OUYA controller sleeps after a few seconds
> idle and re‑attaches as a new SDL instance, but the engine enumerates joysticks once at boot and
> doesn't handle `SDL_JOYDEVICEADDED`, so input dies after sleep until relaunch. During real play
> (controller held = awake) input flows fine.

### Input tooltips

The recurring "Press and hold J to glance left" / "Press N to switch camera modes" hints are
`uActionTooltip`s. Each is configured as `<ACTION>_TOOLTIP a b c d e` — per‑player "activations
left" counters (5 slots) — and shows while a counter is `> 0`. They are seeded in `default.cfg`
(which only loads once, so they reappear after a data wipe). On a gamepad‑only build these are pure
noise, so all 12 are forced off in `autoexec.cfg`:

```
CYCLE_TURN_LEFT_TOOLTIP 0 0 0 0 0   # ... and the other 11
```

---

## 6. Performance

Profiling on hardware showed the render thread at only **6–7 % CPU** with the system ~85 % idle and
~23 MB PSS — i.e. **not** CPU/engine‑bound. The render thread blocks in a per‑frame `glFinish()`
(`rSysdep.cpp` `ThroughputSwap`; gl4es has no GL fence), so the bottleneck is **GPU fill / overdraw**
from the engine's default *high* visual settings.

Two levers, no engine rewrite:

**(a) Overdraw cuts** in `autoexec.cfg` (all plain `tConfItem`s):

```
ALPHA_BLEND 0        # opaque walls: kills per-pixel blending + overdraw (and more legible)
FLOOR_DETAIL 1       # cheap procedural grid lines around the cycle (1); 2/3 = full textured grid = fill killer
FLOOR_MIRROR 0       # no reflected second pass
HIGH_RIM 0           # lower-detail rim walls
DITHER 0
TEXTURES_HI 0        # 16-bit textures
MOTION_BLUR_TIME 0
```

This took the frame rate from **~14 fps to ~56 fps** (≈4×) **without touching resolution**.

**(b) The `st_FirstUse` trap (important).** Deleting `Android/data/<pkg>` resets the engine to
"first use", and the first‑use path calls `sr_LoadDefaultConfig()`, which sets
`sr_floorDetail = rFLOOR_TWOTEXTURE` (the *maximum*, full‑arena textured grid) and re‑enables alpha
blending — **after** `autoexec.cfg` has applied its cuts. The NVIDIA/Tegra branch of that function
leaves the floor at maximum. So a data wipe silently reverted every perf setting and tanked the
frame rate. The fix is an `#ifdef __ANDROID__` clamp at the end of `sr_LoadDefaultConfig()` in
`rScreen.cpp` that forces the low‑detail profile every time it runs (floor = grid, no alpha blend /
high rim / dither / mirror, 16‑bit textures). This makes the **first** launch match later launches.

Floor detail enum (`rScreen.h`): `rFLOOR_OFF=0`, `rFLOOR_GRID=1` (cheap procedural lines around the
cycle), `rFLOOR_TEXTURE=2` (full‑arena textured grid), `rFLOOR_TWOTEXTURE=3`. The port ships
`FLOOR_DETAIL 1` — the minimum *visible* grid.

---

## 7. Fullscreen & render scaling

The OUYA hands the app an overscan‑safe content surface (~1728×892), not the full 1920×1080 panel,
and rendering at that native resolution wastes fill rate. The port renders at a fixed **720p** and
lets the OS upscale to the whole panel — fullscreen *and* a smaller fragment count:

- `SDLSurface` calls `getHolder().setFixedSize(1280, 720)` in its constructor, which makes the GL
  drawable 720p (confirmed in logcat: `SDL Window size: 1280x720`). The engine's
  `SDL_GL_GetDrawableSize()` then drives a 720p `glViewport`.
- **The missing piece**: the `SDLSurface` was added to its `RelativeLayout` with *no* `LayoutParams`
  → default `WRAP_CONTENT`, so the 720p buffer was not stretched (black borders). Adding explicit
  `MATCH_PARENT` (+ `CENTER_IN_PARENT`) layout params makes SurfaceFlinger upscale the 720p buffer to
  true fullscreen.
- `autoexec.cfg` also forces `FULLSCREEN 1` / `LAST_FULLSCREEN 1` so a stale `user.cfg` can never
  leave the game windowed.

---

## 8. OUYA packaging

### Games‑menu tile

Per the [OUYA‑saviors guide](https://github.com/ouya-saviors/documentation/wiki/.apk-in-OUYA-games-menu),
a game appears in the OUYA games menu when it has a launchable activity with the
`tv.ouya.intent.category.GAME` category **and** the APK contains an icon at **`assets/ouya_icon.png`,
732×412 px**. Both are provided (the icon is center‑cropped/resized from the project banner). A
standard `android:icon` launcher icon is also included.

### First‑launch ANR → LoaderActivity

Unpacking the bundled data takes ~1 minute on the OUYA's flash. Doing that synchronously in
`SDLActivity.onCreate` blocks the UI thread long enough to trip Android's **ANR** watchdog (~5 s),
and the system force‑finishes the game. The fix is a dedicated **`LoaderActivity`** (now the
launcher / OUYA‑GAME entry point) that:

1. shows a tiny black "Loading…" screen,
2. runs `AssetExporter.exportIfNeeded` on a **background thread**, then
3. `startActivity(ArmagetronActivity)` and `finish()`.

`ArmagetronActivity` (the SDL activity) is now `exported=false` with no intent filter; its own
export call becomes a fast no‑op because the loader already populated the data.

### Manifest essentials

`landscape`, `Theme.NoTitleBar.Fullscreen`, `glEsVersion 0x00020000`, touchscreen `not required`,
gamepad declared, `INTERNET` + `ACCESS_NETWORK_STATE` + `WRITE_EXTERNAL_STORAGE`.

---

## 9. Build notes

- On Windows, **build from a path without spaces** — AGP/ninja throw
  `java.io.IOException: filename/directory syntax incorrect` otherwise. Use a junction
  (`New-Item -ItemType Junction -Path C:\arma -Target "<repo>"`) and build from `C:\arma\android`.
- After moving the workspace, wipe the stale CMake cache once: stop the daemon (`gradlew --stop`)
  and delete `app/.cxx` and `app/build/intermediates/{cxx,cmake}` (they bake in the old path).
- Java/asset‑only changes rebuild in ~1–7 s (the native lib is cached). Editing an engine `.cpp`
  triggers an incremental native recompile + relink (still fast).

### Iterating on a real OUYA

```sh
adb connect <ouya-ip>:5555           # the OUYA's DHCP address (changes between sessions)

# Always wipe before redeploy so fresh config/first-use defaults apply cleanly:
adb shell pm clear org.armagetronad.ouya
adb shell rm -r /sdcard/Android/data/org.armagetronad.ouya
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n org.armagetronad.ouya/.LoaderActivity
```

Diagnostics go to **logcat** (the engine's `arma.log` is block‑buffered and drops events):
`adb logcat -s SDL AssetExporter ARMA-INPUT`.

---

## 10. Summary of port‑specific patches

| Area | File(s) | Change |
|------|---------|--------|
| GL/GLU | `cpp/glu_shim/glu_shim.c` | hand‑written GLU (7 funcs + tess stubs) |
| Build config | `cpp/aa_config.h`, `cpp/CMakeLists.txt` | API‑16 config, source glob, `DONTUSEMEMMANAGER` |
| Data dir | `tron/gArmagetron.cpp` | point dirs at external storage; log to `arma.log` |
| Input (menus) | `ui/uInputQueue.{cpp,h}`, `ui/uMenu.cpp` | menu‑only pad→nav‑key converter |
| Input (device) | `ui/uInput.cpp` | deterministic joystick `internalName`; drop stick axes |
| Input (binds) | `assets/config/autoexec.cfg` | pad binds, camera on X, tooltips off, perf cuts, fullscreen |
| Performance | `render/rScreen.cpp` | `__ANDROID__` low‑detail clamp in `sr_LoadDefaultConfig()` |
| Fullscreen | `java/.../SDLActivity.java` | `setFixedSize(1280,720)` + `MATCH_PARENT` surface |
| Packaging | `java/.../LoaderActivity.java`, `AndroidManifest.xml`, `assets/ouya_icon.png` | ANR‑safe loader, OUYA tile |
| Robust assets | `java/.../AssetExporter.java` | retry/mkdirs/sentinel‑gated export (survives data wipe) |

---

*Unofficial community port for preservation. Armagetron Advanced is GPL‑2.0‑or‑later; this port
keeps that license.*
