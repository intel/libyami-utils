/*
 * Copyright (C) 2011-2014 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "decodehelp.h"

#include "common/utils.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <Yami.h>
#include <getopt.h>

using namespace YamiMediaCodec;

static void printHelp(const char* app)
{
    printf("%s <options>\n", app);
    printf("   -i media file to decode\n");
    printf("   -w wait before quit: 0:no-wait, 1:auto(jpeg wait), 2:wait\n");
    printf("   -f dumped fourcc [*]\n");
    printf("   -o dumped output dir\n");
    printf("   -n specify how many frames to be decoded\n");
    printf("   -m <render mode>\n");
    printf("     -2: print MD5 by per frame and the whole decoded file MD5\n");
    printf("     -1: skip video rendering [*]\n");
    printf("      0: dump video frame to file [*]\n");
    printf("      1: render to X window [*]\n");
    printf("      2: texture: render to Pixmap + texture from Pixmap [*]\n");
    printf("      3: texture: export video frame as drm name (RGBX) + texture from drm name\n");
    printf("      4: texture: export video frame as dma_buf(RGBX) + texutre from dma_buf\n");
    printf("      5: texture: export video frame as dma_buf(NV12) + texture from dma_buf. not implement yet\n");
    printf("      6: use external dma buf for decode and display, only v4l2decode support this\n");
    printf(" [*] v4l2decode doesn't support the option\n");
    printf("  --capi: use the codec capi to encode or decode, default(false)\n");
    printf("  --temporal-layer: decode SVC-T stream up to given layer, default 0, only vp8 support\n");
    printf("      0: decode all layers\n");
    printf("    N>0: decode the first N layers\n");
}

bool processCmdLine(int argc, char** argv, DecodeParameter* parameters)
{
    int32_t option_index;
    bool isSetFourcc = false;
    std::string outputFile;
    parameters->renderFrames = UINT_MAX;
    parameters->waitBeforeQuit = 1;
    parameters->renderMode = 1;
    parameters->inputFile = NULL;
    parameters->useCAPI = false;

    const struct option long_opts[] = {
        { "help", no_argument, NULL, 'h' },
        { "capi", no_argument, NULL, 0 },
        { "temporal-layer", required_argument, NULL, 0 },
        { NULL, no_argument, NULL, 0 }
    };

    char opt;
    while ((opt = getopt_long_only(argc, argv, "h:m:n:i:f:o:w:?", long_opts,&option_index)) != -1){
        switch (opt) {
        case 'h':
        case '?':
            printHelp(argv[0]);
            return false;
        case 'i':
            parameters->inputFile = optarg;
            break;
        case 'w':
            parameters->waitBeforeQuit = atoi(optarg);
            break;
        case 'm':
            parameters->renderMode = atoi(optarg);
            break;
        case 'n':
            parameters->renderFrames = atoi(optarg);
            break;
        case 'f':
            if (strlen(optarg) == 4) {
                parameters->renderFourcc = YAMI_FOURCC(toupper(optarg[0]), toupper(optarg[1]), toupper(optarg[2]), toupper(optarg[3]));
                isSetFourcc = true;
            }
            else {
                fprintf(stderr, "invalid fourcc: %s\n", optarg);
                return false;
            }
            break;
        case 'o':
            outputFile = optarg;
            break;
        case 0:
            switch (option_index) {
            case 1:
                parameters->useCAPI = true;
                break;
            case 2:
                parameters->temporalLayer = atoi(optarg);
                break;
            default:
                printHelp(argv[0]);
                break;
            }
            break;
        default:
            printHelp(argv[0]);
            break;
        }
    }
    if (optind < argc) {
        int indexOpt = optind;
        printf("unrecognized option: ");
        while (indexOpt < argc)
            printf("%s ", argv[indexOpt++]);
        printf("\n");
        printHelp(argv[0]);
        return false;
    }

    if (!parameters->inputFile) {
        fprintf(stderr, "no input media file specified.\n");
        return false;
    }
    if (outputFile.empty())
        outputFile = "./";
    parameters->outputFile = outputFile;
    if (!isSetFourcc)
        parameters->renderFourcc = guessFourcc(parameters->outputFile.c_str());
    int width, height;
    if (guessResolution(parameters->inputFile, width, height)) {
        parameters->width = width;
        parameters->height = height;
    }
    return true;
}

bool possibleWait(const char* mimeType, const DecodeParameter* parameters)
{
    // waitBeforeQuit 0:nowait, 1:auto(jpeg wait), 2:wait
    switch (parameters->waitBeforeQuit) {
    case 0:
        break;
    case 1:
        if (parameters->renderMode == 0 || strcmp(mimeType, YAMI_MIME_JPEG))
            break;
    case 2:
        fprintf(stdout, "press any key to continue ...");
        getchar();
        break;
    default:
        break;
    }

    return true;
}
