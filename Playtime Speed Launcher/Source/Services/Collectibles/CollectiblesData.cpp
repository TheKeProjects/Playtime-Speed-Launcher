#include "Services/Collectibles/CollectiblesData.h"

namespace SpeedrunLauncher::Services::Collectibles::Data {

namespace {

Item I(const char* key, const wchar_t* name, const wchar_t* area = L"", const wchar_t* hint = L"") { return { { key }, name, area, hint }; }
Item I(std::vector<const char*> keys, const wchar_t* name, const wchar_t* area = L"", const wchar_t* hint = L"") {
    return { std::move(keys), name, area, hint };
}

constexpr uint32_t kNotes = 0xFFCF5A, kDiaries = 0xC08BFF, kVhs = 0xFF5A76, kAudio = 0x4FC3FF, kToys = 0x5CE08A, kTrophies = 0xFFB13B;

// Chapter 1: no inventory keys of its own, the live reader names each tape by its color
// (Services\Collectibles\LiveInventory): "vhs:<color>" once got, "played:vhs:<color>" once in its VHS player.
const Chapter& Chapter1() {
    static const Chapter c{ 1, L"A Tight Squeeze", {
        { "vhs", "collect_cat_vhs", kVhs, {
            I("vhs:blue", L"Security Tape", L"Entrance", L"Azul"),
            I("vhs:green", L"Leith Pierre", L"Entrance", L"Verde"),
            I("vhs:yellow", L"Rich Avery", L"Storage", L"Amarilla"),
            I("vhs:pink", L"Stella Greyber", L"Machine Room", L"Rosa"),
            I("vhs:black", L"The Scientist", L"Catwalk", L"Negra"),
        } },
    } };
    return c;
}

// Chapter 2: named by the live reader / the save (Services\Collectibles\LiveInventory, SaveGameService):
// "vhs:<Name>" per tape (its item definition, <Name>VHSTape_Inventory_3DItem_C), "trophy:<Name>" per
// trophy collected (<Name>_Trophy_3DItem_C, saved by the game at once), "doc:<Name>" per note
// (Document<Name>_Inventory_3DItem_C).
const Chapter& Chapter2() {
    static const Chapter c{ 2, L"Fly in a Web", {
        { "vhs", "collect_cat_vhs", kVhs, {
            I("vhs:Ludwig", L"Elliot Ludwig's Biography", L"Intro"),
            I("vhs:JimmyRoth", L"Jimmy Roth", L"Water Treatment"),
            I("vhs:GreenHand", L"Green Hand", L"Molding Room"),
            I("vhs:Manager", L"Manager", L"After the water puzzle"),
            I("vhs:Scientist", L"The Prototype (Scientist)", L"Industrial Stairs"),
            I("vhs:Scepticye", L"Marcus Brickley", L"Spiral Staircase"),
            I("vhs:Rich", L"Rich", L"Revoked Toys"),
        } },
        { "notes", "collect_cat_notes", kNotes, {
            I("doc:814", L"Document 814"),
            I("doc:Construction", L"Construction"),
            I("doc:Matteo", L"Matteo Lata"),
            I("doc:Proctoring", L"Proctoring"),
            I("doc:SaraAbella", L"Sara Abella"),
            I("doc:Transfer", L"Transfer"),
            I("doc:Windows", L"Windows"),
        } },
        { "trophies", "collect_cat_trophies", kTrophies, {
            I("trophy:Daisy", L"Daisy", L"Intro"),
            I("trophy:Barry", L"Barry", L"Water Treatment (conveyor puzzle)"),
            I("trophy:GreenHand", L"Green Hand", L"Molding Room"),
            I("trophy:Claw", L"Claw", L"HUB"),
            I("trophy:Train", L"Mommy's Loco Motive (Train)", L"HUB"),
            I("trophy:PJ", L"PJ Pugapillar", L"Window escape hallway"),
            I("trophy:Bunzo", L"Bunzo", L"Revoked Toys"),
            I("trophy:Kissy", L"Kissy Missy", L"Spiral Staircase"),
            I("trophy:Mommy", L"Mommy Long Legs", L"The end"),
        } },
    } };
    return c;
}

// Chapter 3's ItemDataTable: the Name of each row is what the inventory map (and Playtime.sav) keeps.
// The tapes' names include the quotes. Not tapes: Tape_01 ("Tape 1", a placeholder row) and Tape_12
// ("THE HOUR OF JOY", in no map). In the order they're found.
const Chapter& Chapter3() {
    static const Chapter c{ 3, L"Deep Sleep", {
        { "vhs", "collect_cat_vhs", kVhs, {
            I("\"Claire Harper Debrief\"", L"Claire Harper Debrief", L"Intro Tunnels", L"Azul: tras el primer conducto"),
            I("\"Grabpack 2.0 Prototype\"", L"Grabpack 2.0 Prototype", L"Gas Production Zone", L"Lima: donde consigues el GrabPack 2.0"),
            I("\"WELCOME TO PLAYTIME\"", L"WELCOME TO PLAYTIME", L"Nightmare", L"Roja: al final de la pesadilla"),
            I("\"#1322 Report: VOID\"", L"#1322 Report: VOID", L"Home Sweet Home", L"Marrón: primera planta"),
            I("\"Samuel Lee's Last Day\"", L"Samuel Lee's Last Day", L"Home Sweet Home", L"Verde: segunda planta"),
            I("\"(Illegible Writing)\"", L"(Illegible Writing)", L"School", L"Amarilla: tras el segundo puzle eléctrico"),
            I("\"Stuart & Rich\"", L"Stuart & Rich", L"Caves", L"Naranja: en las cuevas, después de la Escuela"),
            I("\"CatNap Check-In\"", L"CatNap Check-In", L"Playhouse", L"Morada: después de las cuevas"),
            I("\"8/8/95 EMERGENCY ALERT\"", L"8/8/95 EMERGENCY ALERT", L"Office", L"Blanca: cerca del inicio de la Oficina"),
            I("\"Hartmann Incident\"", L"Hartmann Incident", L"Office", L"Rosa: sala de la llave del conducto"),
            I("\"Log 24459\"", L"Log 24459", L"CatNap", L"Negra: a la izquierda del ascensor, pelea con CatNap"),
        } },
        { "notes", "collect_cat_notes", kNotes, {
            I("Note 1", L"Teacher Note A", L"School"),
            I("Note 2", L"Teacher Note B", L"School"),
            I("Note 3", L"Teacher Note C", L"School"),
            I("Note 4", L"Teacher Note D", L"School"),
            I("Note 5", L"Teacher Note E", L"School"),
            I("Note 6", L"Teacher Note F", L"School"),
        } },
    } };
    return c;
}

const Chapter& Chapter4() {
    static const Chapter c{ 4, L"Safe Haven", {
        { "notes", "collect_cat_notes", kNotes, {
            I("RedSmokeDocumentation", L"Caution: Red Smoke"),
            I("CellBlocks_Observations1_Note", L"Weekly Checkup: 1172", L"Prison Cell Blocks"),
            I("CellBlocks_Observations2_Note", L"Weekly Checkup: 1424", L"Prison Cell Blocks"),
            I("CellBlocks_1163PrisonProfile_Note", L"Prisoner Profile, Experiment 1163", L"Prison Cell Blocks"),
            I("CellBlocks_1166PrisonerProfile_Note", L"Prisoner Profile, Experiment 1166", L"Prison Cell Blocks"),
            I("UtilityZone_ReconReport1_Note", L"Reconnaissance Report: 12.15.04", L"Prison Utility Zone"),
            I("SafeHaven_ReconReport2_Note", L"Reconnaissance Report: 01.10.05", L"Safe Haven"),
            I("SafeHaven_UntitledPoem_Note", L"An Untitled Poem", L"Safe Haven"),
            I("SafeHaven_JacksParents_Note", L"A Cryptic Letter", L"Safe Haven"),
            I("SafeHaven_SafeHavenProjectOverview", L"Project Overview: Safe Haven", L"Safe Haven"),
            I("WasteWaterPlant_Subject1322C_Note", L"Subject 1322C Report", L"Waste Water Plant"),
            I("NoMansLand_Propaganda_Notes", L"Propaganda Flier", L"No Man's Land"),
            I("DoctorsDomain_MissionStatement_Notes", L"Secondary Lab Mission Statement", L"Doctor's Domain"),
            I("DoctorsDomain_HSKT_Doc", L"Secondary Lab Instructions", L"Doctor's Domain"),
            I("DoctorsHQ_HiddenDoc1_Notes", L"Young Geniuses Program Removal Letter", L"Doctor's Headquarters"),
            I("Foundation_AccelerantNote", L"Foundation Operator Note", L"The Foundation"),
        } },
        { "diaries", "collect_cat_diaries", kDiaries, {
            I("ToyGraveyard_Diary_00", L"Riley Journal: Entry #1", L"Toy Graveyard", L"Sobre un barril en el Checkpoint 1A"),
            I("ToyGraveyard_Diary_01", L"Riley Journal: Entry #2", L"Toy Graveyard"),
            I("ToyGraveyard_Diary_02", L"Riley Journal: Entry #3", L"Toy Graveyard"),
            I("ToyGraveyard_Diary_03", L"Riley Journal: Entry #4", L"Toy Graveyard"),
            I("ToyGraveyard_Diary_04", L"Riley Journal: Entry #5", L"Toy Graveyard"),
            I("CellBlocks_GuardNote1_Note", L"Cole Journal: Entry #1", L"Prison Cell Blocks"),
            I("CellBlocks_GuardNote2_Note", L"Cole Journal: Entry #2", L"Prison Cell Blocks"),
            I("CellBlocks_GuardNote3_note", L"Cole Journal: Entry #3", L"Prison Cell Blocks"),
            I("CellBlocks_GuardNote4_Note", L"Cole Journal: Entry #4", L"Prison Cell Blocks"),
        } },
        { "vhs", "collect_cat_vhs", kVhs, {
            I("FlareGunSafetyVideo", L"Flare Hand Safety Video", L"", L"Oficina junto al Checkpoint 1A, en la silla"),
            I("BreakTimeVHSTape", L"Break Time", L"", L"Utility Room, junto al tren (llave del Utility Room)"),
            I("PrisonCellBlock_VHS_WardenOffice", L"Warden Office 8/8/1995", L"", L"Despacho del alcaide, recepción de la prisión"),
            I("TheDoctorVHSTape", L"The Doctor", L"", L"Sala de servidores donde conoces a Yarnaby"),
            I("StellaTape", L"Stella Greyber Project Audit", L"", L"Archivos, tras la persecución de Yarnaby"),
            I("TheaterIncidentVHSTape", L"'93 Theater Incident", L"", L"Almacén del pasillo en T, tras conocer a Doey"),
            I("OllieandPoppyVHSTape", L"Ollie", L"", L"Estantería a la izquierda tras conocer a Poppy y Kissy Missy"),
            I("ConstructionPlansTape", L"Construction Plans: The Shelf", L"", L"Bajo una cama de la enfermería de Safe Haven"),
            I("DoeyKevinTape", L"1322 Report: Kevin", L"", L"Estanterías del fondo, sala de tratamiento de agua"),
            I("CivilWar1VHSTape", L"Security Feed 02.09.01", L"", L"Sala tras usar la cápsula, después de Yarnaby"),
            I({ "DoctorsAwakeningVHSTape", "FixedExperiment1354Tape" }, L"Experiment #1354", L"", L"Sobre un Smiling Critter, laberinto de gas"),
            I("DoctorPrototypeVHSTape", L"Time", L"", L"Despacho de Sawyer, sala de consolas del Doctor"),
            I("ManagementRichTape", L"Management: Rich", L"", L"Estantería de la zona vallada de Safe Haven"),
            I("1322Report:Jack", L"1322 Report: Jack", L"", L"Sobre un barril en The Foundation, por la pasarela"),
            I("UntitledPoppyVHSTape", L"Untitled Tape", L"", L"Estante bajo de la cueva, durante la persecución de Doey"),
            I("CivilWar2Video", L"A Reminder", L"", L"Oficina del almacén, tras el jefe Doey"),
            I("TopSecretVideo", L"TOP SECRET", L"", L"Final del capítulo: la cinta blanca (vuelve Huggy). Las guías no la cuentan para el logro"),
        } },
    } };
    return c;
}

const Chapter& Chapter5() {
    static const Chapter c{ 5, L"Broken Things", {
        { "notes", "collect_cat_notes", kNotes, {
            I("ID_Notes_Level01_InterceptedCommunique", L"An Intercepted Communique", L"Harvest Processing Center 04"),
            I("ID_Notes_Level01_PlaytimeManifest", L"Shipping Manifest - July '95", L"Harvest Processing Center 04"),
            I("ID_Notes_Level01_ToyManifest", L"Inventory - September '95", L"Harvest Processing Center 04"),
            I("ID_Notes_Level03_MorgueNote", L"Work Procedures - Recycling & Reclamation", L"Recycling & Reclamation"),
            I("ID_Notes_Level03_OutimalNote", L"Outimal Note", L"Recycling & Reclamation"),
            I("ID_Notes_Level03_OutimalBrandingNote", L"Wrongside Outimal Branding Note", L"Recycling & Reclamation"),
            I("ID_Notes_Level04_GibletLetter", L"Giblet's Checklist", L"The Boilers"),
            I("ID_Notes_Level04_BetterPressNewspaper", L"The Better Press, Issue 14", L"The Boilers"),
            I("ID_Notes_Level05_LivingFruits", L"Reminder", L"Research & Development"),
            I("ID_Notes_Level05_PoppyGelPlants", L"Lab Report: Medium Growth & Plantlife", L"Research & Development"),
            I("ID_Notes_Level05_ScientistPersonalLetter1", L"Dr. Arkins Personal Letter 1", L"Research & Development"),
            I("ID_Notes_Level05_MissingPrototypeHands", L"GrabPack Hands Rejection", L"Research & Development"),
            I("ID_Notes_Level05_MasterBackup", L"Master Backup?", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide1", L"Negation Compound Presentation (1 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide2", L"Negation Compound Presentation (2 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide3", L"Negation Compound Presentation (3 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide4", L"Negation Compound Presentation (4 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide5", L"Negation Compound Presentation (5 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_NegationPresentationSlide6", L"Negation Compound Presentation (6 of 6)", L"Research & Development", L"Imperdible (historia)"),
            I("ID_Notes_Level05_LeithLetter3", L"Letter To Leith #2", L"Research & Development"),
            I("ID_Notes_Level05_MedicalReport", L"Medical Report: Edward M.N. Ritterman", L"Research & Development"),
            I("ID_Notes_Level05_SurvivorNote2", L"To Whomever Finds This", L"Research & Development"),
            I("ID_Notes_Level06_ExperimentsObservations", L"Conditioning Observations", L"Testing"),
            I("ID_Notes_Level08_LilyProtoObs", L"Observation: Lily", L"Testing"),
            I("ID_Notes_Level06_LetterstoLeith", L"Letter To Leith #1", L"Testing"),
            I("ID_Notes_Level07_ScientistLetter2", L"Dr. Arkins Personal Letter 2", L"Extreme Aggression Under Stress"),
            I("ID_Notes_Level09_LilyHairNote", L"Experiment 1468 Proposal", L"The Doll House"),
            I("ID_Notes_Level09_MusicSheet1", L"Music Sheet - Novice", L"The Doll House"),
            I("ID_Notes_Level09_NurseryDamage", L"Nursery Damage", L"The Doll House"),
            I("ID_Notes_Level09_SweetStreetLog", L"Observation Log: Sweet Street", L"The Doll House"),
            I({ "ID_Notes_Level09_MusicSheet2", "ID_Notes_Level09_MusicSheet3", "ID_Notes_Level09_MusicSheet4" }, L"Music Sheet - Expert", L"The Doll House"),
            I("ID_Notes_Level09_LilyJournalEntry", L"Lily Journal", L"The Doll House"),
            I("ID_Notes_Level11_LabNote1", L"Dr. Arkins Personal Letter 3", L"Reanimation"),
            I("ID_Notes_Level11_ProtoObservation", L"Experiment Notes", L"Reanimation"),
            I("ID_Notes_Level12_LeithJournal", L"L.P. Journal Entry", L"The End of the Line"),
        } },
        { "vhs", "collect_cat_vhs", kVhs, {
            I("ID_VHS_HuggyReanimation", L"Experiment 1170: Executive Showcase", L"Testing"),
            I("ID_VHS_Level08", L"Three P's of Diorama Safety", L"Sweet Street"),
            I("ID_VHS_Level11", L"How to Maintain Your-- (Illegible)", L"Reanimation"),
        } },
        { "audio", "collect_cat_audio", kAudio, {
            I("ID_Cassette_PoppyKissy", L"Sisters", L"Recycling & Reclamation"),
            I("ID_Cassette_Level04_Case", L"The Case", L"The Boilers"),
            I("ID_Cassette_Level04_SecurityLog", L"Security Log - 08081995-16", L"The Boilers"),
            I("ID_Cassette_Level05_OutimalsFocusGroup", L"Focus Test: The Wrongside Outimals", L"Research & Development"),
            I("ID_Cassette_Level05_PierreAndRitterman", L"Pierre/Ritterman, August '95", L"Research & Development"),
            I("ID_Cassette_GracieChum", L"Field Test: Chum Chompkins", L"Testing"),
            I("ID_Cassette_Level08_Gracie", L"'96 Isolation Day 122", L"Sweet Street"),
            I("ID_Cassette_Level09_AJustEnd", L"A Just End", L"The Doll House"),
            I("ID_Cassette_Level11_ElliotProto", L"Elliot / Prototype", L"Reanimation"),
            I("ID_Cassette_Level11_ProtoInterrogates", L"Interrogation", L"Reanimation"),
        } },
        { "toys", "collect_cat_toys", kToys, {
            I("ID_Collectible_BindleBoogie", L"Outimal: Bindle Boogie (plush)", L"Recycling & Reclamation"),
            I("ID_Collectible_Level04_GooberGutzPlushie", L"Outimal: Goober GUTZ (plush)", L"The Boilers"),
            I("ID_Collectible_Level04_GoldenGibletTrophy", L"Golden Giblet", L"The Boilers"),
            I("ID_Collectible_Level04_GooberGutzDoll", L"Outimal: GOOBER Gutz (doll)", L"The Boilers"),
            I("ID_Collectible_BindleBoogieDoll", L"Outimal: Bindle BOOGIE (doll)", L"Research & Development"),
            I("ID_Collectible_GoldenChumTrophy", L"Golden Chum", L"Research & Development"),
            I("ID_Collectible_BlankyBlankzDoll", L"Outimal: Blanky BLANKZ (plush)", L"Extreme Aggression Under Stress"),
            I("ID_Collectible_BlankyBlankz", L"Outimal: BLANKY Blankz (doll)", L"Extreme Aggression Under Stress"),
            I("ID_Collectible_Lvl8_BooBooDoll", L"Outimal: Dizzy BOOBOO (plush)", L"Sweet Street"),
            I("ID_Collectible_Lvl8_BooBoo", L"Outimal: DIZZY Booboo (doll)", L"Sweet Street"),
            I("ID_Outimal_Level09_DoodleBones_Doll", L"Outimal: Doodle Bones (doll)", L"The Doll House"),
            I("ID_Collectible_GoldenLilyTrophy", L"Golden Lily", L"The Doll House"),
            I("ID_Notes_Level09_MsGracie_IDCard", L"Ms. Gracie's Playtime ID", L"The Doll House"),
            I("ID_Outimal_Level09_DoodleBones_Plushie", L"Outimal: Doodle Bones (plush)", L"The Doll House"),
            I("ID_Collectible_LoopsyNoodle", L"Outimal: LOOPSY Noodle (doll)", L"Reanimation"),
            I("ID_Collectible_RedSmoke", L"Red Smoke Sample", L"Reanimation"),
            I("ID_Collectible_GoldenLimonTrophy", L"Golden Limon", L"Reanimation"),
            I("ID_Collectible_LoopsyNoodleDoll", L"Outimal: Loopsy NOODLE (plush)", L"Reanimation"),
            I("ID_Collectible_Level12_GoldenPrototypeTrophy", L"Golden Prototype", L"The End of the Line"),
        } },
    } };
    return c;
}

} // namespace

int Chapter::Total() const {
    int n = 0;
    for (const Category& c : cats) n += static_cast<int>(c.items.size());
    return n;
}

const Chapter& Get(int number) { return number == 1 ? Chapter1() : number == 2 ? Chapter2() : number == 3 ? Chapter3() : number == 4 ? Chapter4() : Chapter5(); }

std::vector<std::vector<bool>> Collected(const Chapter& chapter, std::string_view text) {
    std::vector<std::vector<bool>> out;
    for (const Category& c : chapter.cats) {
        std::vector<bool> got(c.items.size(), false);
        for (size_t i = 0; i < c.items.size() && !text.empty(); ++i)
            for (const char* key : c.items[i].keys) {
                // Same matching as the overlay page: "\0<key>\0" (Chapters 1 to 4) / "/<key>." (Chapter 5)
                const std::string needle = chapter.number != 5 ? std::string(1, '\0') + key + std::string(1, '\0') : "/" + std::string(key) + ".";
                if (text.find(needle) != std::string_view::npos) {
                    got[i] = true;
                    break;
                }
            }
        out.push_back(std::move(got));
    }
    return out;
}

bool TracksPlayed(const Chapter& chapter) { return chapter.number >= 1 && chapter.number <= 4; }

PlayedCount Played(const Chapter& chapter, std::string_view text) {
    PlayedCount out;
    if (!TracksPlayed(chapter)) return out;
    for (const Category& c : chapter.cats) {
        if (std::string_view(c.id) != "vhs") continue;
        out.total += static_cast<int>(c.items.size());
        for (const Item& item : c.items)
            for (const char* key : item.keys)
                if (text.find(std::string(1, '\0') + "played:" + key + std::string(1, '\0')) != std::string_view::npos) {
                    ++out.played;
                    break;
                }
    }
    return out;
}

} // namespace SpeedrunLauncher::Services::Collectibles::Data
