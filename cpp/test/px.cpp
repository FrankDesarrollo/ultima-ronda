#include "raylib.h"
#include <cstdio>
#include <cstdlib>
int main(int c,char**v){ Image im=LoadImage(v[1]); for(int i=2;i+1<c;i+=2){ Color k=GetImageColor(im,atoi(v[i]),atoi(v[i+1])); printf("(%s,%s) = %d %d %d\n",v[i],v[i+1],k.r,k.g,k.b);} }
