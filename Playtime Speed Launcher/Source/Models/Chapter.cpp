#include "Models/Chapter.h"
#include "Core/Files.h"

namespace SpeedrunLauncher::Models {

std::wstring ChapterPreset::Command() const {
    return L"download_depot " + std::to_wstring(appId) + L" " + std::to_wstring(depotId) + L" " + manifestId;
}

bool Chapter::IsInstalled() const { return gameExePath && !gameExePath->empty() && Files::Exists(*gameExePath); }

std::vector<Chapter> Chapter::All() {
    std::vector<Chapter> all;
    {
        Chapter c;
        c.number = 1;
        c.title = L"UN FUERTE APRETÓN";
        c.subTitle = L"Chapter 1: A Tight Squeeze";
        c.description = L"Como exempleado de Playtime Co., al fin vuelves a la fábrica muchos años después de que todos adentro desaparecieron.";
        c.steamFolderName = L"Poppy Playtime";
        c.subFolderName = L"WindowsNoEditor\\Poppy_Playtime";
        c.steamAppId = 1721470;
        c.presets = {
            { L"Any% <1.1", 1721470, 1721471, L"8897518061680100138", L"5,79 GB" },
            { L"NMG <1.1", 1721470, 1721471, L"5760656490992572463", L"5,79 GB" },
            { L"NMG/Any% 1.2", 1721470, 1721471, L"1908539100791719856", L"5,79 GB" },
            { L"Parche 1.3", 1721470, 1721471, L"1791252873090904500", L"5,79 GB" },
        };
        all.push_back(c);
    }
    {
        Chapter c;
        c.number = 2;
        c.title = L"ATRAPADO EN LA RED";
        c.subTitle = L"Chapter 2: Fly in a Web";
        c.description = L"Profundiza en los oscuros secretos de Playtime Co. mientras escapas de las garras de Mommy Long Legs en la Game Station.";
        c.steamFolderName = L"Poppy Playtime";
        c.subFolderName = L"WindowsNoEditor\\Playtime_Prototype4";
        c.steamAppId = 1882830;
        c.presets = {
            { L"Parche 1.0", 1817490, 1817491, L"3472064775658982753", L"8,40 GB" },
            { L"Parche 1.1", 1817490, 1817491, L"2957411760343078601", L"8,40 GB" },
            { L"Parche 1.2", 1817490, 1817491, L"6474053569987505700", L"8,40 GB" },
        };
        all.push_back(c);
    }
    {
        Chapter c;
        c.number = 3;
        c.title = L"SUEÑO PROFUNDO";
        c.subTitle = L"Chapter 3: Deep Sleep";
        c.description = L"Desciende al Game Station y enfrenta los horrores que acechan en los niveles más profundos de la fábrica abandonada.";
        c.steamFolderName = L"Poppy Playtime";
        c.subFolderName = L"PoppyPlaytime_Chapter3";
        c.steamAppId = 2395280;
        c.presets = {
            { L"Parche 1.0", 2555190, 2555198, L"6195016546179599385", L"36,5 GB" },
        };
        all.push_back(c);
    }
    {
        Chapter c;
        c.number = 4;
        c.title = L"REFUGIO";
        c.subTitle = L"Chapter 4: Safe Haven";
        c.description = L"Aventúrate por los oscuros pasillos de la fábrica en busca de la verdad sobre los experimentos de Playtime Co.";
        c.steamFolderName = L"Poppy Playtime";
        c.subFolderName = L"Playtime_Chapter4";
        c.steamAppId = 2877340;
        c.presets = {
            { L"Parche 1.0", 3008670, 3008670, L"692859848352034537", L"7,25 GB" },
        };
        all.push_back(c);
    }
    {
        Chapter c;
        c.number = 5;
        c.title = L"COSAS ROTAS";
        c.subTitle = L"Chapter 5: Broken Things";
        c.description = L"El siguiente capítulo de la saga de Poppy Playtime está en desarrollo.";
        c.steamFolderName = L"Poppy Playtime";
        c.subFolderName = L"Chapter5";
        c.steamAppId = 4100940;
        c.presets = {
            { L"Parche 1", 4100940, 4100940, L"4806573455248764564", L"11,1 GB" },
            { L"Parche 2", 4100940, 4100940, L"2433159361022984191", L"11,1 GB" },
        };
        all.push_back(c);
    }
    {
        Chapter c;
        c.number = 6;
        c.title = L"PRÓXIMAMENTE";
        c.subTitle = L"Chapter 6";
        c.description = L"Coming Soon";
        c.steamFolderName = L"Poppy Playtime";
        c.isAvailable = false;
        all.push_back(c);
    }
    return all;
}

} // namespace SpeedrunLauncher::Models
