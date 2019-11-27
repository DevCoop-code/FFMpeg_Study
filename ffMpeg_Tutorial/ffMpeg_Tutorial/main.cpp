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
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

int main(int argc, const char * argv[]) {
    //Registers all available file formats and codecs with the library
    av_register_all();
    
    //Open the file
    AVFormatContext *pFormatCtx = NULL;
    /*
     Read File Header
     */
    //Reads file header and Stores information about the file format in the AVFormatContext
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, 0) != 0)
    {
        return -1;  //Couldn't open file
    }
    
    /*
     Check out the stream information
     */
    //Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        return -1;  //Couldn't find stream information
    }
    
    /*
     Dump information about file onto standard error
     */
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    //pFormatCtx->streams :Array of pointters
    //pFormatCtx->nb_streams :Number of elements in AVFormatContext.streams
    int videoStream = -1;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    
    //Find the first video stream
    for(int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStream = i;
            break;
        }
    }
    if(videoStream == -1)
        return -1;
    
    //Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    AVCodec *pCodec = NULL;
    //Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec! \n");
        return -1;  //Codec not found
    }
    //Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    //Must not use the AVCodecContext from the video stream directly!
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
    {
        fprintf(stderr, "Couldn't copy codec context \n");
        return -1;  //Error copying codec context
    }
    //Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1;  //Could not open codec
    
    return 0;
}
