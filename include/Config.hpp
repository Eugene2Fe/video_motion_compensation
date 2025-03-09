#ifndef CONFIG_H
#define CONFIG_H

const int GOOD_FEATURES_MAX_POINTS = 200;  // кол-во точек для отслеживания
const double GOOD_FEATURES_POINT_QUALITY = 0.01;  // коэф качества точек
const int GOOD_FEATURES_POINTS_MIN_DIST_PX = 30;  // пикселей между хорошими точками отслеживания
const int NFRAMES_SMOOTH_COEF = 10;  // сглаживать по скока кадров влево вправо траекторию движения
const int MAX_CROP_PIXELS = 500;     // чтобы не ввели кроп на миллион
const int SUCCESS_TRACKING_STATUS = 1;  // хорошие точки имеют статус 1
const int ADDITION_PREVIEW_OFFSET = 10;  // рамка между видосами в дебаг компейр режиме

#endif  // CONFIG_H