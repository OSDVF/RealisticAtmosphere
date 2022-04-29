//?#version 440
#ifndef DEBUG_H
#define DEBUG_H
#ifdef DEBUG
    uniform vec4 debugAttributes;
    #define DEBUG_NORMALS (debugAttributes.x == 1.0)
    #define DEBUG_RM (debugAttributes.y == 1.0)
    #define DEBUG_ATMO_OFF (debugAttributes.z == 1.0)
#else
    #define DEBUG_ATMO_OFF false
    #define DEBUG_NORMALS false
    #define DEBUG_RM false
#endif
#endif