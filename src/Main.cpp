#include <cmath>
#include <opencv2/opencv.hpp>

#include "../include/Config.hpp"
#include "GrayscaleConverter.hpp"
#include "Logger.hpp"

bool AUTO_BORDER_CROP_PIXELS = false;
int BORDER_CROP_PIXELS = DEFAULT_BORDER_CROP_PIXELS;
bool DEBUG = false;

struct VideoInfo {
    double frame_width;
    double frame_height;
    double frame_rate;
    double total_frames;
    double duration;

    void print() const {
        std::cout << "Информация о видео:" << std::endl;
        std::cout << "  Разрешение: " << frame_width << "x" << frame_height << std::endl;
        std::cout << "  Частота кадров: " << frame_rate << " FPS" << std::endl;
        std::cout << "  Количество кадров: " << total_frames << std::endl;
        std::cout << "  Длительность: " << duration << " секунд" << std::endl;
    }
};

struct FrameTransformation {
    FrameTransformation() {}
    FrameTransformation(double shift_x, double shift_y, double rotation_angle) {
        delta_x = shift_x;
        delta_y = shift_y;
        delta_angle = rotation_angle;
    }
    double delta_x;
    double delta_y;
    double delta_angle;
};

struct MotionTrajectory {
    MotionTrajectory() {}
    MotionTrajectory(double coord_x, double coord_y, double _angle) {
        position_x = coord_x;
        position_y = coord_y;
        angle = _angle;
    }
    double position_x;
    double position_y;
    double angle;
};

bool isFFmpegEnabled() {
    std::string build_info = cv::getBuildInformation();
    return build_info.find("FFMPEG") != std::string::npos;
}

void printHelp() {
    std::cout << "Usage: ./VideoMotionCompensation [video.file] [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --debug               Enable debug mode. Draw compared videos : Default:ON"
              << std::endl;
    std::cout << "  --BORDER_CROP_PIXELS=N  Set border crop pixels (default: AUTO CALCULATED)"
              << std::endl;
    std::cout << "  --help                Show this help message" << std::endl;
}

void processCLIArgs(int argc, char **argv, Logger &logger) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printHelp();
            exit(0);
        } else if (arg == "--debug") {
            DEBUG = true;
        } else if (arg == "--BORDER_CROP_PIXELS=AUTO") {
            AUTO_BORDER_CROP_PIXELS = true;
        } else if (arg.rfind("--BORDER_CROP_PIXELS=", 0) == 0) {
            try {
                BORDER_CROP_PIXELS = std::stoi(arg.substr(21));
                if (BORDER_CROP_PIXELS < 0 || BORDER_CROP_PIXELS > MAX_CROP_PIXELS) {
                    logger.log(LogLevel::ERROR,
                               "Ошибка: Некорректное значение для BORDER_CROP_PIXELS!");
                    exit(-1);
                }
                AUTO_BORDER_CROP_PIXELS = false;
            } catch (const std::exception &) {
                std::cerr << "Ошибка: Некорректное значение для BORDER_CROP_PIXELS!" << std::endl;
                exit(-1);
            }
        } else if (arg[0] == '-') {
            std::cerr << "Предупреждение: Неизвестный флаг '" << arg << "' будет проигнорирован."
                      << std::endl;
        }
    }
}

VideoInfo getVideoInfo(cv::VideoCapture &video_reader) {
    VideoInfo video_info = {
        video_reader.get(cv::CAP_PROP_FRAME_WIDTH), video_reader.get(cv::CAP_PROP_FRAME_HEIGHT),
        video_reader.get(cv::CAP_PROP_FPS), video_reader.get(cv::CAP_PROP_FRAME_COUNT), 0.0};

    if (video_info.frame_rate > 0) {
        video_info.duration = video_info.total_frames / video_info.frame_rate;
    }

    return video_info;
}

std::vector<FrameTransformation> calculateFrameShifts(cv::VideoCapture &video_reader,
                                                      Logger &logger, const VideoInfo &video_info) {
    std::vector<FrameTransformation> frame_shift_info;
    cv::Mat last_good_Transformation;
    cv::Mat current_frame;
    cv::Mat previous_frame;
    video_reader >> previous_frame;
    cv::Mat previous_grey_frame = GrayscaleConverter::convertToGray(previous_frame);

    int frame_counter = 1;

    while (true) {
        video_reader >> current_frame;

        if (current_frame.empty()) {
            std::cout << " " << std::endl;
            logger.log(LogLevel::INFO, "Все фреймы входного видео обработаны!");
            break;
        }

        std::vector<cv::Point2f> all_keypoints_curr;
        std::vector<cv::Point2f> all_keypoints_prev;
        std::vector<cv::Point2f> filtered_keypoints_curr;
        std::vector<cv::Point2f> filtered_keypoints_prev;
        std::vector<uchar> tracking_status;
        std::vector<float> tracking_err;

        cv::Mat current_grey_frame = GrayscaleConverter::convertToGray(current_frame);

        goodFeaturesToTrack(previous_grey_frame, all_keypoints_prev, GOOD_FEATURES_MAX_POINTS,
                            GOOD_FEATURES_POINT_QUALITY, GOOD_FEATURES_POINTS_MIN_DIST_PX);
        calcOpticalFlowPyrLK(previous_grey_frame, current_grey_frame, all_keypoints_prev,
                             all_keypoints_curr, tracking_status, tracking_err);

        for (size_t i = 0; i < tracking_status.size(); i++) {
            if (tracking_status[i] == SUCCESS_TRACKING_STATUS) {
                filtered_keypoints_prev.push_back(all_keypoints_prev[i]);
                filtered_keypoints_curr.push_back(all_keypoints_curr[i]);
            }
        }

        cv::Mat T = estimateAffinePartial2D(filtered_keypoints_prev, filtered_keypoints_curr);

        if (T.empty()) {
            last_good_Transformation.copyTo(T);
            logger.log(LogLevel::INFO, "Преобразование не найдено, вероятно кадр статичный????");
        }

        T.copyTo(last_good_Transformation);

        double delta_x = T.at<double>(0, 2);
        double delta_y = T.at<double>(1, 2);
        double delta_angle = atan2(T.at<double>(1, 0), T.at<double>(0, 0));

        frame_shift_info.push_back(FrameTransformation(delta_x, delta_y, delta_angle));

        logger.log(LogLevel::TO_FILE_ONLY, "Кадр=", frame_counter, " delta_x=", delta_x,
                   " delta_y=", delta_y, " delta_angle=", delta_angle);

        current_frame.copyTo(previous_frame);
        current_grey_frame.copyTo(previous_grey_frame);

        std::cout << "\rОбработка кадра: " << frame_counter + 1 << " / " << video_info.total_frames
                  << " [" << std::string((frame_counter * 50) / video_info.total_frames, '=')
                  << std::string(50 - (frame_counter * 50) / video_info.total_frames, ' ') << "] "
                  << " Найденные точки: " << filtered_keypoints_prev.size() << std::flush;
        frame_counter++;
    }

    return frame_shift_info;
}

std::vector<MotionTrajectory> buildTrajectory(
    const std::vector<FrameTransformation> &frame_shift_info, Logger &logger) {
    double x = 0;
    double y = 0;
    double a = 0;
    std::vector<MotionTrajectory> trajectory;

    for (size_t i = 0; i < frame_shift_info.size(); i++) {
        x += frame_shift_info[i].delta_x;
        y += frame_shift_info[i].delta_y;
        a += frame_shift_info[i].delta_angle;
        trajectory.push_back(MotionTrajectory(x, y, a));

        logger.log(LogLevel::TO_FILE_ONLY, "Траектория => Кадр:", i + 1, ", x=", x, ", y=", y,
                   ", angle=", a);
    }

    return trajectory;
}

std::vector<MotionTrajectory> smoothTrajectory(const std::vector<MotionTrajectory> &trajectory,
                                               Logger &logger) {
    std::vector<MotionTrajectory> smoothed_trajectory;

    for (size_t i = 0; i < trajectory.size(); i++) {
        double sum_x = 0;
        double sum_y = 0;
        double sum_a = 0;
        int frames_processed = 0;

        for (int j = -NFRAMES_SMOOTH_COEF; j <= NFRAMES_SMOOTH_COEF; j++) {
            ptrdiff_t index = static_cast<ptrdiff_t>(i) + j;
            if (index >= 0 && index < static_cast<ptrdiff_t>(trajectory.size())) {
                sum_x += trajectory[index].position_x;
                sum_y += trajectory[index].position_y;
                sum_a += trajectory[index].angle;

                frames_processed++;
            }
        }

        double avg_x = sum_x / frames_processed;
        double avg_y = sum_y / frames_processed;
        double avg_a = sum_a / frames_processed;

        smoothed_trajectory.push_back(MotionTrajectory(avg_x, avg_y, avg_a));

        logger.log(LogLevel::TO_FILE_ONLY, "Сглаженная раектория => Кадр:", i + 1,
                   ", avg_x=", avg_x, ", avg_y=", avg_y, ", avg_angle=", avg_a);
    }

    return smoothed_trajectory;
}

std::vector<FrameTransformation> calculateCorrectedShifts(
    const std::vector<FrameTransformation> &frame_shift_info,
    const std::vector<MotionTrajectory> &smoothed_trajectory, double &max_diff_x,
    double &max_diff_y) {
    std::vector<FrameTransformation> new_frame_shift_info;
    double x = 0;
    double y = 0;
    double a = 0;
    max_diff_x = 0.0;
    max_diff_y = 0.0;

    for (size_t i = 0; i < frame_shift_info.size(); i++) {
        x += frame_shift_info[i].delta_x;
        y += frame_shift_info[i].delta_y;
        a += frame_shift_info[i].delta_angle;

        double diff_x = smoothed_trajectory[i].position_x - x;
        double diff_y = smoothed_trajectory[i].position_y - y;
        double diff_a = smoothed_trajectory[i].angle - a;

        max_diff_x = std::max(max_diff_x, diff_x);
        max_diff_y = std::max(max_diff_y, diff_y);

        double corrected_delta_x = frame_shift_info[i].delta_x + diff_x;
        double corrected_delta_y = frame_shift_info[i].delta_y + diff_y;
        double corrected_delta_angle = frame_shift_info[i].delta_angle + diff_a;

        new_frame_shift_info.push_back(
            FrameTransformation(corrected_delta_x, corrected_delta_y, corrected_delta_angle));
    }

    return new_frame_shift_info;
}

void writeStabilizedVideo(cv::VideoCapture &video_reader, cv::VideoWriter &video_writer,
                          const std::vector<FrameTransformation> &new_frame_shift_info, int crop_x,
                          int crop_y, Logger &logger) {
    int frame_counter = 0;
    video_reader.set(cv::CAP_PROP_POS_FRAMES, frame_counter);
    cv::Mat T(2, 3, CV_64F);
    cv::Mat current_frame;

    while (frame_counter < static_cast<int>(new_frame_shift_info.size())) {
        cv::Mat current_frame_rehab;

        try {
            video_reader >> current_frame;

            if (current_frame.empty()) {
                logger.log(LogLevel::INFO, "Готово, стабилизированное видео сохранено!");
                break;
            }

            T.at<double>(0, 0) = cos(new_frame_shift_info[frame_counter].delta_angle);
            T.at<double>(0, 1) = -sin(new_frame_shift_info[frame_counter].delta_angle);
            T.at<double>(1, 0) = sin(new_frame_shift_info[frame_counter].delta_angle);
            T.at<double>(1, 1) = cos(new_frame_shift_info[frame_counter].delta_angle);

            T.at<double>(0, 2) = new_frame_shift_info[frame_counter].delta_x;
            T.at<double>(1, 2) = new_frame_shift_info[frame_counter].delta_y;

            warpAffine(current_frame, current_frame_rehab, T, current_frame.size());

            if (crop_x * 2 >= current_frame_rehab.rows || crop_y * 2 >= current_frame_rehab.cols) {
                logger.log(LogLevel::ERROR,
                           "Ошибка: Обрезка выходит за границы кадра! Пропускаем кадр.");
                continue;
            }

            current_frame_rehab =
                current_frame_rehab(cv::Range(crop_x, current_frame_rehab.rows - crop_x),
                                    cv::Range(crop_y, current_frame_rehab.cols - crop_y));
            resize(current_frame_rehab, current_frame_rehab, current_frame.size());

            video_writer.write(current_frame_rehab);

        } catch (cv::Exception &e) {
            logger.log(LogLevel::ERROR, "OpenCV Exception: ", e.what(), " на кадре ",
                       frame_counter);
        } catch (std::exception &e) {
            logger.log(LogLevel::ERROR, "Exception: ", e.what(), " на кадре ", frame_counter);
        } catch (...) {
            logger.log(LogLevel::ERROR, "Неизвестная ошибка на кадре ", frame_counter);
        }

        if (DEBUG) {
            cv::Mat canvas = cv::Mat::zeros(current_frame.rows * 2 + ADDITION_PREVIEW_OFFSET,
                                            current_frame.cols, current_frame.type());
            current_frame.copyTo(canvas(cv::Range(0, current_frame.rows), cv::Range::all()));
            current_frame_rehab.copyTo(canvas(
                cv::Range(current_frame.rows + 10, current_frame.rows * 2 + 10), cv::Range::all()));

            if (canvas.cols > 1920 || canvas.rows > 1080) {
                resize(canvas, canvas, cv::Size(canvas.cols / 2, canvas.rows / 2));
            }

            cv::imshow("Difference View: Original (Top) vs Stabilized (Bottom)", canvas);
            cv::waitKey(20);
            frame_counter++;
        }
    }
}

int main(int argc, char **argv) {
    int threads = cv::getNumThreads();
    std::cout << "OpenCV работает в " << threads << " потоках" << std::endl;

    Logger logger("log.txt");

    if (argc < 2) {
        printHelp();
        return 0;
    }

    processCLIArgs(argc, argv, logger);

    std::cout << "Открываем файл: " << argv[1] << std::endl;
    cv::VideoCapture video_reader(argv[1]);

    if (!video_reader.isOpened()) {
        logger.log(LogLevel::ERROR, "Ошибка: не удалось открыть видео!");
        return -1;
    }

    if (!isFFmpegEnabled()) {
        std::cerr << "Ошибка: OpenCV собран без поддержки FFMPEG!" << std::endl;
        return -1;
    }

    VideoInfo video_info = getVideoInfo(video_reader);
    video_info.print();

    int codec_type = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    std::string input_filename(argv[1]);
    std::string output_filename;
    size_t dot_pos = input_filename.find_last_of(".");
    if (dot_pos != std::string::npos) {
        std::string extension = input_filename.substr(dot_pos);
        output_filename = input_filename.substr(0, dot_pos) + "_stabilized" + extension;
    } else {
        output_filename = input_filename + "_stabilized.mp4";
    }

    cv::VideoWriter video_writer(output_filename, codec_type, video_info.frame_rate,
                                 cv::Size(video_info.frame_width, video_info.frame_height));

    std::vector<FrameTransformation> frame_shift_info =
        calculateFrameShifts(video_reader, logger, video_info);

    std::vector<MotionTrajectory> trajectory = buildTrajectory(frame_shift_info, logger);
    std::vector<MotionTrajectory> smoothed_trajectory = smoothTrajectory(trajectory, logger);

    double max_diff_x = 0.0;
    double max_diff_y = 0.0;

    std::vector<FrameTransformation> new_frame_shift_info =
        calculateCorrectedShifts(frame_shift_info, smoothed_trajectory, max_diff_x, max_diff_y);

    int crop_x = 0;
    int crop_y = 0;
    if (AUTO_BORDER_CROP_PIXELS) {
        if (max_diff_x > max_diff_y) {
            crop_x = static_cast<int>(max_diff_x);
            crop_y = crop_x * video_info.frame_height / video_info.frame_width;
        } else {
            crop_y = static_cast<int>(max_diff_y);
            crop_x = crop_y * video_info.frame_width / video_info.frame_height;
        }
    } else {
        crop_y = BORDER_CROP_PIXELS;
        crop_x = BORDER_CROP_PIXELS * video_info.frame_width / video_info.frame_height;
    }

    logger.log(LogLevel::INFO, "Обрезка кадров выполнена, обрезаем по ширине на ", crop_x,
               " пикселей, по высоте на ", crop_y, " пикселей.");

    writeStabilizedVideo(video_reader, video_writer, new_frame_shift_info, crop_x, crop_y, logger);

    return 0;
}