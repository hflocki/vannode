// Truma fault codes with a short German text, for the heater-node and the display.
// Shared copy of firmware/_common/truma_errors.h (tools/common/sync_common.py) - edit it there.
//
// Codes as shown on the CP Plus without letters, e.g. "E 122 H" = 122. The heater family decides
// the meaning: the same number can mean something else on the Combi D (e.g. 121).
//   1. CP Plus code lists per family (gas Combi, Combi D, VarioHeat) with E (error, must be
//      acknowledged) or W (warning, clears itself). Compiled from the Truma manuals, as listed on
//      womo-teile.de/truma-fehlercodes; texts rephrased.
//   2. Gas Combi only, fallback: fault numbers of the Truma "Diagnostics catalogue Truma Combi"
//      (04.02.2019), translated and shortened. Kind unknown there.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace truma_err {

enum Family : uint8_t { GAS, DIESEL, VARIO };

// Family of a heater model key of the heater-node ("combi", "combi_e", "combi_d", "combi_d_e", "vario").
inline Family family(const char* model) {
  if (!model) return GAS;
  if (!strncmp(model, "combi_d", 7)) return DIESEL;
  if (!strcmp(model, "vario")) return VARIO;
  return GAS;
}

static const char* const TEXTS[] = {
  "Verbrennungsluftgebläse: Strom zu niedrig",
  "Verbrennungsluftgebläse: Strom zu hoch",
  "Fehler in der Elektronik",
  "Fühler Verbrennungsluft außerhalb der Toleranz",
  "Wasser überhitzt – Heizung abkühlen lassen, Boiler füllen",
  "Luft überhitzt – Warmluftauslässe und Ansaugung frei machen",
  "Elektronik passt nicht zum Gerät",
  "Wasserfühler außerhalb der Toleranz",
  "Raumtemperaturfühler oder Kabel defekt",
  "Luftfühler außerhalb der Toleranz",
  "Unterspannung: Batterie unter 10,4 V",
  "Umluftgebläse: Strom zu niedrig",
  "Umluftgebläse: Strom zu hoch",
  "Bedienteil oder Bedienteilkabel defekt",
  "FrostControl-Heizelement: Strom zu hoch",
  "Magnetventil 2 kW: Strom zu hoch",
  "Magnetventil 4 kW: Strom zu hoch",
  "Zünder: Strom zu hoch",
  "Gesperrt nach Überhitzung (Temperaturschalter)",
  "Fensterschalter: Fenster über dem Kamin offen",
  "12-V-Versorgung: Überspannung",
  "12-V-Versorgung: Unterspannung",
  "230-V-Versorgung gestört",
  "Umluftgebläse, Bedienteil oder Kabelbaum defekt",
  "Überhitzung (Temperaturschalter hat ausgelöst)",
  "Spannungsversorgung schwankt",
  "Zündfunke stört die Elektronik",
  "Umluftgebläse defekt",
  "Spannung schwankt oder Zündfunke stört die Elektronik",
  "Verbrennungsluftgebläse defekt",
  "Gerät zu heiß oder Fühler Verbrennungsluft falsch",
  "Kurzschluss an den Magnetventilen",
  "Kurzschluss am Zünder",
  "Flamme aus oder nicht erkannt (Brennstoff, Masseverbindung)",
  "Flammensignal im Standby (Ventil hängt offen)",
  "Wechselspannungsanteile in der 12-V-Versorgung",
  "Verbrennungsluftgebläse: Drehzahl zu niedrig",
  "Verbrennungsluftgebläse: Drehzahl zu hoch",
  "Flamme instabil (Brennstoffzufuhr prüfen)",
  "Brennstoffzufuhr unterbrochen (Gas leer/zu, Diesel leer oder Luft in der Leitung)",
  "Verbrennungsluftgebläse, Kabelbaum oder Elektronik defekt",
  "Keine Verbindung zwischen Heizung und Bedienteil (12 V, Kabel)",
  "Brennstoffzufuhr im Mix-Betrieb unterbrochen",
  "230-V-Versorgung: Überspannung",
  "Verbrennungsluftgebläse: Drehzahl außerhalb der Toleranz",
  "Magnetventile falsch angesteuert",
  "Überhitzung Luft: Temperaturschalter ausgelöst",
  "Überhitzung Wasser: Temperaturschalter ausgelöst (Boiler leer?)",
  "Kurzschluss Verbrennungsluftgebläse",
  "Zünder: Strom zu niedrig",
  "Sicherheitsventil: Strom zu hoch",
  "Gesperrt: mehr als 4 Resets in 15 min (Service-Taste an der Elektronik)",
  "CP Plus passt nicht zur Heizung – CP Plus neu initialisieren",
  "Gesperrt: Verbrennungsluft über 215 °C",
  "Heizung beim Einschalten schon unter Spannung – aus- und einschalten",
  "230-V-Temperaturschalter Wasser hat ausgelöst",
  "Warmwasser an, aber Boiler leer",
  "Warmluftaustritte oder Umluftansaugung blockiert",
  "Batteriespannung unter 10,4 V",
  "FrostControl-Heizelement: Kurzschluss",
  "Fenster über dem Kamin offen",
  "Überspannung über 16,4 V",
  "Unterspannung, Batterie zu schwach",
  "Keine 230 V (Netz, Sicherung oder Überhitzungsschutz)",
  "Flamme nicht erkannt oder Gas unterbrochen (Flasche leer, Ventil zu)",
  "Flamme aus oder instabil, Gasversorgung unterbrochen",
  "Flamme nicht erkannt: Zuluft/Abgas blockiert, Gasregler oder Elektronik defekt",
  "Überhitzung: Temperaturschalter offen oder Verbrennungsluft über 215 °C",
  "Flamme nicht erkannt oder Gas unterbrochen",
  "Butananteil in der Gasflasche zu hoch",
  "Keine 12 V an der Heizung oder keine Verbindung zum Bedienteil",
  "Warmluftrohre nicht angeschlossen oder Auslässe/Ansaugung blockiert",
  "Gasversorgung im Mischbetrieb unterbrochen",
  "Unterspannung droht: Batterie unter 10,4 V",
  "Gasversorgung unterbrochen (Flasche leer, Ventil zu)",
  "Gesperrt nach Überhitzung (Temperaturschalter offen)",
  "Gesperrt: mehr als 4 Quittierungen in 15 min – Ursache beheben",
  "230-V-Überhitzungsschutz ausgelöst",
  "Einstellungen des Bedienteils kommen nicht an (Verbindung Bedienteil – Heizung)",
  "Dosierpumpe unterbrochen oder Temperaturschalter ausgelöst",
  "Kraftstoffmangel (Tank fast leer, leer oder Schräglage)",
  "Keine Verbindung zwischen Heizung und Bedienteil",
  "Fehler in der Elektronik (12-V-Versorgung, FrostControl-Heizelement prüfen)",
  "Fehler ohne nähere Angabe im Truma-Katalog",
  "Warmlufttemperatur zu hoch",
  "Wassertemperatur im Heizbetrieb zu hoch",
  "Wassertemperatur im Warmwasserbetrieb zu hoch",
  "Ein Temperaturschalter hat ausgelöst",
  "Unterspannung droht: unter 11,5 V",
  "Keine Verbindung Heizung – Bedienteil, keine 12 V an der Heizung oder Kabel defekt",
  "Unterspannung droht: Batterie unter 10,8 V",
  "Erstbefüllung steht aus",
  "Erstbefüllung läuft, danach Regelbetrieb",
  "Heizung startet nicht (nach dem 5. Quittieren Betriebssperre)",
  "Startversuche ausgeschöpft: Betriebssperre",
  "Flamme wiederholt nicht erkannt; ein Startversuch zur Erstbefüllung möglich",
  "Flamme nicht erkannt: Gas leer/zu, Regler vereist, Butan zu hoch oder Zuluft/Abgas zu",
  "Unterspannung: Batterie unter 10 V",
};

struct Entry { uint16_t code; char kind; uint8_t text; };   // kind: 'E', 'W' or 0 = unknown

// CP Plus codes per family, sorted by code
static const Entry CP_GAS[] = {
  {17, 0, 56}, {18, 0, 57}, {21, 0, 8}, {24, 0, 58}, {29, 0, 59}, {42, 0, 60}, {43, 0, 61}, {44, 0, 62},
  {45, 0, 63}, {111, 'E', 2}, {112, 'E', 64}, {121, 'E', 65}, {122, 0, 66}, {131, 'E', 2}, {150, 'E', 67},
  {160, 'E', 2}, {161, 'E', 2}, {162, 'E', 2}, {170, 'E', 28}, {202, 0, 68}, {211, 0, 69}, {212, 0, 66},
  {255, 'W', 70}, {301, 'W', 20}, {401, 'W', 56}, {402, 'W', 71}, {407, 'W', 22}, {408, 'W', 72},
  {411, 'W', 73}, {412, 'W', 60}, {417, 'W', 20}, {418, 'W', 21}, {507, 'E', 74}, {516, 'E', 74},
  {517, 'E', 74}, {603, 'E', 75}, {607, 'E', 76}, {621, 'E', 8}, {624, 'E', 59}, {632, 'E', 77},
};
static const Entry CP_DIESEL[] = {
  {110, 'E', 78}, {111, 'E', 8}, {121, 'E', 79}, {122, 'E', 80}, {131, 'E', 81}, {132, 'E', 82},
  {133, 'E', 83}, {150, 'W', 84}, {151, 'W', 85}, {152, 'E', 86}, {160, 'W', 21}, {161, 'W', 21},
  {162, 'E', 60}, {163, 'W', 87}, {164, 'W', 22}, {170, 'W', 88}, {255, 'W', 89}, {301, 'W', 20},
  {302, 'W', 21}, {303, 'W', 90}, {411, 'W', 90}, {417, 'W', 20}, {418, 'W', 21}, {423, 'W', 91},
  {424, 'W', 92}, {540, 'E', 20}, {541, 'E', 21}, {560, 'E', 93}, {561, 'E', 94}, {565, 'E', 95},
};
static const Entry CP_VARIO[] = {
  {2, 'E', 96}, {16, 'E', 96}, {25, 'W', 61}, {26, 'W', 97}, {27, 'W', 57}, {28, 'W', 60}, {29, 'W', 73},
  {255, 'W', 70},
};

// Truma Combi diagnostics catalogue (gas Combi), sorted by code
static const Entry CATALOGUE[] = {
  {5, 0, 0}, {6, 0, 1}, {8, 0, 2}, {10, 0, 3}, {11, 0, 2}, {15, 0, 2}, {16, 0, 2}, {17, 0, 4}, {18, 0, 5},
  {19, 0, 6}, {20, 0, 7}, {21, 0, 8}, {22, 0, 9}, {23, 0, 2}, {24, 0, 10}, {25, 0, 11}, {26, 0, 12},
  {28, 0, 13}, {29, 0, 14}, {30, 0, 15}, {31, 0, 16}, {32, 0, 2}, {33, 0, 15}, {34, 0, 16}, {35, 0, 2},
  {36, 0, 17}, {37, 0, 2}, {38, 0, 2}, {39, 0, 2}, {40, 0, 2}, {41, 0, 18}, {42, 0, 19}, {43, 0, 20},
  {44, 0, 21}, {45, 0, 22}, {46, 0, 23}, {47, 0, 2}, {48, 0, 24}, {49, 0, 2}, {50, 0, 2}, {51, 0, 2},
  {52, 0, 2}, {53, 0, 2}, {54, 0, 2}, {55, 0, 2}, {56, 0, 2}, {57, 0, 2}, {58, 0, 2}, {59, 0, 2},
  {60, 0, 25}, {61, 0, 26}, {62, 0, 2}, {63, 0, 2}, {64, 0, 2}, {65, 0, 2}, {66, 0, 2}, {67, 0, 2},
  {68, 0, 26}, {69, 0, 27}, {70, 0, 2}, {71, 0, 2}, {72, 0, 2}, {73, 0, 2}, {74, 0, 2}, {75, 0, 2},
  {76, 0, 2}, {77, 0, 2}, {78, 0, 2}, {79, 0, 2}, {80, 0, 28}, {81, 0, 26}, {82, 0, 2}, {83, 0, 2},
  {84, 0, 2}, {85, 0, 2}, {86, 0, 2}, {87, 0, 2}, {88, 0, 26}, {89, 0, 29}, {90, 0, 2}, {91, 0, 2},
  {92, 0, 2}, {93, 0, 2}, {94, 0, 30}, {96, 0, 2}, {97, 0, 2}, {98, 0, 2}, {99, 0, 2}, {100, 0, 31},
  {101, 0, 32}, {102, 0, 2}, {103, 0, 2}, {104, 0, 2}, {105, 0, 2}, {106, 0, 2}, {107, 0, 2}, {108, 0, 2},
  {109, 0, 2}, {110, 0, 2}, {111, 0, 2}, {112, 0, 33}, {113, 0, 34}, {116, 0, 35}, {117, 0, 35},
  {118, 0, 36}, {119, 0, 37}, {120, 0, 2}, {121, 0, 38}, {122, 0, 39}, {123, 0, 2}, {124, 0, 2}, {125, 0, 2},
  {126, 0, 2}, {127, 0, 40}, {128, 0, 2}, {129, 0, 2}, {130, 0, 2}, {131, 0, 2}, {133, 0, 2}, {134, 0, 2},
  {135, 0, 2}, {136, 0, 2}, {137, 0, 2}, {138, 0, 2}, {139, 0, 2}, {140, 0, 2}, {141, 0, 2}, {142, 0, 2},
  {143, 0, 2}, {144, 0, 2}, {145, 0, 2}, {146, 0, 2}, {147, 0, 2}, {148, 0, 18}, {149, 0, 24}, {150, 0, 24},
  {160, 0, 2}, {161, 0, 2}, {162, 0, 2}, {163, 0, 2}, {164, 0, 2}, {165, 0, 2}, {166, 0, 2}, {167, 0, 2},
  {168, 0, 2}, {169, 0, 2}, {170, 0, 28}, {171, 0, 26}, {172, 0, 2}, {173, 0, 2}, {174, 0, 2}, {175, 0, 2},
  {176, 0, 2}, {177, 0, 2}, {178, 0, 26}, {180, 0, 2}, {181, 0, 2}, {182, 0, 2}, {183, 0, 2}, {184, 0, 30},
  {186, 0, 2}, {187, 0, 2}, {188, 0, 2}, {189, 0, 2}, {190, 0, 2}, {191, 0, 2}, {192, 0, 2}, {193, 0, 2},
  {194, 0, 2}, {195, 0, 2}, {196, 0, 2}, {197, 0, 2}, {198, 0, 2}, {199, 0, 2}, {200, 0, 2}, {201, 0, 2},
  {202, 0, 33}, {203, 0, 34}, {206, 0, 35}, {207, 0, 35}, {208, 0, 36}, {209, 0, 37}, {210, 0, 2},
  {211, 0, 38}, {212, 0, 39}, {213, 0, 2}, {214, 0, 2}, {215, 0, 2}, {216, 0, 2}, {217, 0, 40}, {218, 0, 2},
  {219, 0, 2}, {220, 0, 2}, {221, 0, 2}, {223, 0, 2}, {224, 0, 2}, {225, 0, 2}, {226, 0, 2}, {227, 0, 2},
  {228, 0, 2}, {229, 0, 2}, {230, 0, 2}, {231, 0, 2}, {232, 0, 2}, {233, 0, 2}, {234, 0, 2}, {235, 0, 2},
  {236, 0, 2}, {237, 0, 2}, {238, 0, 18}, {239, 0, 24}, {240, 0, 24}, {255, 0, 41}, {301, 0, 20},
  {302, 0, 21}, {303, 0, 10}, {305, 0, 2}, {401, 0, 4}, {402, 0, 5}, {407, 0, 22}, {408, 0, 42},
  {411, 0, 10}, {412, 0, 19}, {413, 0, 2}, {414, 0, 2}, {417, 0, 20}, {418, 0, 21}, {420, 0, 43},
  {421, 0, 2}, {501, 0, 2}, {502, 0, 30}, {503, 0, 30}, {504, 0, 3}, {505, 0, 2}, {506, 0, 44}, {507, 0, 39},
  {508, 0, 34}, {509, 0, 17}, {510, 0, 45}, {511, 0, 2}, {512, 0, 46}, {513, 0, 47}, {514, 0, 2},
  {515, 0, 44}, {516, 0, 39}, {517, 0, 39}, {520, 0, 2}, {521, 0, 2}, {522, 0, 44}, {523, 0, 17},
  {524, 0, 48}, {525, 0, 1}, {526, 0, 0}, {527, 0, 49}, {528, 0, 15}, {529, 0, 16}, {530, 0, 50},
  {531, 0, 2}, {532, 0, 15}, {533, 0, 16}, {534, 0, 50}, {535, 0, 2}, {536, 0, 2}, {537, 0, 2}, {538, 0, 2},
  {539, 0, 2}, {601, 0, 2}, {602, 0, 2}, {603, 0, 18}, {604, 0, 2}, {605, 0, 24}, {606, 0, 2}, {607, 0, 51},
  {608, 0, 52}, {609, 0, 2}, {610, 0, 2}, {611, 0, 2}, {612, 0, 2}, {613, 0, 2}, {614, 0, 2}, {617, 0, 53},
  {619, 0, 7}, {620, 0, 9}, {621, 0, 8}, {622, 0, 13}, {623, 0, 54}, {624, 0, 14}, {625, 0, 2}, {626, 0, 2},
  {627, 0, 12}, {628, 0, 11}, {629, 0, 2}, {630, 0, 2}, {631, 0, 2}, {632, 0, 55}, {633, 0, 2}, {634, 0, 2},
  {635, 0, 2}, {636, 0, 2}, {700, 0, 2}, {701, 0, 2}, {702, 0, 2}, {703, 0, 2}, {704, 0, 2}, {705, 0, 2},
  {706, 0, 2}, {707, 0, 2}, {708, 0, 2}, {709, 0, 2}, {710, 0, 2}, {711, 0, 2}, {712, 0, 2}, {713, 0, 2},
  {714, 0, 2}, {715, 0, 2}, {716, 0, 2}, {717, 0, 2}, {718, 0, 2}, {719, 0, 2}, {720, 0, 2}, {721, 0, 2},
  {722, 0, 2}, {723, 0, 2}, {724, 0, 2}, {725, 0, 2}, {726, 0, 2}, {727, 0, 2}, {728, 0, 2}, {729, 0, 2},
  {730, 0, 2}, {731, 0, 2}, {732, 0, 2}, {733, 0, 2}, {734, 0, 2}, {735, 0, 2}, {736, 0, 2}, {737, 0, 2},
  {738, 0, 2}, {739, 0, 2}, {740, 0, 2}, {741, 0, 2}, {742, 0, 2}, {743, 0, 2}, {744, 0, 2}, {745, 0, 2},
  {746, 0, 2}, {747, 0, 2}, {748, 0, 2}, {749, 0, 2}, {750, 0, 2}, {751, 0, 2}, {752, 0, 2}, {753, 0, 2},
  {754, 0, 2}, {755, 0, 2}, {756, 0, 2}, {757, 0, 2}, {758, 0, 2}, {800, 0, 2}, {801, 0, 2}, {802, 0, 2},
  {803, 0, 2}, {804, 0, 2}, {805, 0, 2}, {806, 0, 2}, {807, 0, 2}, {808, 0, 2}, {809, 0, 2}, {810, 0, 2},
  {811, 0, 2}, {812, 0, 2}, {813, 0, 2}, {814, 0, 2}, {815, 0, 2}, {816, 0, 2}, {817, 0, 2}, {818, 0, 2},
  {819, 0, 2}, {820, 0, 2}, {821, 0, 2}, {822, 0, 2}, {823, 0, 2}, {824, 0, 2}, {825, 0, 2}, {826, 0, 2},
  {827, 0, 2}, {828, 0, 2}, {829, 0, 2}, {830, 0, 2}, {831, 0, 2}, {832, 0, 2}, {833, 0, 2}, {834, 0, 2},
  {835, 0, 2}, {836, 0, 2}, {837, 0, 2}, {838, 0, 2}, {839, 0, 2}, {840, 0, 2}, {841, 0, 2}, {842, 0, 2},
  {843, 0, 2}, {844, 0, 2}, {845, 0, 2}, {846, 0, 2}, {847, 0, 2}, {848, 0, 2}, {849, 0, 2}, {850, 0, 2},
  {851, 0, 2}, {852, 0, 2}, {853, 0, 2}, {854, 0, 2}, {855, 0, 2}, {856, 0, 2}, {857, 0, 2}, {858, 0, 2},
};

inline const Entry* find_(const Entry* tab, size_t n, int code) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (tab[mid].code == code) return &tab[mid];
    if (tab[mid].code < code) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}

struct Info { const char* text; char kind; };   // text nullptr = unknown code
#define TRUMA_ERR_N(t) (sizeof(t) / sizeof(t[0]))

// Text and kind of a fault code for the heater family. {nullptr, 0} for 0 and unknown codes.
inline Info lookup(int code, Family f) {
  if (code <= 0) return {nullptr, 0};
  const Entry* e = f == DIESEL ? find_(CP_DIESEL, TRUMA_ERR_N(CP_DIESEL), code)
                 : f == VARIO  ? find_(CP_VARIO, TRUMA_ERR_N(CP_VARIO), code)
                               : find_(CP_GAS, TRUMA_ERR_N(CP_GAS), code);
  // the diagnostics catalogue is for the gas Combi only
  if (!e && f == GAS) e = find_(CATALOGUE, TRUMA_ERR_N(CATALOGUE), code);
  return e ? Info{TEXTS[e->text], e->kind} : Info{nullptr, 0};
}

}  // namespace truma_err
