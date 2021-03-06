#include <iostream>
#include <mxnet/c_predict_api.h>
#include <mxnet/c_api.h>
#include <fstream>
#include <cassert>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <bitset>

#ifndef MSHADOW_USE_CBLAS
#define MSHADOW_USE_CBLAS 1
#endif

class BufferFile {
public :
    std::string file_path_;
    int length_;
    char *buffer_;

    explicit BufferFile(const std::string &file_path)
            : file_path_(file_path) {

        std::ifstream ifs(file_path.c_str(), std::ios::in | std::ios::binary);
        if (!ifs) {
            std::cerr << "Can't open the file. Please check " << file_path << ". \n";
            length_ = 0;
            buffer_ = NULL;
            return;
        }

        ifs.seekg(0, std::ios::end);
        length_ = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        std::cout << file_path.c_str() << " ... " << length_ << " bytes\n";

        buffer_ = new char[sizeof(char) * length_];
        ifs.read(buffer_, length_);
        ifs.close();
    }

    int GetLength() {
        return length_;
    }

    char *GetBuffer() {
        return buffer_;
    }

    ~BufferFile() {
        if (buffer_) {
            delete[] buffer_;
            buffer_ = NULL;
        }
    }
};

class predictor{
private:
    PredictorHandle handler = 0;
    int width = 256;
    int height = 32;
    int channels = 1;
public:
    predictor(const std::string &json_file, const std::string &para_file) : channels(1) {
        BufferFile json(json_file);
        BufferFile para(para_file);
        mx_uint num_input_nodes = 10;
        const char *input_key[10] = {"data", "l0_init_c", "l1_init_c", "l2_init_c", "l3_init_c", "l0_init_h", "l1_init_h",
                                     "l2_init_h", "l3_init_h", "label"};
        const char **input_keys = input_key;
        const mx_uint input_shape_indptr[11] = {0, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22};
        int num_hidden = 128;
        const mx_uint input_shape_data[22] = {
                1, static_cast<mx_uint>(channels), static_cast<mx_uint>(height), static_cast<mx_uint>(width),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint>(num_hidden),
                1, static_cast<mx_uint >(24)
        };

        MXPredCreate((const char *) json.GetBuffer(),
                     (const char *) para.GetBuffer(),
                     static_cast<size_t>(para.GetLength()),
                     1,
                     0,
                     num_input_nodes,
                     input_keys,
                     input_shape_indptr,
                     input_shape_data,
                     &handler);
    }

    std::string predict(const std::vector<uchar> &byte_data) {
        int image_size = width * height;
        std::vector<mx_float> image_data = std::vector<mx_float>(image_size);
        GetImage(byte_data, image_data.data(), 1, cv::Size(256, 32));
        MXPredSetInput(handler, "data", image_data.data(), image_size);
        MXPredForward(handler);

        mx_uint output_index = 0;

        mx_uint *shape = 0;
        mx_uint shape_len;

        // Get Output Result
        MXPredGetOutputShape(handler, output_index, &shape, &shape_len);
        size_t size = 1;
        for (mx_uint i = 0; i < shape_len; ++i) size *= shape[i];

        std::vector<float> data(size);
        MXPredGetOutput(handler, output_index, &(data[0]), size);
        char value[11] = {' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
        std::vector<char> result;
        for (int i = 0; i < 32; ++i) {
            int argmax = 0;
            for (int j = 1; j < 11; ++j) {
                if (data[i * 11 + j] - data[i * 11 + argmax] > 0) {
                    argmax = j;
                }
            }
            if (result.empty() || result[result.size() - 1] != value[argmax]) {
                result.push_back(value[argmax]);
            }
        }
        std::string to_return;
        for (char i : result) {
            if (i != ' ') {
                to_return += i;
            }
        }
        return to_return;
    }

    ~predictor() {
        MXPredFree(handler);
    }

    void GetImage(const std::vector<uchar>& byte_data,
                      mx_float *image_data, const int channels,
                      const cv::Size resize_size) {
        cv::Mat im = cv::Mat(byte_data,true);
        if (im.empty()) {
            assert(false);
        }

        cv::imshow("test",im);
        cv::waitKey(0);

        mx_float *ptr_image = image_data;

        for (int i = 0; i < im.rows; i++) {
            uchar *data = im.ptr<uchar>(i);

            for (int j = 0; j < im.cols; j++) {
                *ptr_image++ = static_cast<mx_float>(*data++);;
            }
        }
    }
};
