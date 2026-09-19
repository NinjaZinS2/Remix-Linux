#pragma once
// Busca de capa na internet (Linux): HTTP via libcurl (dlopen) e miniaturas
// decodificadas na thread (CPU) e viradas em textura na thread principal.
// A parte comum (URLs, parse) esta em app_web_common.h.
#include "app_state.h"
#include "app_web_common.h"

static void EvictThumb(const std::wstring& path){ PlatformEvictThumb(path); }
static std::string HttpGetBytes(const std::wstring& host,const std::wstring& path,std::wstring* outType=nullptr,DWORD* status=nullptr){
    std::string ct; long st=0;
    std::string out=sys::HttpGet("https://"+WideToUtf8(host)+WideToUtf8(path), outType?&ct:nullptr, status?&st:nullptr);
    if(outType) *outType=Utf8ToWide(ct);
    if(status) *status=(DWORD)st;
    return out;
}
// thread principal: libera texturas e imagens pendentes
static void ClearWebResultsPlatform(){
    WebPick& wb=WP();
    for(auto&r:wb.res){
        if(r.img){gfx::FreeImg((Img*)r.img);r.img=nullptr;}
        if(r.cpu){Image* im=(Image*)r.cpu; UnloadImage(*im); delete im; r.cpu=nullptr;}
    }
    wb.res.clear();wb.cells.clear();
}
static void WebSearchAsync(std::wstring q){
    WebPick& wb=WP();
    unsigned gen=(unsigned)++wb.gen;
    wb.searching=true;
    {
        std::lock_guard<std::mutex> lk(wb.m);
        ClearWebResultsPlatform();
        wb.sel=-1;wb.scroll=0;
        wb.status=sys::HttpAvailable()?L"Buscando imagens...":L"libcurl não encontrada no sistema: busca indisponível.";
    }
    if(!sys::HttpAvailable()){wb.searching=false;return;}
    std::thread([q,gen](){
        WebPick& wb=WP();
        std::string html=HttpGetBytes(L"www.bing.com",WebSearchPath(q));
        if(gen!=(unsigned)wb.gen.load())return;
        auto pr=ParseImageSearch(html);
        {
            std::lock_guard<std::mutex> lk(wb.m);
            if(gen!=(unsigned)wb.gen.load())return;
            for(auto&p:pr)wb.res.push_back(WebRes{p.first,p.second,nullptr,nullptr});
            wb.status=wb.res.empty()?L"Nada encontrado. Tente outras palavras.":L"Carregando miniaturas...";
        }
        sys::Post(EV_REDRAW);
        for(size_t i=0;i<pr.size();++i){
            if(gen!=(unsigned)wb.gen.load())return;
            if(pr[i].second.empty())continue;
            std::wstring host,path2;SplitUrl(pr[i].second,host,path2);
            std::string img=HttpGetBytes(host,path2);
            if(img.size()>800){
                const char* type=ImageTypeFromBytes(img);
                if(type){
                    Image im=LoadImageFromMemory(type,(const unsigned char*)img.data(),(int)img.size());
                    if(im.data){
                        std::lock_guard<std::mutex> lk(wb.m);
                        if(gen==(unsigned)wb.gen.load()&&i<wb.res.size()&&!wb.res[i].cpu&&!wb.res[i].img){wb.res[i].cpu=new Image(im);im.data=nullptr;}
                        if(im.data)UnloadImage(im);
                    }
                }
            }
            sys::Post(EV_REDRAW);
        }
        {
            std::lock_guard<std::mutex> lk(wb.m);
            if(gen==(unsigned)wb.gen.load()){wb.status=L"Clique numa imagem e aperte USAR ESSA.";wb.searching=false;}
        }
        sys::Post(EV_REDRAW);
    }).detach();
}
static void WebDownloadSelectedAsync(){
    WebPick& wb=WP();
    std::wstring url;
    {
        std::lock_guard<std::mutex> lk(wb.m);
        if(wb.sel<0||wb.sel>=(int)wb.res.size()||wb.downloading)return;
        url=wb.res[(size_t)wb.sel].murl;
        wb.downloading=true;
        wb.status=L"Baixando imagem em tamanho cheio...";
    }
    std::thread([url](){
        std::wstring host,path;SplitUrl(url,host,path);
        std::wstring ctype;std::string data=HttpGetBytes(host,path,&ctype);
        auto&W=WDS();
        std::lock_guard<std::mutex> lk2(W.m);
        W.ok=false;W.path.clear();
        if(data.size()>3000){
            std::wstring ext=L".jpg";
            const char* t=ImageTypeFromBytes(data);
            if(t)ext=Utf8ToWide(t);
            else if(ctype.find(L"png")!=std::wstring::npos)ext=L".png";
            else if(ctype.find(L"webp")!=std::wstring::npos)ext=L".webp";
            else if(ctype.find(L"bmp")!=std::wstring::npos)ext=L".bmp";
            std::wstring f=sys::TempDir()+L"/remix_cover_full"+ext;
            if(sys::WriteFileBytes(f,data)){
                Image chk=LoadImage(WideToUtf8(f).c_str());
                if(chk.data){UnloadImage(chk);W.ok=true;W.path=f;}
            }
        }
        sys::Post(EV_WEB_DOWNLOAD_DONE);
    }).detach();
}
static void ConsumeWebDownload(){
    WebPick& wb=WP();
    auto&W=WDS();
    std::lock_guard<std::mutex> lk2(W.m);
    wb.downloading=false;
    if(W.ok&&!W.path.empty()){
        ApplyCoverPick(wb.track,W.path);
        std::error_code ec; std::filesystem::remove(std::filesystem::path(W.path),ec);
        W.path.clear();W.ok=false;
        wb.open=false;wb.editing=false;
    } else {
        std::lock_guard<std::mutex> lkw(wb.m);
        wb.status=L"Falha ao baixar. Tente outra imagem.";
    }
}
// Miniatura pronta (textura) para a celula i; sobe a imagem CPU se preciso. Chamar com wb.m travado.
static Img* WebThumb(WebRes& r){
    if(!r.img&&r.cpu){ Image* im=(Image*)r.cpu; r.img=gfx::ImgFromImage(*im); delete im; r.cpu=nullptr; }
    return (Img*)r.img;
}
