#include "align.h"
#include <string>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <array>

using std::string;
using std::cout;
using std::endl;

#define RED 0
#define GREEN 1
#define BLUE 2

Image mirror(Image srcImage, int radius)
{
    Image resImage(srcImage.n_rows + 2 * radius, srcImage.n_cols + 2 * radius);

    // копируем общую часть srcImage и resImage
    for (int i = 0; i < int(srcImage.n_rows); i++) {
        for (int j = 0; j < int(srcImage.n_cols); j++) {
            resImage(i + radius, j + radius) = srcImage(i, j);
        }
    }

    for (int i = 0; i < radius; i++) {
        for(int j = radius; j < int(resImage.n_cols) - radius; j++) {
            // зеркалируем верхнюю и нижнюю части
            resImage(i, j) = resImage(2 * radius - i, j);
            resImage((resImage.n_rows - 1) - i, j) = resImage((resImage.n_rows - 1) - 2 * radius + i, j);
        }
    }

    // зеркалируем боковые стороны
    for (int i = radius; i < int(resImage.n_rows) - radius; i++) {
        for (int j = 0; j < radius; j++) {
            resImage(i, j) = resImage(i, 2 * radius - j);
            resImage(i, (resImage.n_cols - 1) - j) = resImage(i, (resImage.n_cols - 1) - 2 * radius + j);
        }
    }

    // зеркалируем углы
    for (int i = 0; i < radius; i++) {
        for (int j = 0; j < radius; j++) {
            // левый верхний
            resImage(i, j) = resImage(2 * radius - j, 2 * radius - i);

            // правый верхний
            resImage(i, (resImage.n_cols - 1) - j) = resImage(2 * radius - j, (resImage.n_cols - 1) - 2 * radius + i);

            // левый нижний
            resImage((resImage.n_rows - 1) - i, j) = resImage((resImage.n_rows - 1) - 2 * radius + j, 2 * radius - i);

            // правый нижний
            resImage((resImage.n_rows - 1) - i, (resImage.n_cols - 1) - j) = resImage((resImage.n_rows - 1) - 2 * radius + j,
                                                                                      (resImage.n_cols - 1) - 2 * radius + i);
        }
    }

    return resImage;
}

Image align(Image srcImage, bool isPostprocessing, std::string postprocessingType, double fraction, bool isMirror,
            bool isInterp, bool isSubpixel, double subScale)
{
    // srcImage уже загружено
    uint width = srcImage.n_cols, height = srcImage.n_rows / 3;

    // это максимальный сдвиг; возьмем его как 5% от высоты и 5% от ширины
    int shift_h = height * 5 / 100, shift_w = width * 5 / 100;

    // делим srcImage на три различных изображения по каналам
    // сразу отступаем на 10% от границ для улучшения метрики
    uint ind_h = height * 10 / 100, ind_w = width * 10 / 100; // отступы по высоте и ширине
    uint height_wi = height - 2 * ind_h, width_wi = width - 2 * ind_w; // высота и ширина с отступами
    Image blueImage = srcImage.submatrix(ind_h, ind_w, height_wi, width_wi), // конструктор копирования
          greenImage = srcImage.submatrix(height + ind_h, ind_w, height_wi, width_wi),
          redImage = srcImage.submatrix(2 * height + ind_h, ind_w, height_wi, width_wi);

    // среднеквадратичное отклонение

    // сначала берем минимум по green и red
    double min_mse = DBL_MAX; // минимум по среднеквадратичному отклонению
    int shift_imin_rg = 0, shift_jmin_rg = 0; // соответствующие сдвиги

    for (int shift_i = -shift_h; shift_i <= shift_h; shift_i++) { // эти два цикла - сдвиг одного изображения относительно другого
        for (int shift_j = -shift_w; shift_j <= shift_w; shift_j++) {
            unsigned long long sum_pix = 0; // сумма по всем перекрывающимся пикселям

            // тут считаем сумму для перекрывающейся области
            for (uint i = std::max(0, shift_i); i < height_wi + std::min(0, shift_i); i++) {
                for (uint j = std::max(0, shift_j); j < width_wi + std::min(0, shift_j); j++) { // i и j - для blueImage
                    long long int tmp = static_cast<int>(std::get<GREEN>(greenImage(i, j))) -
                                        static_cast<int>(std::get<RED>(redImage(i - shift_i, j - shift_j)));
                    sum_pix += static_cast<unsigned long long>(tmp * tmp);
                }
            }

            double mse; // среднеквадратичное отклонение
            mse = sum_pix / static_cast<double>((height_wi - std::abs(shift_i)) * (width_wi - std::abs(shift_j)));

            if (mse < min_mse) { // новый минимум и его сдвиги
                min_mse = mse;
                shift_imin_rg = shift_i;
                shift_jmin_rg = shift_j;
            }
        }
    }

    // теперь берем минимум по green и blue
    min_mse = DBL_MAX; // минимум по среднеквадратичному отклонению
    int shift_imin_bg = 0, shift_jmin_bg = 0; // соответствующие сдвиги

    for (int shift_i = -shift_h; shift_i <= shift_h; shift_i++) { // эти два цикла - сдвиг одного изображения относительно другого
        for (int shift_j = -shift_w; shift_j <= shift_w; shift_j++) {
            unsigned long long sum_pix = 0; // сумма по всем перекрывающимся пикселям

            // тут считаем сумму для перекрывающейся области
            for (uint i = std::max(0, shift_i); i < height_wi + std::min(0, shift_i); i++) {
                for (uint j = std::max(0, shift_j); j < width_wi + std::min(0, shift_j); j++) { // i и j - для blueImage
                    long long int tmp = static_cast<int>(std::get<GREEN>(greenImage(i, j))) -
                                        static_cast<int>(std::get<BLUE>(blueImage(i - shift_i, j - shift_j)));
                    sum_pix += static_cast<unsigned long long>(tmp * tmp);
                }
            }

            double mse; // среднеквадратичное отклонение
            mse = sum_pix / static_cast<double>((height_wi - std::abs(shift_i)) * (width_wi - std::abs(shift_j)));

            if (mse < min_mse) { // новый минимум и его сдвиги
                min_mse = mse;
                shift_imin_bg = shift_i;
                shift_jmin_bg = shift_j;
            }
        }
    }

    // теперь лепим все воедино

    // возвращаем отдельным цветам края
    blueImage = srcImage.submatrix(0, 0, height, width);
    greenImage = srcImage.submatrix(height, 0, height, width);
    redImage = srcImage.submatrix(2 * height, 0, height, width);

    // изображение-результат; остальные изображеня двигаем относительно него
    Image resImage(height + std::min(std::min(0, shift_imin_rg), shift_imin_bg) - std::max(std::max(0, shift_imin_rg), shift_imin_bg),
                   width + std::min(std::min(0, shift_jmin_rg), shift_jmin_bg) - std::max(std::max(0, shift_jmin_rg), shift_jmin_bg));

    uint shift_bi = std::max(std::max(0, shift_imin_rg), shift_imin_bg), // сдвиг для зеленого цвета
         shift_bj = std::max(std::max(0, shift_jmin_rg), shift_jmin_bg);
    // для других цветов смотрим сдвиги относительно зеленого

    for (uint i = 0; i < resImage.n_rows; i++) {
        for (uint j = 0; j < resImage.n_cols; j++) {
            resImage(i, j) = std::make_tuple(std::get<RED>(redImage(i + shift_bi - std::min(0, shift_imin_rg) - std::max(0, shift_imin_rg),
                                                                    j + shift_bj - std::min(0, shift_jmin_rg) - std::max(0, shift_jmin_rg))),
                                             std::get<GREEN>(greenImage(i + shift_bi, j + shift_bj)),
                                             std::get<BLUE>(blueImage(i + shift_bi - std::min(0, shift_imin_bg) - std::max(0, shift_imin_bg),
                                                                        j + shift_bj - std::min(0, shift_jmin_bg) - std::max(0, shift_jmin_bg))));
        }
    }

    if (isPostprocessing) {

        if (postprocessingType == "--gray-world") {
            resImage = gray_world(resImage);
        }
        if (postprocessingType == "--unsharp") {
            if (isMirror) {
                resImage = mirror(resImage, 1);
            }

            resImage = unsharp(resImage);

            if (isMirror) {
                resImage = resImage.submatrix(1, 1, resImage.n_rows - 2, resImage.n_cols - 2);
            }
        }
        if (postprocessingType == "--autocontrast") {
            resImage = autocontrast(resImage, fraction);
        }
    }

    return resImage;
}

Image sobel_x(Image src_image) {
    Matrix<double> kernel = {{-1, 0, 1},
                             {-2, 0, 2},
                             {-1, 0, 1}};
    return custom(src_image, kernel);
}

Image sobel_y(Image src_image) {
    Matrix<double> kernel = {{ 1,  2,  1},
                             { 0,  0,  0},
                             {-1, -2, -1}};
    return custom(src_image, kernel);
}

Image gray_world(Image srcImage) {
    double ave_red = 0, ave_green = 0, ave_blue = 0;

    // сложим яркости по каналам
    for (uint i = 0; i < srcImage.n_rows; i++) {
        for (uint j = 0; j < srcImage.n_cols; j++) {
            ave_red += std::get<RED>(srcImage(i, j));
            ave_green += std::get<GREEN>(srcImage(i, j));
            ave_blue += std::get<BLUE>(srcImage(i,j));
        }
    }

    // нормируем по площади - средние яркости
    uint square = srcImage.n_rows * srcImage.n_cols;
    ave_red /= square;
    ave_green /= square;
    ave_blue /= square;

    double ave = (ave_red + ave_green + ave_blue) / 3; // среднее средних

    // множители для пикселей
    ave_red = ave / ave_red;
    ave_green = ave / ave_green;
    ave_blue = ave / ave_blue;

    // собственно, фильтр
    for (uint i = 0; i < srcImage.n_rows; i++) {
        for (uint j = 0; j < srcImage.n_cols; j++) {
            uint new_red = static_cast<uint>(static_cast<double>(std::get<RED>(srcImage(i, j))) * ave_red),
                 new_green = static_cast<uint>(static_cast<double>(std::get<GREEN>(srcImage(i, j))) * ave_green),
                 new_blue = static_cast<uint>(static_cast<double>(std::get<BLUE>(srcImage(i,j))) * ave_blue);
            if (new_red > 255) { new_red = 255; }
            if (new_green > 255) { new_green = 255; }
            if (new_blue > 255) { new_blue = 255; }

            srcImage(i, j) = std::make_tuple(new_red, new_green, new_blue);
        }
    }

    return srcImage;
}

Image resize(Image src_image, double scale) {
    return src_image;
}

class ForCustom
{
public:
    std::tuple<uint, uint, uint> operator () (const Image &m) const
    {
        uint size = 2 * radius + 1;
        uint red, green, blue;
        double sum_red = 0, sum_green = 0, sum_blue = 0;
        for (uint i = 0; i < size; i++) {
            for (uint j = 0; j < size; j++) {
                std::tie(red, green, blue) = m(i, j);
                sum_red += static_cast<double>(red) * weight(i, j);
                sum_green += static_cast<double>(green) * weight(i, j);
                sum_blue += static_cast<double>(blue) * weight(i, j);
            }
        }
        if (sum_red < 0) { sum_red = 0; }
        if (sum_red > 255) { sum_red = 255; }

        if (sum_green < 0) { sum_green = 0; }
        if (sum_green > 255) { sum_green = 255; }

        if (sum_blue < 0) { sum_blue = 0; }
        if (sum_blue > 255) { sum_blue = 255; }

        return std::make_tuple(static_cast<uint>(sum_red), static_cast<uint>(sum_green), static_cast<uint>(sum_blue));
    }

    static uint radius;
    static Matrix<double> weight;
};

uint ForCustom::radius;
Matrix<double> ForCustom::weight;

Image unsharp(Image srcImage) {
    Matrix<double> kernel = {{-1 / 6.0, -2 / 3.0, -1 / 6.0},
                             { -2 / 3.0, 13 / 3.0, -2 / 3.0},
                             {-1 / 6.0, -2 / 3.0, -1 / 6.0}};

    ForCustom::radius = 1;
    srcImage = custom(srcImage, kernel);

    return srcImage;
}

Image custom(Image srcImage, Matrix<double> kernel) {
    // Function custom is useful for making concrete linear filtrations
    // like gaussian or sobel. So, we assume that you implement customapply
    // and then implement other filtrations using this function.
    // sobel_x and sobel_y are given as an example.

    ForCustom::weight = kernel;

    srcImage = srcImage.unary_map(ForCustom());

    return srcImage;
}

Image autocontrast(Image srcImage, double fraction) {

    uint histo[256]; // гистограмма
    std::memset(histo, 0, sizeof(histo)); // обнуляем массив гистограммы

    // считаем яркость каждого пикселя по формуле и добавляем их в гистограмму
    for (uint i = 0; i < srcImage.n_rows; i++) {
        for (uint j = 0 ; j < srcImage.n_cols; j++) {
            histo[static_cast<int>(std::lround(0.2125 * std::get<RED>(srcImage(i, j)) +
                                               0.7154 * std::get<GREEN>(srcImage(i, j)) +
                                               0.0721 * std::get<BLUE>(srcImage(i, j))))]++;
        }
    }

    // обрезаем гистограмму с обоих концов на fraction
    uint for_white = 0, for_black = 0;
    int ymin = 0, ymax = 255; // с какого значения начинается нужная часть гистограммы
    uint max_pix = std::lround(fraction * srcImage.n_cols * srcImage.n_rows); // сколько пикселей с обеих сторон гистограммы отбрасываем
    while (for_white <= max_pix) {
        for_white += histo[ymin];
        ymin++;
    }
    while (for_black <= max_pix) {
        for_black += histo[ymax];
        ymax--;
    }
    // нужная часть гистограммы: ymin..ymax (включительно)

    // ymin - минимальное значение яркости, ymax - максимальное
    // меньше минимального - белое, больше максимального - черное
    // для остального считаем коэффициенты линейного преобразования f^(-1)(y) = lin_a * y + lin_b
    double lin_a = 255 / static_cast<double>(ymax - ymin), lin_b = -ymin * 255 / static_cast<double>(ymax - ymin);
    cout << ymin << ' ' << ymax  << ' ' << lin_a << " " << lin_b << endl;

    // применяем линейное пребразование ко всем пикселям; считаем отдельно по каналам
    for (uint i = 0; i < srcImage.n_rows; i++) {
        for (uint j = 0; j < srcImage.n_cols; j++) {
            int red = std::lround(lin_a * std::get<RED>(srcImage(i, j)) + lin_b),
                green = std::lround(lin_a * std::get<GREEN>(srcImage(i, j)) + lin_b),
                blue = std::lround(lin_a * std::get<BLUE>(srcImage(i, j)) + lin_b);

            if (red < 0) { red = 0; }
            if (red > 255) { red = 255; }

            if (green < 0) { green = 0; }
            if (green > 255) { green = 255; }

            if (blue < 0) { blue = 0; }
            if (blue > 255) { blue = 255; }

            srcImage(i, j) = std::make_tuple(red, green, blue);
        }
    }

    return srcImage;
}

Image gaussian(Image src_image, double sigma, int radius)  {
    return src_image;
}

Image gaussian_separable(Image src_image, double sigma, int radius) {
    return src_image;
}

Image median(Image srcImage, int radius) {

    srcImage = mirror(srcImage, radius);

    Image resImage = srcImage.deep_copy();

    for (uint i = 0 + radius; i < srcImage.n_rows - radius; i++) {
        for (uint j = 0 + radius; j < srcImage.n_cols - radius; j++) {
            std::vector<uint> nhs_red, nhs_green, nhs_blue; // вектор соседей в пределах заданного радиуса для каждого канала

            nhs_red.reserve((2 * radius + 1) * (2 * radius + 1)); // задаем минимальный размер хранилища
            nhs_green.reserve((2 * radius + 1) * (2 * radius + 1));
            nhs_blue.reserve((2 * radius + 1) * (2 * radius + 1));

            // заносим элементы окрестности пикселя в вектор для каждого канала
            for (uint nhs_i = i - radius; nhs_i <= i + radius; nhs_i++) {
                for (uint nhs_j = j - radius; nhs_j <= j + radius; nhs_j++) {
                    nhs_red.push_back(std::get<RED>(srcImage(nhs_i, nhs_j)));
                    nhs_green.push_back(std::get<GREEN>(srcImage(nhs_i, nhs_j)));
                    nhs_blue.push_back(std::get<BLUE>(srcImage(nhs_i, nhs_j)));
                }
            }

            // сортировка
            std::sort(nhs_red.begin(), nhs_red.end());
            std::sort(nhs_green.begin(), nhs_green.end());
            std::sort(nhs_blue.begin(), nhs_blue.end());

            // выбор медианы
            resImage(i, j) = std::make_tuple(nhs_red[nhs_red.size() / 2],
                                             nhs_green[nhs_green.size() / 2],
                                             nhs_blue[nhs_blue.size() / 2]);
        }
    }

    resImage = resImage.submatrix(radius, radius, resImage.n_rows - 2 * radius, resImage.n_cols - 2 * radius);

    return resImage;
}

// змейка
Image median_linear(Image srcImage, int radius) {

    srcImage = mirror(srcImage, radius);
    // линейная медиана
    Image resImage = srcImage.deep_copy();

    uint histo_red[256], histo_green[256], histo_blue[256]; // гистограммы
    std::memset(histo_red, 0, sizeof(histo_red)); // обнуляем массив гистограмм
    std::memset(histo_green, 0, sizeof(histo_green));
    std::memset(histo_blue, 0, sizeof(histo_blue));

    int med = (2 * radius + 1); // индекс медианы в массиве
    med *= med;
    med /= 2;

    for (int i = radius; i < static_cast<int>(srcImage.n_rows) - radius; i++) {
        // хотим ходить змейкой; по /четным/ строкам идём вправо по j

        if ((i - radius) % 2 == 0) {
            // заходим сюда при переходе на новую строку
            // срезаем сверху - добавляем снизу, кроме начального случая: i = radius
            int j = radius;

            if (i != radius) {
                for (int k = -radius; k <= radius; k++) {
                    // срезаем сверху
                    histo_red[std::get<RED>(srcImage(i - radius - 1, j + k))]--;
                    histo_green[std::get<GREEN>(srcImage(i - radius - 1, j + k))]--;
                    histo_blue[std::get<BLUE>(srcImage(i - radius - 1, j + k))]--;

                    // добавляем снизу
                    histo_red[std::get<RED>(srcImage(i + radius, j + k))]++;
                    histo_green[std::get<GREEN>(srcImage(i + radius, j + k))]++;
                    histo_blue[std::get<BLUE>(srcImage(i + radius, j + k))]++;
                }

            } else { // случай i == radius; левый верхний угол
                // заполняем гистограммы
                for (int hi = -radius; hi <= radius; hi++) {
                    for (int hj = -radius; hj <= radius; hj++) {
                        histo_red[std::get<RED>(srcImage(i + hi, j + hj))]++;
                        histo_green[std::get<GREEN>(srcImage(i + hi, j + hj))]++;
                        histo_blue[std::get<BLUE>(srcImage(i + hi, j + hj))]++;
                    }
                }
            }

            // ищем медиану
            int find_rmed = 0, ri = 0;
            while (find_rmed <= med) {
                find_rmed += histo_red[ri];
                ri++;
            }
            ri--;

            int find_gmed = 0, gi = 0;
            while (find_gmed <= med) {
                find_gmed += histo_green[gi];
                gi++;
            }
            gi--;

            int find_bmed = 0, bi = 0;
            while (find_bmed <= med) {
                find_bmed += histo_blue[bi];
                bi++;
            }
            bi--;

            resImage(i, j) = std::make_tuple(ri, gi, bi);

            // для остальных j просто идём вправо
            for (j = radius + 1; j < static_cast<int>(srcImage.n_cols) - radius; j++) {
                for (int k = -radius; k <= radius; k++) {
                    // срезаем слева
                    histo_red[std::get<RED>(srcImage(i + k, j - radius - 1))]--;
                    histo_green[std::get<GREEN>(srcImage(i + k, j - radius - 1))]--;
                    histo_blue[std::get<BLUE>(srcImage(i + k, j - radius - 1))]--;

                    // добавляем справа
                    histo_red[std::get<RED>(srcImage(i + k, j + radius))]++;
                    histo_green[std::get<GREEN>(srcImage(i + k, j + radius))]++;
                    histo_blue[std::get<BLUE>(srcImage(i + k, j + radius))]++;
                }

                // ищем медиану
                find_rmed = 0, ri = 0;
                while (find_rmed <= med) {
                    find_rmed += histo_red[ri];
                    ri++;
                }
                ri--;

                find_gmed = 0, gi = 0;
                while (find_gmed <= med) {
                    find_gmed += histo_green[gi];
                    gi++;
                }
                gi--;

                find_bmed = 0, bi = 0;
                while (find_bmed <= med) {
                    find_bmed += histo_blue[bi];
                    bi++;
                }
                bi--;

                resImage(i, j) = std::make_tuple(ri, gi, bi);
            }
        } else { // по /нечётным/ строкам идём влево по j
            // заходим сюда при переходе на новую строку
            // срезаем сверху - добавляем снизу;
            int j = static_cast<int>(srcImage.n_cols) - radius - 1;

            for (int k = -radius; k <= radius; k++) {
                // срезаем сверху
                histo_red[std::get<RED>(srcImage(i - radius - 1, j + k))]--;
                histo_green[std::get<GREEN>(srcImage(i - radius - 1, j + k))]--;
                histo_blue[std::get<BLUE>(srcImage(i - radius - 1, j + k))]--;

                // добавляем снизу
                histo_red[std::get<RED>(srcImage(i + radius, j + k))]++;
                histo_green[std::get<GREEN>(srcImage(i + radius, j + k))]++;
                histo_blue[std::get<BLUE>(srcImage(i + radius, j + k))]++;
            }

            // ищем медиану
            int find_rmed = 0, ri = 0;
            while (find_rmed <= med) {
                find_rmed += histo_red[ri];
                ri++;
            }
            ri--;

            int find_gmed = 0, gi = 0;
            while (find_gmed <= med) {
                find_gmed += histo_green[gi];
                gi++;
            }
            gi--;

            int find_bmed = 0, bi = 0;
            while (find_bmed <= med) {
                find_bmed += histo_blue[bi];
                bi++;
            }
            bi--;

            resImage(i, j) = std::make_tuple(ri, gi, bi);

            // для остальных j просто идём влево
            for (j = static_cast<int>(srcImage.n_cols) - radius - 2; j >= radius; j--) {
                for (int k = -radius; k <= radius; k++) {
                    // срезаем справа
                    histo_red[std::get<RED>(srcImage(i + k, j + radius + 1))]--;
                    histo_green[std::get<GREEN>(srcImage(i + k, j + radius + 1))]--;
                    histo_blue[std::get<BLUE>(srcImage(i + k, j + radius + 1))]--;

                    // добавляем слева
                    histo_red[std::get<RED>(srcImage(i + k, j - radius))]++;
                    histo_green[std::get<GREEN>(srcImage(i + k, j - radius))]++;
                    histo_blue[std::get<BLUE>(srcImage(i + k, j - radius))]++;
                }

                // ищем медиану
                find_rmed = 0, ri = 0;
                while (find_rmed <= med) {
                    find_rmed += histo_red[ri];
                    ri++;
                }
                ri--;

                find_gmed = 0, gi = 0;
                while (find_gmed <= med) {
                    find_gmed += histo_green[gi];
                    gi++;
                }
                gi--;

                find_bmed = 0, bi = 0;
                while (find_bmed <= med) {
                    find_bmed += histo_blue[bi];
                    bi++;
                }
                bi--;

                resImage(i, j) = std::make_tuple(ri, gi, bi);
            }
        }
    }

    resImage = resImage.submatrix(radius, radius, resImage.n_rows - 2 * radius, resImage.n_cols - 2 * radius);

    return resImage;
}

uint get_median(std::array<uint, 256> histo, int med) {
    int find_med = 0, i = 0;

    while (find_med <= med) {
        find_med += histo[i];
        i++;
    }
    i--;

    return i;
}

Image median_const(Image srcImage, int radius) {
    srcImage = mirror(srcImage, radius);

    Image resImage = srcImage.deep_copy();

    int med = (2 * radius + 1); // индекс медианы в массиве
    med *= med;
    med /= 2;

    std::array<uint, 256> ker_histo_red, ker_histo_green, ker_histo_blue; // гистограмма ядра
    ker_histo_red.fill(0);
    ker_histo_green.fill(0);
    ker_histo_blue.fill(0);

    std::vector<std::array<uint, 256>> histo_cols_red, histo_cols_green, histo_cols_blue; // гистограммы столбцов
    histo_cols_red.insert(histo_cols_red.end(), srcImage.n_cols, ker_histo_red); // создаем все столбцы и заполняем нулями
    histo_cols_green.insert(histo_cols_green.end(), srcImage.n_cols, ker_histo_green);
    histo_cols_blue.insert(histo_cols_blue.end(), srcImage.n_cols, ker_histo_blue);

    // проиницализируем гистограммы для i = radius; недозаполним для универсальности
    int i = radius, j;
    for (int k = -radius; k < radius; k++) {
        for (j = 0; j < int(srcImage.n_cols); j++) {
            histo_cols_red[j][std::get<RED>(srcImage(i + k, j))]++;
            histo_cols_green[j][std::get<GREEN>(srcImage(i + k, j))]++;
            histo_cols_blue[j][std::get<BLUE>(srcImage(i + k, j))]++;
        }
    }

    // проинициализируем ядро
    j = radius;
    for (int hj = -radius; hj <= radius; hj++) {
        for (int h = 0; h < 256; h++) {
            ker_histo_red[h] += histo_cols_red[j + hj][h];
            ker_histo_green[h] += histo_cols_green[j + hj][h];
            ker_histo_blue[h] += histo_cols_blue[j + hj][h];
        }
    }

    // ходим змейкой
    for (i = radius; i < int(srcImage.n_rows) - radius; i++) {
        if ((i - radius) % 2 == 0) {
            // спускаемся вниз слева
            j = radius;
            for (int hj = -radius; hj <= radius; hj++) {
                // в гистограммах столцов удаляем значения сверху, добавляем значения снизу; и в ядре тоже!
                // удаляем сверху
                if (i != radius) {
                    histo_cols_red[j + hj][std::get<RED>(srcImage(i - radius - 1, j + hj))]--;
                    ker_histo_red[std::get<RED>(srcImage(i - radius - 1, j + hj))]--;

                    histo_cols_green[j + hj][std::get<GREEN>(srcImage(i - radius - 1, j + hj))]--;
                    ker_histo_green[std::get<GREEN>(srcImage(i - radius - 1, j + hj))]--;

                    histo_cols_blue[j + hj][std::get<BLUE>(srcImage(i - radius - 1, j + hj))]--;
                    ker_histo_blue[std::get<BLUE>(srcImage(i - radius - 1, j + hj))]--;
                }

                // добавляем снизу
                histo_cols_red[j + hj][std::get<RED>(srcImage(i + radius, j + hj))]++;
                ker_histo_red[std::get<RED>(srcImage(i + radius, j + hj))]++;

                histo_cols_green[j + hj][std::get<GREEN>(srcImage(i + radius, j + hj))]++;
                ker_histo_green[std::get<GREEN>(srcImage(i + radius, j + hj))]++;

                histo_cols_blue[j + hj][std::get<BLUE>(srcImage(i + radius, j + hj))]++;
                ker_histo_blue[std::get<BLUE>(srcImage(i + radius, j + hj))]++;
            }

            resImage(i, j) = std::make_tuple(get_median(ker_histo_red, med),
                                             get_median(ker_histo_green, med),
                                             get_median(ker_histo_blue, med));

            // двигаемся вправо
            for (j = radius + 1; j < int(srcImage.n_cols) - radius; j++) {
                // удаляем сверху
                if (i != radius) {
                    histo_cols_red[j + radius][std::get<RED>(srcImage(i - radius - 1, j + radius))]--;
                    histo_cols_green[j + radius][std::get<GREEN>(srcImage(i - radius - 1, j + radius))]--;
                    histo_cols_blue[j + radius][std::get<BLUE>(srcImage(i - radius - 1, j + radius))]--;
                }

                // добавляем снизу
                histo_cols_red[j + radius][std::get<RED>(srcImage(i + radius, j + radius))]++;
                histo_cols_green[j + radius][std::get<GREEN>(srcImage(i + radius, j + radius))]++;
                histo_cols_blue[j + radius][std::get<BLUE>(srcImage(i + radius, j + radius))]++;

                // обновляем гистограмму для ядра
                for (int h = 0; h < 256; h++) {
                    // удаляем столбец слева
                    ker_histo_red[h] -= histo_cols_red[j - radius - 1][h];
                    ker_histo_green[h] -= histo_cols_green[j - radius - 1][h];
                    ker_histo_blue[h] -= histo_cols_blue[j - radius - 1][h];

                    // прибавляем столбец справа
                    ker_histo_red[h] += histo_cols_red[j + radius][h];
                    ker_histo_green[h] += histo_cols_green[j + radius][h];
                    ker_histo_blue[h] += histo_cols_blue[j + radius][h];
                }

                resImage(i, j) = std::make_tuple(get_median(ker_histo_red, med),
                                                 get_median(ker_histo_green, med),
                                                 get_median(ker_histo_blue, med));
            }

        } else {
            // спускаемся вниз справа
            j = int(srcImage.n_cols) - radius - 1;
            for (int hj = -radius; hj <= radius; hj++) {
                // в гистограммах столцов удаляем значения сверху, добавляем значения снизу; и в ядре тоже!
                // удаляем сверху
                histo_cols_red[j + hj][std::get<RED>(srcImage(i - radius - 1, j + hj))]--;
                ker_histo_red[std::get<RED>(srcImage(i - radius - 1, j + hj))]--;

                histo_cols_green[j + hj][std::get<GREEN>(srcImage(i - radius - 1, j + hj))]--;
                ker_histo_green[std::get<GREEN>(srcImage(i - radius - 1, j + hj))]--;

                histo_cols_blue[j + hj][std::get<BLUE>(srcImage(i - radius - 1, j + hj))]--;
                ker_histo_blue[std::get<BLUE>(srcImage(i - radius - 1, j + hj))]--;

                // добавляем снизу
                histo_cols_red[j + hj][std::get<RED>(srcImage(i + radius, j + hj))]++;
                ker_histo_red[std::get<RED>(srcImage(i + radius, j + hj))]++;

                histo_cols_green[j + hj][std::get<GREEN>(srcImage(i + radius, j + hj))]++;
                ker_histo_green[std::get<GREEN>(srcImage(i + radius, j + hj))]++;

                histo_cols_blue[j + hj][std::get<BLUE>(srcImage(i + radius, j + hj))]++;
                ker_histo_blue[std::get<BLUE>(srcImage(i + radius, j + hj))]++;
            }

            resImage(i, j) = std::make_tuple(get_median(ker_histo_red, med),
                                             get_median(ker_histo_green, med),
                                             get_median(ker_histo_blue, med));

            // двигаемся влево
            for (j = int(srcImage.n_cols) - radius - 2; j >= radius; j--) {
                // в гистограммах столцов удаляем значения сверху, добавляем значения снизу
                // удаляем сверху
                histo_cols_red[j - radius][std::get<RED>(srcImage(i - radius - 1, j - radius))]--;
                histo_cols_green[j - radius][std::get<GREEN>(srcImage(i - radius - 1, j - radius))]--;
                histo_cols_blue[j - radius][std::get<BLUE>(srcImage(i - radius - 1, j - radius))]--;

                // добавляем снизу
                histo_cols_red[j - radius][std::get<RED>(srcImage(i + radius, j - radius))]++;
                histo_cols_green[j - radius][std::get<GREEN>(srcImage(i + radius, j - radius))]++;
                histo_cols_blue[j - radius][std::get<BLUE>(srcImage(i + radius, j - radius))]++;

                // обновляем гистограмму для ядра
                for (int h = 0; h < 256; h++) {
                    // удаляем столбец справа
                    ker_histo_red[h] -= histo_cols_red[j + radius + 1][h];
                    ker_histo_green[h] -= histo_cols_green[j + radius + 1][h];
                    ker_histo_blue[h] -= histo_cols_blue[j + radius + 1][h];

                    // прибавляем столбец слева
                    ker_histo_red[h] += histo_cols_red[j - radius][h];
                    ker_histo_green[h] += histo_cols_green[j - radius][h];
                    ker_histo_blue[h] += histo_cols_blue[j - radius][h];
                }

                resImage(i, j) = std::make_tuple(get_median(ker_histo_red, med),
                                                 get_median(ker_histo_green, med),
                                                 get_median(ker_histo_blue, med));
            }
        }
    }

    resImage = resImage.submatrix(radius, radius, resImage.n_rows - 2 * radius, resImage.n_cols - 2 * radius);

    return resImage;
}

Image canny(Image src_image, int threshold1, int threshold2) {
    return src_image;
}
