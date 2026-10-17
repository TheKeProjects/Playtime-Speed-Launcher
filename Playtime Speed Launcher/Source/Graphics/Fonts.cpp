#include "Graphics/Fonts.h"
#include <array>
#include <initializer_list>
#include "Core/Files.h"
#include "Graphics/Device.h"

namespace SpeedrunLauncher::Fonts {

namespace {

std::array<Family, 5> families;

bool SystemHas(const wchar_t* name) {
    ComPtr<IDWriteFontCollection> system;
    if (FAILED(Device::Get().DWrite()->GetSystemFontCollection(&system))) return false;
    UINT32 index = 0;
    BOOL exists = FALSE;
    system->FindFamilyName(name, &index, &exists);
    return exists != FALSE;
}

std::wstring FirstInstalled(std::initializer_list<const wchar_t*> names) {
    for (const wchar_t* n : names)
        if (SystemHas(n)) return n;
    return *names.begin();
}

// Builds a private collection from one font file and returns its first family name.
bool LoadPrivate(const std::wstring& path, Family& out) {
    if (!Files::Exists(path)) return false;
    ComPtr<IDWriteFactory5> factory5;
    if (FAILED(Device::Get().DWrite()->QueryInterface(IID_PPV_ARGS(&factory5)))) return false;
    ComPtr<IDWriteFontSetBuilder1> builder;
    ComPtr<IDWriteFontFile> file;
    if (FAILED(factory5->CreateFontSetBuilder(&builder)) || FAILED(factory5->CreateFontFileReference(path.c_str(), nullptr, &file)) ||
        FAILED(builder->AddFontFile(file.Get())))
        return false;
    ComPtr<IDWriteFontSet> set;
    ComPtr<IDWriteFontCollection1> collection;
    if (FAILED(builder->CreateFontSet(&set)) || FAILED(factory5->CreateFontCollectionFromFontSet(set.Get(), &collection))) return false;
    if (collection->GetFontFamilyCount() == 0) return false;
    ComPtr<IDWriteFontFamily> family;
    ComPtr<IDWriteLocalizedStrings> names;
    if (FAILED(collection->GetFontFamily(0, &family)) || FAILED(family->GetFamilyNames(&names))) return false;
    UINT32 length = 0;
    names->GetStringLength(0, &length);
    std::wstring name(length + 1, L'\0');
    names->GetString(0, name.data(), length + 1);
    name.resize(length);
    out.name = name;
    out.collection = collection;
    return true;
}

} // namespace

void Initialize(const std::wstring& poppyFontPath) {
    families[static_cast<int>(Font::Mono)] = { FirstInstalled({ L"Cascadia Code", L"Consolas", L"Courier New" }), nullptr };
    families[static_cast<int>(Font::Icons)] = { FirstInstalled({ L"Segoe MDL2 Assets", L"Segoe Fluent Icons" }), nullptr };
    families[static_cast<int>(Font::Ui)] = { L"Segoe UI", nullptr };
    families[static_cast<int>(Font::Consolas)] = { FirstInstalled({ L"Consolas", L"Courier New" }), nullptr };
    Family poppy;
    if (LoadPrivate(poppyFontPath, poppy)) families[static_cast<int>(Font::Poppy)] = poppy;
    else families[static_cast<int>(Font::Poppy)] = families[static_cast<int>(Font::Mono)];
}

const Family& Get(Font font) { return families[static_cast<int>(font)]; }

} // namespace SpeedrunLauncher::Fonts
