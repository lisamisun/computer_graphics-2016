#include <string>
#include <vector>
#include <fstream>
#include <cassert>
#include <iostream>
#include <cmath>

#include "classifier.h"
#include "EasyBMP.h"
#include "linear.h"
#include "argvparser.h"
#include "matrix.h"

using std::string;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::pair;
using std::make_pair;
using std::cout;
using std::cerr;
using std::endl;

using CommandLineProcessing::ArgvParser;

typedef vector<pair<BMP*, int> > TDataSet;
typedef vector<pair<string, int> > TFileList;
typedef vector<pair<vector<float>, int> > TFeatures;

typedef Matrix<float> FImage; // "слепок" изображения; матрица для модулей и углов
typedef Matrix<vector<float> > HistoMatrix; // матрица для гистограмм

#define CELLS 5 // делим изображение CELLS x CELLS блоков
#define SEGMENTS 20 // делим область изменения направления градиента на сегменты
#define COLOR_CELLS 6
#define LBP_CELLS 6

// Load list of files and its labels from 'data_file' and
// stores it in 'file_list'
void LoadFileList(const string& data_file, TFileList* file_list) {
    ifstream stream(data_file.c_str());

    string filename;
    int label;
    
    int char_idx = data_file.size() - 1;
    for (; char_idx >= 0; --char_idx)
        if (data_file[char_idx] == '/' || data_file[char_idx] == '\\')
            break;
    string data_path = data_file.substr(0,char_idx+1);
    
    while(!stream.eof() && !stream.fail()) {
        stream >> filename >> label;
        if (filename.size())
            file_list->push_back(make_pair(data_path + filename, label));
    }

    stream.close();
}

// Load images by list of files 'file_list' and store them in 'data_set'
void LoadImages(const TFileList& file_list, TDataSet* data_set) {
    for (size_t img_idx = 0; img_idx < file_list.size(); ++img_idx) {
            // Create image
        BMP* image = new BMP();
            // Read image from file
        image->ReadFromFile(file_list[img_idx].first.c_str());
            // Add image and it's label to dataset
        data_set->push_back(make_pair(image, file_list[img_idx].second));
    }
}

// Save result of prediction to file
void SavePredictions(const TFileList& file_list,
                     const TLabels& labels, 
                     const string& prediction_file) {
        // Check that list of files and list of labels has equal size 
    assert(file_list.size() == labels.size());
        // Open 'prediction_file' for writing
    ofstream stream(prediction_file.c_str());

        // Write file names and labels to stream
    for (size_t image_idx = 0; image_idx < file_list.size(); ++image_idx)
        stream << file_list[image_idx].first << " " << labels[image_idx] << endl;
    stream.close();
}

// Преобразование изображения в оттенки серого и изображения в формат MImage заодно
FImage grayscale(BMP & image) 
{
    FImage result(image.TellHeight(), image.TellWidth());

    // Y = 0.299R + 0.587G + 0.114B - яркость пикселя изображения
    for (uint i = 0; i < result.n_rows; i++) {
        for (uint j = 0; j < result.n_cols; j++) {
            RGBApixel *pixel = image(j, i);
            float gs_pix = 0.299 * pixel->Red + 0.587 * pixel->Green + 0.114 * pixel->Blue; // яркость пикселя

            result(i, j) = gs_pix; // как бы выставляем все каналы одинаковыми
        }
    }

    return result;
}

// горизонтальный фильтр Собеля
FImage sobel_x(const FImage & image)
{
    FImage result(image.n_rows, image.n_cols); // по краям остаются нули, но теперь это легко обрезать

    for (uint i = 1; i < image.n_rows - 1; i++) {
    	for (uint j = 1; j < image.n_cols - 1; j++) {
            float pixel_left = image(i, j - 1);
            float pixel = image(i, j);
            float pixel_right = image(i, j + 1);

            result(i, j) = (-1) * pixel_left + 0 * pixel + 1 * pixel_right;
	}
    }

    return result.submatrix(1, 1, result.n_rows - 2, result.n_cols - 2); // обрезали
}

// вертикальный фильтр Собеля
// переполнения быть не должно
FImage sobel_y(const FImage & image)
{
    FImage result(image.n_rows, image.n_cols); // по краям остаются нули, но теперь это легко обрезать

    for (uint i = 1; i < image.n_rows - 1; i++) {
    	for (uint j = 1; j < image.n_cols - 1; j++) {
            float pixel_up = image(i - 1, j);
            float pixel = image(i, j);
            float pixel_down = image(i + 1, j);

            result(i, j) = 1 * pixel_up + 0 * pixel + (-1) * pixel_down; 
	}
    }

    return result.submatrix(1, 1, result.n_rows - 2, result. n_cols - 2); // обрезали
}

FImage grad_abs(const FImage & x, const FImage & y)
{
    FImage result(x.n_rows, x.n_cols); 

    for (uint i = 0; i < x.n_rows; i++) {
        for (uint j = 0; j < x.n_cols; j++) {
            result(i, j) = sqrt(x(i, j) * x(i, j) + y(i, j) * y(i, j));
        }
    }

    return result;
}

// число пи - M_PI
FImage grad_dest(const FImage & x, const FImage & y)
{
    FImage result(x.n_rows, x.n_cols);

    for (uint i = 0; i < x.n_rows; i++) {
        for (uint j = 0; j < x.n_cols; j++) {
            result(i, j) = atan2(y(i, j), x(i, j)); // atan2 вроде контролирует нули  
        }
    }

    return result;
}

vector<float> color_features(BMP & image)
{
    vector<float> result; // вектор цветов

    FImage red(COLOR_CELLS, COLOR_CELLS), 
           green(COLOR_CELLS, COLOR_CELLS), 
           blue(COLOR_CELLS, COLOR_CELLS); // матрицы для средних цветов каждой клетки

    // обнуляем эти матрицы
    for (uint i = 0; i < red.n_rows; i++) {
        for (uint j = 0; j < red.n_cols; j++) {
            red(i, j) = green(i, j) = blue(i, j) = 0;
        }
    }

    int cell_h = image.TellHeight() / COLOR_CELLS, cell_w = image.TellWidth() / COLOR_CELLS; // ширина и высота одной клетки в пикселях
    for (int i = 0; i < image.TellHeight(); i++) {
        for (int j = 0; j < image.TellWidth(); j++) {
            // нужно определить в какую именно клетку по вертикали и горизонтали попадает пиксель
            uint place_i = i / cell_h, place_j = j / cell_w; // из какой пиксель клетки
            if (place_i >= COLOR_CELLS) { place_i = COLOR_CELLS - 1; } // боковые пиксели относятся к последней клетке
            if (place_j >= COLOR_CELLS) { place_j = COLOR_CELLS - 1; }

            RGBApixel *pixel = image(j, i); // вытаскиваем пиксель

            red(place_i, place_j) += pixel->Red; 
            green(place_i, place_j) += pixel->Green;
            blue(place_i, place_j) += pixel->Blue;
        }
    }

    // считаем средний цвет для каждой клетки как среднее арифметическое
    for (uint i = 0; i < red.n_rows; i++) {
        for (uint j = 0; j < red.n_cols; j++) {
            int num_pix; // число пикселей в клетке
            
            if (i == red.n_rows - 1) { // по краю снизу клетка может быть больше обычного
                num_pix = cell_h + image.TellHeight() % COLOR_CELLS; 
            } else {
                num_pix = cell_h;
            }

            if (j == red.n_cols - 1) { // по краю справа клетка может быть больше обычного
                num_pix *= cell_w + image.TellWidth() % COLOR_CELLS; 
            } else {
                num_pix *= cell_w;
            }

            red(i, j) /= num_pix;
            green(i, j) /= num_pix;
            blue(i, j) /= num_pix;

            // сразу нормируем и заносим в результирующий вектор
            result.push_back(red(i, j) / 255);
            result.push_back(green(i, j) / 255);
            result.push_back(blue(i, j) / 255);
        }
    }

    return result;
}

vector<float> local_binary_patterns(const FImage & image)
{
    vector<float> result;

    FImage bins(image.n_rows - 2, image.n_cols - 2);
    for (uint i = 1; i < image.n_rows - 1; i++) {
        for (uint j = 1; j < image.n_cols - 1; j++) {
            // смотрим на соседей каждого пикселя
            uint nei = 0; // какой по счёту сосед
            uint bin_code = 0; // бинарный код для (i, j)-го пикселя

            for (uint nei_i = i - 1; nei_i <= i + 1; nei_i++) {
                for (uint nei_j = j - 1; nei_j <= j + 1; nei_j++) {

                    if ((nei_i != i) || (nei_j != j)) { // если это не сам пиксель
                        if (image(i, j) <= image(nei_i, nei_j)) { bin_code = bin_code | (1 << nei); } 
                        
                        nei++; // следующий сосед; всего их 8
                    }
                    
                }
            }
           
            bins(i - 1, j - 1) = bin_code; // теперь каждый пиксель имеет свой бинарный код, который число 0..255
        }
    }

    HistoMatrix histo(LBP_CELLS, LBP_CELLS); // гистограммы каждой клетки
    for (uint i = 0; i < LBP_CELLS; i++) {
        for (uint j = 0; j < LBP_CELLS; j++) {
            for (uint k = 0; k < 256; k++) { // обнуляем каждую гистограмму
                histo(i, j).push_back(0);
            }
        }
    }

    uint cell_h = bins.n_rows / LBP_CELLS, cell_w = bins.n_cols / LBP_CELLS; // ширина и высота одной клетки в пикселях
    // если cell_h или cell_w получается меньше остатка от деления на LBP_CELLS, то клетки снизу и справа сильно больше остальных
    // ну и ничего страшного, исправления получаются недокостылями
    for (uint i = 0; i < bins.n_rows; i++) {
        for (uint j = 0; j < bins.n_cols; j++) {
            // нужно определить в какую именно клетку по вертикали и горизонтали попадает пиксель
            // а потом найти для него место в гистограмме
            uint place_i = i / cell_h, place_j = j / cell_w; // из какой пиксель клетки
            if (place_i >= LBP_CELLS) { place_i = LBP_CELLS - 1; } // боковые пиксели относятся к последней клетке
            if (place_j >= LBP_CELLS) { place_j = LBP_CELLS - 1; }  

            histo(place_i, place_j)[bins(i, j)]++; // формируем гистограмму
        }
    }

    FImage norms(LBP_CELLS, LBP_CELLS); // тут считаем евклидову норму каждой гистограммы
    for (uint i = 0; i < LBP_CELLS; i++) {
        for (uint j = 0; j < LBP_CELLS; j++) {

            norms(i, j) = 0;
            for (uint k = 0; k < 256; k++) {
                norms(i, j) += histo(i, j)[k] * histo(i, j)[k];
            }
            norms(i, j) = sqrt(norms(i, j));

            if (norms(i, j) > 0) {
                for (uint k = 0; k < 256; k++) {
                    histo(i, j)[k] /= norms(i, j); // нормализуем гистограммы
                }
            }
            
            result.insert(result.end(), histo(i, j).begin(), histo(i, j).end()); // конкатенируем все гистограммы
        }
    }

    return result;
}

// Exatract features from dataset.
// You should implement this function by yourself =)
void ExtractFeatures(const TDataSet& data_set, TFeatures* features) {
    for (size_t image_idx = 0; image_idx < data_set.size(); ++image_idx) {
        // PLACE YOUR CODE HERE
        // Remove this sample code and place your feature extraction code here

        BMP image = *(data_set[image_idx].first); // изображение, которое мучаем

        // цветовые признаки
        vector<float> color = color_features(image);
        // конец цветовых признаков

        // HOG
        FImage gs_image = grayscale(image); // преобразуем в оттенки серого
        
        gs_image = gs_image.extra_borders(1, 1); // дополняем границы

        // локальные бинарные шаблоны
        vector<float> locbinpat = local_binary_patterns(gs_image);
        // конец локальных бинарных шаблонов

        FImage image_vx = sobel_x(gs_image); // горизонтальная составляющая вектора градиента
        FImage image_vy = sobel_y(gs_image); // вертикальная составляющая вектора градиента

        FImage v_abs = grad_abs(image_vx, image_vy); // модуль вектора градиента
        FImage v_dest = grad_dest(image_vx, image_vy); // направление вектора градиента
	
        // ненормированное заполнение гистограмм
        FImage norms(CELLS, CELLS); // матрица из норм клеток
        HistoMatrix histo(CELLS, CELLS); // гистограммы каждой клетки

        for (uint i = 0; i < CELLS; i++) {
            for (uint j = 0; j < CELLS; j++) {
                norms(i, j) = 0; // обнуляем матрицу норм

                for (uint k = 0; k < SEGMENTS; k++) { // обнуляем каждую гистограмму
                    histo(i, j).push_back(0);
                }
            }
        }

        uint cell_h = v_abs.n_rows / CELLS, cell_w = v_abs.n_cols / CELLS; // ширина и высота одной клетки в пикселях
        double ang_seg = 2 * M_PI / SEGMENTS; // доля угла в сегменте
        for (uint i = 0; i < v_abs.n_rows; i++) {
            for (uint j = 0; j < v_abs.n_cols; j++) {
                // нужно определить в какую именно клетку по вертикали и горизонтали попадает пиксель
                // а потом найти для него место в гистограмме
                uint place_i = i / cell_h, place_j = j / cell_w; // из какой пиксель клетки
                if (place_i >= CELLS) { place_i = CELLS - 1; } // боковые пиксели относятся к последней клетке
                if (place_j >= CELLS) { place_j = CELLS - 1; }

                int place_ang = (v_dest(i, j) + M_PI) / ang_seg; // сегмент, в который попадает угол
                if (place_ang >= SEGMENTS) { place_ang = SEGMENTS - 1; } // на случай 2 * M_PI

                histo(place_i, place_j)[place_ang] += v_abs(i, j);
                norms(place_i, place_j) += v_abs(i, j) * v_abs(i, j); // копим норму
            }
        }

        for (uint i = 0; i < CELLS; i++) {
            for (uint j = 0; j < CELLS; j++) {
                norms(i, j) = sqrt(norms(i, j)); // евклидова норма клетки

                for (uint k = 0; k < SEGMENTS; k++) { // и сразу нормируем гистограммы
                    if (norms(i, j) > 0) {
                        histo(i, j)[k] /= norms(i, j);
                    }
                }
            }
        }

        // формируем дескриптор
        vector<float> desc;
        for (uint i = 0; i < CELLS; i++) {
            for (uint j = 0; j < CELLS; j++) {
                desc.insert(desc.end(), histo(i, j).begin(), histo(i, j).end());
            }
        }
        desc.insert(desc.end(), color.begin(), color.end()); // цветовые признаки
        desc.insert(desc.end(), locbinpat.begin(), locbinpat.end()); // локальные бинарные шаблоны

        (*features).push_back(make_pair(desc, data_set[image_idx].second));

        // End of sample code
    }
}

// Clear dataset structure
void ClearDataset(TDataSet* data_set) {
        // Delete all images from dataset
    for (size_t image_idx = 0; image_idx < data_set->size(); ++image_idx)
        delete (*data_set)[image_idx].first;
        // Clear dataset
    data_set->clear();
}

// Train SVM classifier using data from 'data_file' and save trained model
// to 'model_file'
void TrainClassifier(const string& data_file, const string& model_file) {
        // List of image file names and its labels
    TFileList file_list;
        // Structure of images and its labels
    TDataSet data_set;
        // Structure of features of images and its labels
    TFeatures features;
        // Model which would be trained
    TModel model;
        // Parameters of classifier
    TClassifierParams params;
    
        // Load list of image file names and its labels
    LoadFileList(data_file, &file_list);
        // Load images
    LoadImages(file_list, &data_set);
        // Extract features from images
    ExtractFeatures(data_set, &features);

        // PLACE YOUR CODE HERE
        // You can change parameters of classifier here
    params.C = 0.01;
    TClassifier classifier(params);
        // Train classifier
    classifier.Train(features, &model);
        // Save model to file
    model.Save(model_file);
        // Clear dataset structure
    ClearDataset(&data_set);
}

// Predict data from 'data_file' using model from 'model_file' and
// save predictions to 'prediction_file'
void PredictData(const string& data_file,
                 const string& model_file,
                 const string& prediction_file) {
        // List of image file names and its labels
    TFileList file_list;
        // Structure of images and its labels
    TDataSet data_set;
        // Structure of features of images and its labels
    TFeatures features;
        // List of image labels
    TLabels labels;

        // Load list of image file names and its labels
    LoadFileList(data_file, &file_list);
        // Load images
    LoadImages(file_list, &data_set);
        // Extract features from images
    ExtractFeatures(data_set, &features);

        // Classifier 
    TClassifier classifier = TClassifier(TClassifierParams());
        // Trained model
    TModel model;
        // Load model from file
    model.Load(model_file);
        // Predict images by its features using 'model' and store predictions
        // to 'labels'
    classifier.Predict(features, model, &labels);

        // Save predictions
    SavePredictions(file_list, labels, prediction_file);
        // Clear dataset structure
    ClearDataset(&data_set);
}

int main(int argc, char** argv) {
    // Command line options parser
    ArgvParser cmd;
        // Description of program
    cmd.setIntroductoryDescription("Machine graphics course, task 2. CMC MSU, 2014.");
        // Add help option
    cmd.setHelpOption("h", "help", "Print this help message");
        // Add other options
    cmd.defineOption("data_set", "File with dataset",
        ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOption("model", "Path to file to save or load model",
        ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOption("predicted_labels", "Path to file to save prediction results",
        ArgvParser::OptionRequiresValue);
    cmd.defineOption("train", "Train classifier");
    cmd.defineOption("predict", "Predict dataset");
        
        // Add options aliases
    cmd.defineOptionAlternative("data_set", "d");
    cmd.defineOptionAlternative("model", "m");
    cmd.defineOptionAlternative("predicted_labels", "l");
    cmd.defineOptionAlternative("train", "t");
    cmd.defineOptionAlternative("predict", "p");

        // Parse options
    int result = cmd.parse(argc, argv);

        // Check for errors or help option
    if (result) {
        cout << cmd.parseErrorDescription(result) << endl;
        return result;
    }

        // Get values 
    string data_file = cmd.optionValue("data_set");
    string model_file = cmd.optionValue("model");
    bool train = cmd.foundOption("train");
    bool predict = cmd.foundOption("predict");

        // If we need to train classifier
    if (train)
        TrainClassifier(data_file, model_file);
        // If we need to predict data
    if (predict) {
            // You must declare file to save images
        if (!cmd.foundOption("predicted_labels")) {
            cerr << "Error! Option --predicted_labels not found!" << endl;
            return 1;
        }
            // File to save predictions
        string prediction_file = cmd.optionValue("predicted_labels");
            // Predict data
        PredictData(data_file, model_file, prediction_file);
    }
}
