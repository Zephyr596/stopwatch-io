// Frames of the custom round-head pet, packed as consecutive PNGs in assets/codex-pet.bin.
#pragma once
#include <cstdint>

namespace sw::codexPet {
struct Frame { uint32_t offset; uint16_t bytes; uint16_t holdMs; };
inline constexpr int width = 176;
inline constexpr int workoutWidth = 232;
extern const uint8_t data[] asm("_binary_assets_codex_pet_bin_start");

inline constexpr Frame idle[] = {
    {0, 7371, 780},
    {7371, 7334, 130},
    {0, 7371, 670},
};

inline constexpr Frame working[] = {
    {14705, 7281, 420},
    {21986, 7360, 145},
    {14705, 7281, 235},
};

inline constexpr Frame waving[] = {
    {29346, 7363, 145},
    {36709, 7596, 145},
    {29346, 7363, 145},
    {36709, 7596, 145},
    {29346, 7363, 160},
};

inline constexpr Frame smoking[] = {
    {44305, 6855, 130},
    {51160, 7242, 155},
    {58402, 6603, 380},
    {65005, 6968, 260},
    {71973, 7187, 285},
    {79160, 6899, 210},
};

inline constexpr Frame complete[] = {
    {86059, 6963, 120},
    {93022, 7338, 120},
    {100360, 7379, 155},
    {107739, 7337, 115},
    {86059, 6963, 290},
};

inline constexpr Frame interrupted[] = {
    {115076, 6949, 290},
    {122025, 6943, 135},
    {115076, 6949, 300},
};

inline constexpr Frame drinking[] = {
    {128968, 6276, 200},
    {135244, 6237, 220},
    {141481, 6090, 525},
    {147571, 6318, 235},
    {153889, 6350, 375},
    {160239, 6206, 220},
};

inline constexpr Frame rolling[] = {
    {29346, 7363, 130},
    {36709, 7596, 130},
    {29346, 7363, 130},
    {36709, 7596, 130},
    {29346, 7363, 130},
    {36709, 7596, 130},
};

inline constexpr Frame knocking[] = {
    {166445, 7356, 120},
    {173801, 7668, 140},
    {181469, 7496, 125},
    {188965, 7459, 130},
    {196424, 7204, 160},
};

inline constexpr Frame rapidKnocking[] = {
    {166445, 7356, 78},
    {173801, 7668, 102},
};

inline constexpr Frame benchPress[] = {
    {203628, 7051, 220},
    {210679, 7402, 160},
    {218081, 7475, 340},
    {210679, 7402, 155},
    {203628, 7051, 210},
    {210679, 7402, 155},
    {218081, 7475, 340},
    {210679, 7402, 155},
    {203628, 7051, 340},
};

inline constexpr Frame latPulldown[] = {
    {225556, 6921, 220},
    {232477, 6996, 160},
    {239473, 6541, 340},
    {232477, 6996, 155},
    {225556, 6921, 210},
    {232477, 6996, 155},
    {239473, 6541, 340},
    {232477, 6996, 155},
    {225556, 6921, 340},
};

inline constexpr Frame gobletSquat[] = {
    {246014, 5393, 220},
    {251407, 5441, 160},
    {256848, 5306, 340},
    {251407, 5441, 155},
    {246014, 5393, 210},
    {251407, 5441, 155},
    {256848, 5306, 340},
    {251407, 5441, 155},
    {246014, 5393, 340},
};

} // namespace sw::codexPet
