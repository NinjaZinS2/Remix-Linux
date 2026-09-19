#pragma once
#include "platform.h"
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include "config.h"

struct Theme {
    std::wstring id;
    std::wstring name;
    COLORREF accent;
};

inline std::vector<Theme> BuiltinThemes() {
    return {
        { L"azul",  L"AZUL",  RGB(40, 150, 255) },
        { L"roxo",  L"ROXO",  RGB(160, 80, 255) },
        { L"rosa",  L"ROSA",  RGB(255, 60, 140) },
        { L"cian",  L"CIAN",  RGB(0, 210, 200)  },
        { L"ambar", L"AMBAR", RGB(255, 175, 40) },
        { L"cinza", L"CINZA", RGB(200, 200, 210) },
    };
}

// Le <pasta>/*.theme.ini (lido em UTF-8 explicito, igual aos outros .inis).
// Formato de cada arquivo:
//   name=Meu Tema
//   color=255,80,10
inline void LoadThemesFrom(const std::wstring& dir, std::vector<Theme>& themes) {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(dir), ec)) return;
    for (std::filesystem::directory_iterator it(std::filesystem::path(dir), ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::directory_entry& e = *it;   // incremento com error_code: o range-for lancava excecao
        std::error_code e2;
        if (!e.is_regular_file(e2)) continue;
        auto ext = e.path().extension().wstring();
        for (auto& c : ext) c = towlower(c);
        if (ext != L".ini") continue;

        std::vector<std::wstring> lines;
        if (!ReadAllUtf8Lines(e.path().wstring(), lines)) continue;

        Theme t;
        t.id = e.path().stem().wstring();
        t.name = t.id;
        int rgb[3] = { 255, 255, 255 };
        for (auto& line : lines) {
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring k = Config::Trim(line.substr(0, eq));
            std::wstring v = Config::Trim(line.substr(eq + 1));
            if (k == L"name") t.name = v;
            else if (k == L"color") {
                // "r,g,b" — parse manual (sem swscanf) pra nao depender de simbolos novos da glibc
                size_t pos = 0;
                for (int i = 0; i < 3 && pos <= v.size(); ++i) {
                    size_t comma = v.find(L',', pos);
                    std::wstring part = v.substr(pos, comma == std::wstring::npos ? std::wstring::npos : comma - pos);
                    rgb[i] = std::max(0, std::min(255, remix_parse_int(part)));
                    if (comma == std::wstring::npos) break;
                    pos = comma + 1;
                }
            }
        }
        t.accent = RGB(rgb[0], rgb[1], rgb[2]);
        bool dup = false;
        for (auto& o : themes) if (o.id == t.id) { dup = true; break; }
        if (!dup) themes.push_back(t);
    }
}

// Temas embutidos + assets/themes empacotados + assets/themes da pasta do
// usuario (no Windows as duas pastas sao a mesma).
inline std::vector<Theme> LoadAllThemes() {
    auto themes = BuiltinThemes();
    std::wstring packaged = Config::Join(Config::AssetDir(), L"themes");
    std::wstring user = Config::Join(Config::Join(Config::BaseDir(), L"assets"), L"themes");
    LoadThemesFrom(packaged, themes);
    if (Config::NormSep(user) != Config::NormSep(packaged)) LoadThemesFrom(user, themes);
    return themes;
}

inline Theme FindTheme(const std::vector<Theme>& themes, const std::wstring& id) {
    for (auto& t : themes) if (t.id == id) return t;
    return themes.front();
}
