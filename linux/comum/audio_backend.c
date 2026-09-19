// Implementacao do miniaudio (+ stb_vorbis para OGG) numa unidade C separada.
// player_ma.h so inclui as declaracoes. Compilado nos dois sistemas
// (gcc/zig cc no Linux, gcc do MinGW ou zig cc no Windows).
#define STB_VORBIS_HEADER_ONLY
#include "audio/stb_vorbis.c"

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "audio/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "audio/stb_vorbis.c"
