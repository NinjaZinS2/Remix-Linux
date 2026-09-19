// Compatibilidade com glibc antiga (.deb para Ubuntu 22.04 / Debian 12).
//
// A glibc >= 2.38 redireciona strtol/sscanf & cia. para simbolos novos
// (__isoc23_*) quando compilamos com _GNU_SOURCE. Um binario gerado numa distro
// nova entao exige GLIBC_2.38 e nao abre em distros mais antigas. Estes shims
// definem os simbolos novos localmente, delegando para as funcoes classicas,
// que existem em qualquer glibc. Nada muda no comportamento (a unica diferenca
// da versao C23 e aceitar prefixo 0b em bases 0/2, que nao usamos).
//
// Este arquivo NAO inclui <cstdlib>/<cstdio> de proposito: as declaracoes
// "reais" usam rotulos asm para amarrar ao simbolo classico.
#include <cstdarg>

extern "C" {

long real_strtol(const char*, char**, int) __asm__("strtol");
unsigned long real_strtoul(const char*, char**, int) __asm__("strtoul");
long long real_strtoll(const char*, char**, int) __asm__("strtoll");
unsigned long long real_strtoull(const char*, char**, int) __asm__("strtoull");
long real_wcstol(const wchar_t*, wchar_t**, int) __asm__("wcstol");
unsigned long real_wcstoul(const wchar_t*, wchar_t**, int) __asm__("wcstoul");
long long real_wcstoll(const wchar_t*, wchar_t**, int) __asm__("wcstoll");
unsigned long long real_wcstoull(const wchar_t*, wchar_t**, int) __asm__("wcstoull");
int real_vsscanf(const char*, const char*, va_list) __asm__("__isoc99_vsscanf");
int real_vfscanf(void*, const char*, va_list) __asm__("__isoc99_vfscanf");
int real_vswscanf(const wchar_t*, const wchar_t*, va_list) __asm__("__isoc99_vswscanf");

long __isoc23_strtol(const char* s, char** e, int b) { return real_strtol(s, e, b); }
unsigned long __isoc23_strtoul(const char* s, char** e, int b) { return real_strtoul(s, e, b); }
long long __isoc23_strtoll(const char* s, char** e, int b) { return real_strtoll(s, e, b); }
unsigned long long __isoc23_strtoull(const char* s, char** e, int b) { return real_strtoull(s, e, b); }
long __isoc23_wcstol(const wchar_t* s, wchar_t** e, int b) { return real_wcstol(s, e, b); }
unsigned long __isoc23_wcstoul(const wchar_t* s, wchar_t** e, int b) { return real_wcstoul(s, e, b); }
long long __isoc23_wcstoll(const wchar_t* s, wchar_t** e, int b) { return real_wcstoll(s, e, b); }
unsigned long long __isoc23_wcstoull(const wchar_t* s, wchar_t** e, int b) { return real_wcstoull(s, e, b); }

int __isoc23_vsscanf(const char* s, const char* f, va_list ap) { return real_vsscanf(s, f, ap); }
int __isoc23_sscanf(const char* s, const char* f, ...) {
    va_list ap; va_start(ap, f); int r = real_vsscanf(s, f, ap); va_end(ap); return r;
}
int __isoc23_vfscanf(void* st, const char* f, va_list ap) { return real_vfscanf(st, f, ap); }
int __isoc23_fscanf(void* st, const char* f, ...) {
    va_list ap; va_start(ap, f); int r = real_vfscanf(st, f, ap); va_end(ap); return r;
}
int __isoc23_vswscanf(const wchar_t* s, const wchar_t* f, va_list ap) { return real_vswscanf(s, f, ap); }
int __isoc23_swscanf(const wchar_t* s, const wchar_t* f, ...) {
    va_list ap; va_start(ap, f); int r = real_vswscanf(s, f, ap); va_end(ap); return r;
}

} // extern "C"

// ---- libm: a glibc nova (2.38/2.43) publicou versoes novas de fmod/sqrtf &
// cia. O binario passaria a exigir GLIBC_2.43 so por isso. O link usa
// -Wl,--wrap=sqrtf,... : toda referencia a sqrtf vira __wrap_sqrtf, que chama a
// versao base (GLIBC_2.2.5 no x86-64), identica em comportamento para nos.
// (Definir "sqrtf" diretamente nao serve: a referencia versionada resolvia
// para a propria definicao e virava recursao infinita.)
#if defined(__x86_64__) && defined(__GLIBC__)
extern "C" {
__asm__(".symver remix_old_sqrtf, sqrtf@GLIBC_2.2.5");
__asm__(".symver remix_old_atan2f, atan2f@GLIBC_2.2.5");
__asm__(".symver remix_old_asinf, asinf@GLIBC_2.2.5");
__asm__(".symver remix_old_acosf, acosf@GLIBC_2.2.5");
__asm__(".symver remix_old_fmod, fmod@GLIBC_2.2.5");
__asm__(".symver remix_old_fmodf, fmodf@GLIBC_2.2.5");
float remix_old_sqrtf(float); float remix_old_atan2f(float, float); float remix_old_asinf(float); float remix_old_acosf(float);
double remix_old_fmod(double, double); float remix_old_fmodf(float, float);
float __wrap_sqrtf(float x) { return remix_old_sqrtf(x); }
float __wrap_atan2f(float y, float x) { return remix_old_atan2f(y, x); }
float __wrap_asinf(float x) { return remix_old_asinf(x); }
float __wrap_acosf(float x) { return remix_old_acosf(x); }
double __wrap_fmod(double x, double y) { return remix_old_fmod(x, y); }
float __wrap_fmodf(float x, float y) { return remix_old_fmodf(x, y); }
}
#endif

// arc4random (GLIBC_2.36) e referenciada pela libstdc++ estatica
// (std::random_device). Implementacao local via getrandom(2), disponivel
// desde a glibc 2.25.
#include <sys/random.h>
#include <cstdint>
extern "C" uint32_t arc4random(void) {
    uint32_t v = 0;
    if (getrandom(&v, sizeof v, 0) != (ssize_t)sizeof v) { static uint32_t s = 2463534242u; s ^= s << 13; s ^= s >> 17; s ^= s << 5; v = s; }
    return v;
}
