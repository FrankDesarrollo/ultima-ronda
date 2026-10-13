#include "raylib.h"
#include <cstdlib>
int main(int c,char**v){ SetTraceLogLevel(LOG_WARNING); Image im=LoadImage(v[1]); ImageCrop(&im,{(float)atof(v[3]),(float)atof(v[4]),(float)atof(v[5]),(float)atof(v[6])}); ExportImage(im,v[2]); }
