# Armagetron Advanced → OUYA — Étape 1 : sources & dépendances

Bilan de l'étape 1 (préparation du portage, base **trunk + gl4es**). Cible : OUYA = Tegra 3,
Android 4.1 / **API 16**, **OpenGL ES 2.0**, **armeabi-v7a**. NDK **23.2.8568313**.

## 1. Code source Armagetron (cloné dans `armagetronad/`)

- **169 fichiers** `.cpp/.mm` répartis : `tools/` 49, `tron/` 48, `engine/` 28, `render/` 16,
  `network/` 15, `ui/` 4, `resource/` 3, `serverquery/` 2.
- **10 fichiers thirdparty in-tree** (compilés avec le jeu, aucune dépendance externe) :
  `binreloc, mathexpr, particles, scrap, utf8`.
- Bibliothèques statiques internes (autotools `noinst_LIBRARIES`) :
  `libtools libnetwork libenginecore libengine librender libtron libui` → programme
  `armagetronad_main`. Manifeste complet des sources : voir `src/Makefile.am` (blocs `*_a_SOURCES`).
- **10 fichiers `.proto`** dans `src/protobuf/` → nécessitent `protoc` (génération `.pb.cc/.pb.h`)
  + `libprotobuf` au link.

## 2. Rendu — stratégie validée (spike OK)

- Tout le rendu passe par la classe abstraite `rRenderer` (`src/render/rRender.h`) ; `glBegin/glEnd`
  directs **interdits** (redéfinis en `#error`). Backend immédiat unique : `src/render/rGLRender.cpp`
  → **gl4es** traduit à ce seul point. Display lists (`rDisplayList.cpp`) supportées par gl4es.
- **Police** : un seul fichier `src/render/rFont.cpp`, défaut `sr_fontTexture` = `FTGLTextureFont`
  (quads texturés immédiats = OK gl4es). **Forcer `FONT_TYPE=sr_fontTexture`** dans la config OUYA ;
  éviter `sr_fontPixmap`(glDrawPixels) / `sr_fontBitmap`(glBitmap), non supportés par gl4es.
- Build : désactiver/stubber **GLEW** (`rGLEW.h`, loader desktop only).

## 3. Dépendances — état des lieux

### Déjà disponibles en binaire armeabi-v7a/android-16 (réutilisés du port AssaultCube)
Stagés dans `android/app/src/main/cpp/lib|include/` :
| Dep | Fichier | Source |
|-----|---------|--------|
| **gl4es** | `lib/gl4es/armeabi-v7a/libGL.a` (+ headers) | AssaultCube |
| **SDL2** 2.0.x | `lib/SDL2/.../libSDL2.so` (+ `libhidapi.so`) | AssaultCube |
| **SDL2_image** | `lib/sdl_image/.../libSDL2_image.so` | AssaultCube |
| **FreeType** | `lib/freetype/armeabi-v7a/libfreetype.a` (+ headers) | `Mario 64 Ouya/freetype-build` — **vérifié elf32-littlearm / android-16** |
| **SDL2_mixer** 2.6.3 | `lib/sdl_mixer/armeabi-v7a/libSDL2_mixer.so` (+ `SDL_mixer.h`) | **port SRB2Kart `C:/srb2k/deps/mixerbuild`** — vérifié elf32-littlearm/android-16, NEEDED=libSDL2.so seul (codecs OGG/WAV intégrés stb_vorbis) |

### Sources disponibles (tarball `deps-android-1.3.tar.xz`, extrait dans `deps-staging/`)
À (re)compiler si besoin : `curl, libpng, zlib` (+ freetype/openal/ogg/vorbis déjà couverts).

### À CONSTRUIRE pour Armagetron (manquants)
| Dep | Pourquoi | Plan |
|-----|----------|------|
| ~~protobuf~~ 3.21.12 | ~~réseau (25 .proto)~~ | ✅ **CONSTRUIT** : `lib/protobuf/armeabi-v7a/libprotobuf.a` (statique, ARM) + 234 headers ; **protoc hôte 3.21.12** dans `android/tools/protoc.exe` (release prébuilt). Version 3.21.12 = dernière **sans abseil**. **Validé** : génère les 25 `.pb.cc/.pb.h` des protos Armagetron. CMake : `BUILD_TESTS/PROTOC_BINARIES/LIBPROTOC/WITH_ZLIB=OFF`. |
| ~~libxml2~~ 2.11.7 | ~~parsing ressources/config~~ | ✅ **CONSTRUIT** : `lib/libxml2/armeabi-v7a/libxml2.a` (statique) + 46 headers. CMake : `WITH_HTTP=ON` (nanohttp, pas de curl), `WITH_PYTHON/ICONV/LZMA/ICU/ZLIB/TESTS/PROGRAMS/MODULES=OFF`. |
| ~~FTGL~~ | ~~rendu texte~~ | ✅ **CONSTRUIT** : `lib/ftgl/armeabi-v7a/libftgl.a` (elf32-littlearm). Patchs : `CMakeLists` (skip `find_package(OpenGL)` si ANDROID) + `FTVectoriser.cpp:171` cast `char*→unsigned char*`. Headers freetype complets requis (sources + config générés). |
| ~~SDL2_mixer~~ | ~~audio~~ | ✅ **récupéré du port SRB2Kart** (`C:/srb2k`), plus à construire |
| **Boost** | **headers seulement** | shared_ptr/lexical_cast/any/variant/tuple/foreach = header-only. `boost::thread`/`system` → **fallback pthread** (cf. configure.ac). **Pas de cross-compile**, juste poser les en-têtes. |

## 4. Ordre de construction recommandé (étape 2)

1. **Boost headers** (copie) + **FTGL** (CMake/NDK contre freetype prêt) → quick wins.
2. **SDL2_mixer** (ndk-build, Android.mk SDL).
3. **libxml2** (configure `--without-python --with-http`, ou + curl).
4. **protobuf** : host protoc (MSVC/mingw) **et** lib ARM même version (CMake NDK, `protobuf_BUILD_TESTS=OFF`).
5. Générer les `.pb.cc` via le protoc hôte, puis monter la coquille Gradle+CMake et lister les 169+10 sources.

### Invocation type cross-compile (rappel pipeline)
```
cmake -S <src> -B <build> -G Ninja -DCMAKE_MAKE_PROGRAM=<sdk>/cmake/3.22.1/bin/ninja.exe \
  -DCMAKE_TOOLCHAIN_FILE=<ndk23>/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-16 -DCMAKE_BUILD_TYPE=Release
```

## 4bis. FTGL — blocage identifié + correctif (pour étape 2)
Le `CMakeLists.txt` de FTGL fait `find_package(OpenGL REQUIRED)` → **échoue sur Android** (pas
d'OpenGL/GLX desktop). FreeType, lui, est correctement trouvé (binaire réutilisé validé).
**Fix** : retirer/contourner ce `find_package` et compiler FTGL contre les **en-têtes GL de gl4es**
déjà stagées dans `cpp/include/GL/` (`gl.h`, **`glu.h`** — donc **GLU est couvert par gl4es**, ce qui
règle aussi l'usage de GLU dans Armagetron). FTGL statique n'a pas besoin de linker GL au build.
Headers staged : `cpp/include/{GL,freetype,gl4es}`. Libs staged :
`freetype/libfreetype.a, gl4es/libGL.a, SDL2/{libSDL2.so,libhidapi.so}, sdl_image/libSDL2_image.so`.

## 4ter. GLU manquant (à régler à l'assemblage)
gl4es fournit `glu.h` mais **PAS l'implémentation GLU**. Armagetron appelle 7 fonctions GLU :
`gluLookAt`, `gluPerspective`, `gluSphere`+`gluNewQuadric`+`gluDeleteQuadric`, `gluBuild2DMipmaps`,
`gluErrorString` (dans `eCamera/rGL/rTexture/rViewport/gExplosion`). FTGL `FTVectoriser` utilise
`gluTess*` mais **seulement pour les polices non-texture** (évité par `FONT_TYPE=texture`).
→ **RÉSOLU (étape 3)** : shim maison `cpp/glu_shim/glu_shim.c` → `lib/glu_shim/armeabi-v7a/libglu_shim.a`.
Implémente les 7 fonctions (math matricielle pour LookAt/Perspective via `glMultMatrixf`, sphère UV
pour gluSphere, `GL_GENERATE_MIPMAP` pour Build2DMipmaps) + setters quadric + **stubs gluTess**
(référencés par FTGL `FTVectoriser` mais jamais appelés avec `FONT_TYPE=texture`). Compilé/validé ARM.

## 4quater. Étape 3 — assemblage (coquille posée & validée au configure)
Toutes les dépendances sont prêtes (gl4es, SDL2+hidapi, SDL2_image, SDL2_mixer, FreeType, FTGL,
libxml2, protobuf+protoc, **glu_shim**, **Boost 1.82 headers** référencés depuis `deps-staging`).
Coquille créée sous `android/` :
- `app/src/main/cpp/CMakeLists.txt` — importe les 10 libs, globe ~180 `.cpp` Armagetron + 25 `.pb.cc`
  + glu_shim, include dirs (d'après les `_CXXFLAGS` des Makefile.am), link ordonné. **`cmake` configure
  OK (EXIT 0)**.
- `app/src/main/cpp/config.h` — config.h **écrit à la main** (basé sur `src/win32/aa_config.h`) :
  HAVE_LIBSDL_MIXER/FTGL/LIBXML2, ENABLE_ZONESV2, POSIX HAVE_*, pas de HAVE_BOOST_THREAD (→ pthread),
  pas de HAVE_GLEW.
- `app/src/main/cpp/gen/protobuf/` — 25 `.pb.cc/.pb.h` générés (protoc hôte).
- Gradle : `settings.gradle`, `build.gradle` (AGP 7.0.2), `app/build.gradle` (API16, NDK23,
  abiFilters armeabi-v7a, `-std=c++17 -frtti -fexceptions`), `gradle.properties` (JDK11).
- `AndroidManifest.xml` (GLES2, gamepad, `tv.ouya.intent.category.GAME` + LEANBACK), activité
  `ArmagetronActivity extends SDLActivity` avec `getLibraries()` ordonné.

**Reste pour un APK (phase itérative de compilation) :**
1. Copier les classes Java SDL2 `org.libsdl.app.*` (de SDL 2.0.14) + garde API-18 `BluetoothManager`.
2. Câbler le point d'entrée `main`→`SDL_main` (inclure `SDL_main.h` côté entrée Armagetron).
3. **Itérer `config.h` + erreurs de compile** des ~180 sources (macros HAVE_* manquantes, iconv,
   GLEW à neutraliser, entrée).
4. Patchs runtime (cf. §5 + AssaultCube) : contexte ES2, `LIBGL_NOTEST`, `FONT_TYPE=texture`,
   data dir `SDL_AndroidGetExternalStoragePath`, ordre `loadLibrary`, downscale 720p.
5. Copier les assets jeu (`config/`, `language/`, `textures/`, `models/`, `sound/`) dans les assets APK.

## 4quinquies. ★ NATIF LINKÉ — `libmain.so` construit (0 erreur, 0 symbole manquant)
La passe de compilation a abouti : **tout le moteur natif compile + linke** pour la cible OUYA
(ELF32 ARM, Android 16, NDK r23c). Corrections appliquées (toutes dans `android/app/src/main/cpp/`) :
- **`aa_config.h`** (le code inclut `aa_config.h`, pas `config.h`) = déblocage structurel ; +
  `DONTUSEMEMMANAGER` (les macros malloc/free maison cassent `std::free` de boost),
  `AA_DATADIR/AA_SYSCONFDIR/AA_PREFIX`, `tTrueVersion.h` écrit à la main.
- Exclusions du glob CMake : TUs de test/démo avec leur propre `main()` (l2/l3_demo, nettest, astat,
  memtest, testgl, engine/test.cpp, transfab), `mathexpr_c` (double de mathexpr), `eSound.cpp`
  (EXTRA_DIST — mais PAS eSoundMixer.cpp : exclure `/eSound\.cpp`), et **`gWinZone.cpp`** (zones-v1,
  `BUILDZONESV1` OFF par défaut ; on build zones-v2 = `zone/*.cpp` + zFortress).
- zlib+libpng construits/stagés (png.h pour rSysdep) ; `thirdparty/scrap/*.cpp` ajouté
  (get_scrap/init_scrap) ; **`vParser.cpp` généré depuis `vParser.ypp` via bison 3.8.2** (msys2).
- Build natif : `deps-staging/arma-cmake-check/libmain.so` (70 Mo non-strippé).

**Reste = packaging APK / runtime** (plus de la compilation) :
1. Classes Java SDL2 `org.libsdl.app.*` (+ garde Bluetooth API-18).
2. Entrée `main`→`SDL_main`.
3. Patchs runtime (ES2, LIBGL_NOTEST, FONT_TYPE=texture, data dir, downscale 720p).
4. Assets du jeu dans l'APK ; `gradlew assembleDebug`.

## 5. Pièges API-16 déjà connus (réappliquer — cf. AssaultCube/OUYA_PORT.md)
Contexte **ES2** avant `SDL_CreateWindow` ; `LIBGL_NOTEST=1` + `LIBGL_*` via constructeur précoce
(crash compilateur Cg Tegra 3) ; `System.loadLibrary` dans l'ordre des dépendances ; gardes
`BluetoothManager`(API18)/`StandardCharsets`(API19)/`Html.fromHtml`(API24) ; data dir via
`SDL_AndroidGetExternalStoragePath()` ; manifeste `landscape` + `tv.ouya.intent.category.GAME` ;
perf 720p downscale + surface `MATCH_PARENT` ; clavier `SDL_HINT_RETURN_KEY_HIDES_IME` ;
activer **RTTI + exceptions** (RTTI massif dans le système réseau/console).
