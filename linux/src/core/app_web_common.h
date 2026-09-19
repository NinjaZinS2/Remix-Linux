#pragma once
// Busca de capa na internet: parte comum (montagem da URL, parse do JSON da
// pagina de resultados). HTTP e decodificacao de imagem sao por plataforma.
#include "config.h"
#include <string>
#include <vector>
#include <cstdio>

static std::wstring UrlEncode(const std::wstring& s){
    std::string u8=WideToUtf8(s);std::string o;char b[8];
    for(unsigned char c:u8){
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')o.push_back((char)c);
        else{snprintf(b,8,"%%%02X",c);o+=b;}
    }
    return Utf8ToWide(o);
}
static void JsonUnescape(std::wstring& s){
    std::wstring o;o.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        if(s[i]==L'\\'&&i+1<s.size()){
            wchar_t n=s[++i];
            if(n==L'u'&&i+4<s.size()){int v=(int)remix_parse_hex(s,i+1,4);o.push_back((wchar_t)v);i+=4;}
            else if(n==L'/')o.push_back(L'/');
            else if(n==L'"')o.push_back(L'"');
            else if(n==L'\\')o.push_back(L'\\');
            else if(n==L'n')o.push_back(L'\n');
            else o.push_back(n);
        } else o.push_back(s[i]);
    }
    s.swap(o);
}
static void SplitUrl(const std::wstring& url,std::wstring& host,std::wstring& path){
    std::wstring u=url;size_t ps=u.find(L"://");
    if(ps!=std::wstring::npos)u=u.substr(ps+3);
    size_t sl=u.find(L'/');
    if(sl==std::wstring::npos){host=u;path=L"/";}
    else{host=u.substr(0,sl);path=u.substr(sl);}
}
// Extrai os pares (murl,turl) do JSON embutido na pagina de resultados.
static std::vector<std::pair<std::wstring,std::wstring>> ParseImageSearch(const std::string& html){
    std::vector<std::pair<std::wstring,std::wstring>> out;
    std::wstring h=Utf8ToWide(html);
    const std::wstring mkey=L"\"murl\":\"";
    const std::wstring tkey=L"\"turl\":\"";
    size_t pos=0;
    while(out.size()<30){
        size_t m=h.find(mkey,pos);
        if(m==std::wstring::npos)break;
        size_t ms=m+mkey.size(),me=ms;
        while(me<h.size()&&h[me]!=L'"'){if(h[me]==L'\\')me++;me++;}
        std::wstring murl=h.substr(ms,me-ms);JsonUnescape(murl);
        pos=me+1;
        std::wstring turl;
        size_t t=h.find(tkey,pos);
        if(t!=std::wstring::npos&&t-pos<4000){
            size_t ts=t+tkey.size(),te=ts;
            while(te<h.size()&&h[te]!=L'"'){if(h[te]==L'\\')te++;te++;}
            turl=h.substr(ts,te-ts);JsonUnescape(turl);
            pos=te+1;
        }
        if(!murl.empty())out.push_back({murl,turl});
    }
    return out;
}
// Tipo da imagem pelos bytes iniciais.
static const char* ImageTypeFromBytes(const std::string& d){
    if(d.size()>=8&&(unsigned char)d[0]==0x89&&d[1]=='P'&&d[2]=='N'&&d[3]=='G')return ".png";
    if(d.size()>=3&&(unsigned char)d[0]==0xFF&&(unsigned char)d[1]==0xD8)return ".jpg";
    if(d.size()>=2&&d[0]=='B'&&d[1]=='M')return ".bmp";
    if(d.size()>=6&&d.compare(0,4,"GIF8")==0)return ".gif";
    return nullptr;
}
static std::wstring WebSearchPath(const std::wstring& q){ return L"/images/search?q="+UrlEncode(q)+L"&count=35"; }
