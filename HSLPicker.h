//-------------------------------------------------------------------------
//
//   Joe Kniss
//    HSL Color Space Picker Widget
//  HSLPickWidget.h : rotates a HSL ball and returns a color
//     3-20-01
//                   ________    ____   ___ 
//                  |        \  /    | /  /
//                  +---+     \/     |/  /
//                  +--+|  |\    /|     < 
//                  |  ||  | \  / |  |\  \ 
//                  |      |  \/  |  | \  \ 
//                   \_____|      |__|  \__\
//                       Copyright  2001 
//                      Joe Michael Kniss
//               "All Your Base are Belong to Us"
//-------------------------------------------------------------------------


#ifndef __HSLPICKER_DOT_H
#define __HSLPICKER_DOT_H

#include "gluvvPrimitive.h"
#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#define HSLPWIDGNAME 22243222

struct gluvvGlobal;

class HSLPicker{
 public:
  HSLPicker();
  ~HSLPicker();

                                     //update value and level
  void updateHL(float dh, float dl); //by percent change in screen space
  void updateS(float ds);            // saturation... dito
  void getColor(float col[3]);       //rgb color

  void reset(float h=0, float s=0, float l=0);

 private:
  float hue; //[0..1]
  float lev; //[0..1]
  float sat; //[0..1]


};

#endif

