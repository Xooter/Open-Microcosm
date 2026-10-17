#pragma once

// Paleta categórica de Okabe e Ito (2008, "Color Universal Design (CUD): how to make figures and
// presentations that are friendly to colorblind people"): ocho colores que se distinguen con las
// tres formas de daltonismo (protanopía, deuteranopía y tritanopía). Todo lo que
// usa el color para distinguir categorías (el mapa de limitación, las series de los gráficos) sale de
// acá. Los mapas continuos (capas de campo) usan viridis, que también es apto.
struct Rgb8 {
    unsigned char r, g, b;
};

namespace okabe_ito {
constexpr Rgb8 kBlack{0, 0, 0};
constexpr Rgb8 kOrange{230, 159, 0};
constexpr Rgb8 kSkyBlue{86, 180, 233};
constexpr Rgb8 kBluishGreen{0, 158, 115};
constexpr Rgb8 kYellow{240, 228, 66};
constexpr Rgb8 kBlue{0, 114, 178};
constexpr Rgb8 kVermillion{213, 94, 0};
constexpr Rgb8 kReddishPurple{204, 121, 167};
// Para series sobre un fondo oscuro (los gráficos): sin el negro, que no se vería; el gris claro va
// último.
constexpr Rgb8 kOnDark[] = {kOrange, kSkyBlue, kBluishGreen, kYellow, kBlue, kVermillion, kReddishPurple, {187, 187, 187}};
} // namespace okabe_ito
