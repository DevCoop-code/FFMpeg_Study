//
//  main.cpp
//  ffMpeg_Tutorial
//
//  Created by HanGyo Jeong on 2019/11/27.
//  Copyright © 2019 HanGyoJeong. All rights reserved.
//

#include <SDL2/SDL.h>
#include <stdio.h>
#include <iostream>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

int main(int argc, const char * argv[]) {
    av_register_all();
    
    return 0;
}
