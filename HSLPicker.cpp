//-------------------------------------------------------------------------
//
//   Joe Kniss
//    HSL Color Space Picker Widget
//  HSLPickWidget.cpp : rotates a HSL ball and returns a color
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

#include "HSLPicker.h"
#include "gluvv.h"
#include <stdio.h>
#include <GL/glut.h>
#include "VectorMath.h"


//============================================================ HSLPicker
//======================================================================
HSLPicker::HSLPicker()
{
  hue   = 0;
  sat   = 1;
  lev   = .5;
}

//=========================================================== ~HSLPicker
//======================================================================
HSLPicker::~HSLPicker()
{

}

//=============================================================== update
//======================================================================
void HSLPicker::updateHL(float dh, float dl)
{
  hue = (hue+dh)> 1.0 ? (hue+dh-1.0) : ((hue+dh)< 0 ? (hue+dh+1.0) : (hue+dh));
  lev = (lev+dl) > 1 ? 1 : (lev+dl) < 0 ? 0 : (lev+dl);
}

void HSLPicker::updateS(float ds)
{
  lev = (sat+ds) > 1 ? 1 : (sat+ds) < 0 ? 0 : (sat+ds);
}


//============================================================ get color
//======================================================================
void HSLPicker::getColor(float col[3]){
  float m1, m2, fract, mid1, mid2;
  int sextant;

  float H = hue;
  float S = sat;
  float L = lev;

  if (S == 0) {
    col[0] = col[1] = col[2] = L;
    return;
  }
  /* else there is hue */
  if (L <= 0.5)
    m2 = L*(1+S);
  else
    m2 = L + S - L*S;
  m1 = 2*L - m2;
  if (1.0 == H)
    H = 0;
  H *= 6;
  sextant = (int) floor(H);
  fract = H - sextant;
  mid1 = m1 + fract*(m2 - m1);
  mid2 = m2 + fract*(m1 - m2);
  /* compared to HSVtoRGB: V -> m2, min -> m1 */
  switch (sextant) {
  case 0: { col[0] = m2;   col[1] = mid1; col[2] = m1;   break; }
  case 1: { col[0] = mid2; col[1] = m2;   col[2] = m1;   break; }
  case 2: { col[0] = m1;   col[1] = m2;   col[2] = mid1; break; }
  case 3: { col[0] = m1;   col[1] = mid2; col[2] = m2;   break; }
  case 4: { col[0] = mid1; col[1] = m1;   col[2] = m2;   break; }
  case 5: { col[0] = m2;   col[1] = m1;   col[2] = mid2; break; }
  }

}

//========================================================= reset
//===============================================================

void HSLPicker::reset(float h, float s, float l)
{
  hue = h;
  lev = l;
  sat = s;

}




