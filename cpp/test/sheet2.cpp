#include "raylib.h"
int main(int argc,char**argv){ SetTraceLogLevel(LOG_WARNING); int n=argc-2; int W=640,H=360,cols=2,rows=(n+1)/2; Image out=GenImageColor(cols*W,rows*H,BLACK);
 for(int i=0;i<n;i++){ Image im=LoadImage(argv[i+2]); ImageResize(&im,W,H); ImageDraw(&out,im,{0,0,(float)W,(float)H},{(float)(i%cols*W),(float)(i/cols*H),(float)W,(float)H},WHITE); UnloadImage(im);} ExportImage(out,argv[1]); }
