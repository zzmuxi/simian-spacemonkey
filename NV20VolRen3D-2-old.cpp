//                   ________    ____   ___ 
//                  |        \  /    | /  /
//                  +---+     \/     |/  /
//                  +--+|  |\    /|     < 
//                  |  ||  | \  / |  |\  \ 
//                  |      |  \/  |  | \  \ 
//                   \_____|      |__|  \__\
//                       Copyright  2001 
//                      Joe Michael Kniss
//                    << jmk@cs.utah.edu >>
//               "All Your Base are Belong to Us"
//-------------------------------------------------------------------------
// NV20VolRen3D2.cpp: implementation of the NV20VolRen3D2 class.
//
//////////////////////////////////////////////////////////////////////


#ifdef WIN32
#include <windows.h>
#endif

#include "NV20VolRen3D-2.h"
#include <iostream.h>
#include "VectorMath.h"
#include "glUE.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NV20VolRen3D2::NV20VolRen3D2()
{
	go = 0;
}

NV20VolRen3D2::~NV20VolRen3D2()
{

}

//================================================================= INIT
//======================================================================

void NV20VolRen3D2::init()
{
	if(gluvv.mv){
		go = 1;
		createVolumes();
		create2DDepTex();
		createShadowTex();

		if(!(pbuff = new PBuffer(gluvv.light.buffsz[0], gluvv.light.buffsz[1], 
			                       GLUT_SINGLE | GLUT_RGBA))){
			cerr << "ERROR: PBuffer intialization failed!" << endl;
		} else {
			pbuff->Initialize(true);
			pbuff->MakeCurrent();
			glClearColor( 0.0, 0.0, 0.0, 0.0 );			
			glMatrixMode( GL_PROJECTION );
			glOrtho(0,gluvv.light.buffsz[0],0,gluvv.light.buffsz[1],1,3);
			glMatrixMode(GL_MODELVIEW);
			gluLookAt(0,0,2,
				        0,0,0,
								0,1,0);
			MakeCurrent();
			cerr << "PBuffer intialization succeded" << endl;
		}
	}

	cerr << "Using the Shadow Renderer!!!!!!!" << endl;
}

//================================================================= DRAW
//======================================================================

void NV20VolRen3D2::draw()
{
	if(!go) return;

	renderVolume();
	//renderSlice();

}

//======================================================== Render Volume
//======================================================================
//========== NOTES ===========================================
// Standard brick vertex ordering:
//  
//     (011)        (111)
//       6 +---------+ 7   Where 1's are the size of the brick
//        /|        /|      allong that axis
//       / |       / |
// (001)/  |(101) /  |
//   4 +---------+ 5 |
//     |   |     |   |(110) z axis
//     | 2 +-----+---+ 3    ^   
//     |  /(010) |  /       |   y axis
//     | /       | /        | /
//     |/        |/         |/
//   0 +---------+ 1        +-------> x axis 
//  (000)      (100)
//

void NV20VolRen3D2::renderVolume()
{
	if(gluvv.picking){
		return;
	}

	//-------------- Re-Scale Alpha values ---------------------------------
	if(gluvv.volren.scaleAlphas){
		if((lastSamp != gluvv.volren.sampleRate)||(gluvv.volren.loadTLUT)){  //see if the sample rate changed
			if((lastGoodSamp != gluvv.volren.goodSamp) || gluvv.volren.loadTLUT){ //good sample rate changed
				copyScale(gluvv.volren.goodSamp * 1/gluvv.volren.gamma, gDeptex);
				lastGoodSamp = gluvv.volren.goodSamp;
			}
			if((lastInteSamp != gluvv.volren.interactSamp) || gluvv.volren.loadTLUT){ //interact samp rate changed
				copyScale(gluvv.volren.interactSamp * 1/gluvv.volren.gamma, iDeptex);
				lastInteSamp = gluvv.volren.interactSamp;
			}
			
			if(gluvv.volren.sampleRate == gluvv.volren.goodSamp){ //which one do we load (good)
				loadDepTex(gDeptex);
				lastSamp = gluvv.volren.goodSamp;
			}
			else if(gluvv.volren.sampleRate == gluvv.volren.interactSamp){ //(interactive)
				loadDepTex(iDeptex);
				lastSamp = gluvv.volren.interactSamp;
			}
			if(gluvv.volren.loadTLUT){  //now load the transfer function
				loadDepTex(gluvv.volren.deptex2, deptex2Name);
			}
			gluvv.volren.loadTLUT = 0;
		}
		} else {  //just do gamma scale, don't update for the sample rate (for testing purposes)
		if(gluvv.volren.loadTLUT){
			copyScale(1/gluvv.volren.gamma, gDeptex);
			loadDepTex(gDeptex);
			loadDepTex(gluvv.volren.deptex2, deptex2Name);
			gluvv.volren.loadTLUT = 0;
		}
	}
	//-------------- end Re-Scale Alpha values ------------------------------
		
  //-------------- do dot product with clip and view dir
	float vdir[3];
	subV3(vdir, gluvv.env.eye, gluvv.clip.pos);
	normalizeV3(vdir);
	normalizeV3(gluvv.clip.dir);
	float dv = dotV3(vdir,gluvv.clip.dir);
	float globalModV[16]; //save original tranform
	glGetFloatv(GL_MODELVIEW_MATRIX, globalModV); //save the world model view
	//-------------- end do dot product with clip and view dir
	
	glPushMatrix();{ //move to the volume location
		glTranslatef(gluvv.rinfo.trans[0], //translate
					 			 gluvv.rinfo.trans[1], 
								 gluvv.rinfo.trans[2]);
		glMultMatrixf(gluvv.rinfo.xform);  //rotate
		glTranslatef(-gluvv.mv->xfSize/2,  //center
								 -gluvv.mv->yfSize/2,
								 -gluvv.mv->zfSize/2);

		//-------------- draw clip slices --------------------------------------
		for(int i=0; i<gluvv.mv->numSubVols; ++i){
			drawClip(i, dv, globalModV);			
		}
		//-------------- end draw clip slices ----------------------------------

		//----------------------------------------------------------------------
		//-------------- render the volume -------------------------------------
		//-- first compute the axis for rendering
		float axis[4]  = {0,0,1,0}; //3 for view direction and one for the dot
			 
		// modify slice axis for shadowing ------
		float ldir[3]; //figure out the light direction
		copyV3(ldir,gluvv.light.pos); //this is the position
		negateV3(ldir);               //this is the direction
		normalizeV3(ldir);            //normalize both vectors
		normalizeV3(axis);            //    light & axis
		float vdl = dotV3(ldir,axis);  //the dot product tells us to flip the axis

		if(vdl<=0){  //need to negate the axis
			//cerr << "back to front" << endl;
			negateV3(axis);
		} else {
			//cerr << "front to back" << endl;
		}
		//now compute  (v - l)/2 + l = halfway view and light
		float spn[3];            //slice plane normal 
		subV3(spn, axis, ldir);
		scaleV3(.5,spn);
		float halfv[3];
		addV3(halfv, spn, ldir);
		copyV3(axis, halfv);
		axis[3] = vdl; //this stores the value of the dot product
		axis[1] = -axis[1];//why? I have no idea, there must be something wrong with my math

		negateV3(axis);  //the actual direction for slicing
			
		// end modify slice axis for shadowing ------

		setupShaders();
		setupRegComb(vdl);
		setupPBuff();

		GLdouble mv[16];
		glGetDoublev(GL_MODELVIEW_MATRIX, mv);  //save modelview matrix

		renderBricks(mv, axis); //RENDER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		
		resetShaders();
		resetRegComb();
		//-------------- end render the volume ---------------------------------
		//----------------------------------------------------------------------

		//-------------- draw clip slices --------------------------------------
		for(i=0; i<gluvv.mv->numSubVols; ++i){
			drawClip(i, -dv, globalModV);			
		}
		//-------------- end draw clip slices ----------------------------------

	} glPopMatrix();

	resetShaders();
	resetRegComb();
	resetClips();

	if(gluvv.light.showView){
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, shadowName);
		glBegin(GL_QUADS);{
			glTexCoord2f(0,0);
			glVertex3f(0,0,0);
			glTexCoord2f(1,0);
			glVertex3f(1,0,0);
			glTexCoord2f(1,1);
			glVertex3f(1,1,0);
			glTexCoord2f(0,1);
			glVertex3f(0,1,0);
		} glEnd();
		glDisable(GL_TEXTURE_2D);
	}
}

//======================================================== Render Bricks
//======================================================================

void NV20VolRen3D2::renderBricks(GLdouble mv[16], GLfloat axis[4])
{
	
	if(gluvv.mv->numSubVols > 1){ //render multiple bricked volumes
		int *order = new int[gluvv.mv->numSubVols];   //the correct draw order
		float *zval = new float[gluvv.mv->numSubVols];//zvalue of each subvolume
		float ref[3];
		copyV3(ref, axis);
		negateV3(ref);
		scaleV3(100, ref);
		for(int i=0; i< gluvv.mv->numSubVols; ++i){ //compute zvalues
			Volume *v = &gluvv.mv->volumes[i];
			order[i] = i;
			float c[3] = {v->xfPos + v->xfSize/2.0, v->yfPos + v->yfSize/2.0, v->zfPos + v->zfSize/2.0};
			float center[3];
			translateV3W(center, mv, c);
			float sub[3];
			subV3(sub, ref, center);
			//zval[i] = normV3(sub);//(float)(center[0]*center[0] + center[1]*center[1] + center[2]*center[2]);
			zval[i] = (float)(center[0]*center[0] + center[1]*center[1] + center[2]*center[2]);
		}

		for(i=0; i< gluvv.mv->numSubVols-1; ++i){ //now sort
			for(int j=i+1; j<gluvv.mv->numSubVols; ++j){
				if(axis[3] <=0){ //back to front
					if(zval[order[i]] < zval[order[j]]){//test for swap
						int tmp = order[i];
						order[i] = order[j];
						order[j] = tmp;
					}
				} else { //front to back
					if(zval[order[i]] > zval[order[j]]){//test for swap
						int tmp = order[i];
						order[i] = order[j];
						order[j] = tmp;
					}
				}
			}
		}

		for(i=0 ; i< gluvv.mv->numSubVols; ++i){ //finaly render
			Volume *v = &gluvv.mv->volumes[order[i]];

			float sxf = v->xfSize;
			float syf = v->yfSize;
			float szf = v->zfSize;
			float hpx = 1.0/(gluvv.mv->xiSize*2.0);
			float hpy = 1.0/(gluvv.mv->yiSize*2.0);
			float hpz = 1.0/(gluvv.mv->ziSize*2.0);

			float vo[8][3] = {{0,0,0},{sxf,0,0},{0,syf,0},{sxf,syf,0},{0,0,szf},{sxf,0,szf},{0,syf,szf},{sxf,syf,szf}};
			float tx[8][3] = {{hpx,hpy,hpz},{1-hpx,hpy,hpz},{hpx,1-hpy,hpz},{1-hpx,1-hpy,hpz},
												{hpx,hpy,1-hpz},{1-hpx,hpy,1-hpz},{hpx,1-hpy,1-hpz},{1-hpx,1-hpy,1-hpz}};
			glPushMatrix(); {
				setupClips(order[i],vo,tx); 

				glTranslatef(v->xfPos, v->yfPos, v->zfPos);
	  		render3DVA(gluvv.volren.sampleRate,mv,order[i],vo,tx,axis);
				
			} glPopMatrix();
		}

	} else { //just render one volume!
		float sxf = gluvv.mv->xfSize;
		float syf = gluvv.mv->yfSize;
		float szf = gluvv.mv->zfSize;
		float vo[8][3] = {{0,0,0},{sxf,0,0},{0,syf,0},{sxf,syf,0},{0,0,szf},{sxf,0,szf},{0,syf,szf},{sxf,syf,szf}};
		float tx[8][3] = {{0,0,0},{1  ,0,0},{0,1  ,0},{1  ,1  ,0},{0,0,1  },{1  ,0,1  },{0,1  ,1  },{1  ,1  ,1  }};
		
		setupClips(0,vo,tx);

		render3DVA(gluvv.volren.sampleRate,mv,0,vo,tx,axis);
		
	}
}

//========================================================== Setup Clips
//======================================================================
void NV20VolRen3D2::setupClips(int vol, float vo[8][3], float tx[8][3])
{
	Volume *v = &gluvv.mv->volumes[vol];

	if(gluvv.clip.on && gluvv.clip.ortho){
				float cp[3];
				cp[0] = gluvv.clip.vpos[0] > v->xfPos ? 
					(gluvv.clip.vpos[0] < v->xfSize + v->xfPos ? gluvv.clip.vpos[0] - v->xfPos : v->xfSize) : 0;
				cp[1] = gluvv.clip.vpos[1] > v->yfPos ? 
					(gluvv.clip.vpos[1] < v->yfSize + v->yfPos ? gluvv.clip.vpos[1] - v->yfPos : v->yfSize) : 0;
				cp[2] = gluvv.clip.vpos[2] > v->zfPos ? 
					(gluvv.clip.vpos[2] < v->zfSize + v->zfPos ? gluvv.clip.vpos[2] - v->zfPos : v->zfSize) : 0;
				switch(gluvv.clip.oaxis){
				case VolRenAxisXPos:
					tx[1][0] = cp[0]/v->xfSize;
					tx[3][0] = cp[0]/v->xfSize;
					tx[5][0] = cp[0]/v->xfSize;
					tx[7][0] = cp[0]/v->xfSize;
					vo[1][0] = cp[0];
					vo[3][0] = cp[0];
					vo[5][0] = cp[0];
					vo[7][0] = cp[0];
					break;
				case VolRenAxisXNeg:
					tx[0][0] = cp[0]/v->xfSize;
					tx[2][0] = cp[0]/v->xfSize;
					tx[4][0] = cp[0]/v->xfSize;
					tx[6][0] = cp[0]/v->xfSize;
					vo[0][0] = cp[0];
					vo[2][0] = cp[0];
					vo[4][0] = cp[0];
					vo[6][0] = cp[0];
					break;
        case VolRenAxisYPos:
					tx[2][1] = cp[1]/v->yfSize;
					tx[3][1] = cp[1]/v->yfSize;
					tx[6][1] = cp[1]/v->yfSize;
					tx[7][1] = cp[1]/v->yfSize;
					vo[2][1] = cp[1];
					vo[3][1] = cp[1];
					vo[6][1] = cp[1];
					vo[7][1] = cp[1];
					break;
        case VolRenAxisYNeg:
					tx[0][1] = cp[1]/v->yfSize;
					tx[1][1] = cp[1]/v->yfSize;
					tx[4][1] = cp[1]/v->yfSize;
					tx[5][1] = cp[1]/v->yfSize;
					vo[0][1] = cp[1];
					vo[1][1] = cp[1];
					vo[4][1] = cp[1];
					vo[5][1] = cp[1];
					break;
        case VolRenAxisZPos:
					tx[4][2] = cp[2]/v->zfSize;
					tx[5][2] = cp[2]/v->zfSize;
					tx[6][2] = cp[2]/v->zfSize;
					tx[7][2] = cp[2]/v->zfSize;
					vo[4][2] = cp[2];
					vo[5][2] = cp[2];
					vo[6][2] = cp[2];
					vo[7][2] = cp[2];
					break;
        case VolRenAxisZNeg:
					tx[0][2] = cp[2]/v->zfSize;
					tx[1][2] = cp[2]/v->zfSize;
					tx[2][2] = cp[2]/v->zfSize;
					tx[3][2] = cp[2]/v->zfSize;
					vo[0][2] = cp[2];
					vo[1][2] = cp[2];
					vo[2][2] = cp[2];
					vo[3][2] = cp[2];
					break;
				}
	} 

}

//============================================================ drawClips
//======================================================================
void NV20VolRen3D2::drawClip(int vol, float dv, float wmv[16])
{
	if((!gluvv.clip.ortho)){
		switch(gluvv.dmode){
		case GDM_V1:
		case GDM_V1G:
		case GDM_V2:
		case GDM_VGH_VG:
		case GDM_VGH_V:
			cerr << "clipping" << endl;
		  glActiveTexture(GL_TEXTURE2_ARB); { //3rd & 4th axis, or clipping
				glEnable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_CULL_FRAGMENT_NV);
			}
			glPushMatrix();{                   //this sets the clipping plane
				glLoadIdentity();                //clips are set in world space
				glMultMatrixf(wmv);              //origional world space coords
				glTranslatef(gluvv.clip.pos[0],  //location of clipping plane
					           gluvv.clip.pos[1],
					           gluvv.clip.pos[2]);
				glMultMatrixf(gluvv.clip.xform); //rotation of clip plane
				GLdouble zup[4] = {0, 0, -1, 0}; // always in z direction
				glEnable(GL_CLIP_PLANE5);        //enable the gl clip plane
				glClipPlane(GL_CLIP_PLANE5, zup);
      } glPopMatrix();
			break;
		}
	}
	
	
	if((!gluvv.clip.on)||(!gluvv.clip.ortho)) return;
	
	Volume *v = &gluvv.mv->volumes[vol];
	float c[4][3];

	copyV3(c[0],gluvv.clip.corners[0]);
	copyV3(c[1],gluvv.clip.corners[1]);
	copyV3(c[2],gluvv.clip.corners[2]);
	copyV3(c[3],gluvv.clip.corners[3]);
	//this moves the clip plane to sub-volume space
	c[0][0] = CLAMP_ARB(0,c[0][0]-v->xfPos,v->xfSize);
	c[1][0] = CLAMP_ARB(0,c[1][0]-v->xfPos,v->xfSize);
	c[2][0] = CLAMP_ARB(0,c[2][0]-v->xfPos,v->xfSize);
	c[3][0] = CLAMP_ARB(0,c[3][0]-v->xfPos,v->xfSize);
	c[0][1] = CLAMP_ARB(0,c[0][1]-v->yfPos,v->yfSize);
	c[1][1] = CLAMP_ARB(0,c[1][1]-v->yfPos,v->yfSize);
	c[2][1] = CLAMP_ARB(0,c[2][1]-v->yfPos,v->yfSize);
	c[3][1] = CLAMP_ARB(0,c[3][1]-v->yfPos,v->yfSize);
	c[0][2] = CLAMP_ARB(0,c[0][2]-v->zfPos,v->zfSize);
	c[1][2] = CLAMP_ARB(0,c[1][2]-v->zfPos,v->zfSize);
	c[2][2] = CLAMP_ARB(0,c[2][2]-v->zfPos,v->zfSize);
	c[3][2] = CLAMP_ARB(0,c[3][2]-v->zfPos,v->zfSize);

	glEnable(GL_REGISTER_COMBINERS_NV);
	glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);

	// I am just using these to replace the texture's alpha with a different one
	float alpha[4] = {1,0,0,gluvv.clip.alpha};
	glCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, alpha); //set W
	float greenish[4] = {0,1,0,1};
	glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, greenish); //set W

	//blue is the primary color
	glColor4f(0,0,1,1);

	//now for some painfull swizling
	//extract the red, extract the green
	glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB, 
			GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_TRUE, GL_FALSE);
	GlErr("nv20volren","gcom0.4");
	
	//alpha goes to red + green
	glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_INVERT_NV, GL_RGB);
	glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
			GL_DISCARD_NV, GL_DISCARD_NV, GL_TEXTURE0_ARB, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	GlErr("nv20volren","gcom1.4");

	//add blue now, set the correct alpha
	glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	GlErr("nv20volren","fcom1");
	glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	GlErr("nv20volren","fcom2");
	glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	GlErr("nv20volren","fcom3");
	glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	GlErr("nv20volren","fcom0");
	
	glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

	glEnable(GL_BLEND);

	float offset = .001; //this is used to avoid z-compete when the slice is on top

	glPushMatrix();{
		//make sure we draw this in sub-volume space
		glTranslatef(v->xfPos, v->yfPos, v->zfPos);
		switch(gluvv.clip.oaxis){
		case VolRenAxisXPos:
			if((dv < 0)&&(c[0][0] > 0)&&(c[0][0]<v->xfSize)){
				float ov[3] = {offset, 0, 0}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);
			}
			break;
		case VolRenAxisXNeg:
			if(dv > 0){
				float ov[3] = {-offset, 0, 0}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);
			}
			break;
		case VolRenAxisYPos:
			if((dv > 0)&&(c[0][1] > 0)&&(c[0][1]<v->yfSize)){
				float ov[3] = {0, offset, 0}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);
			}
			break;
		case VolRenAxisYNeg:
			if((dv < 0)&&(c[0][1] > 0)&&(c[0][1]<v->yfSize)){
				float ov[3] = {0, -offset, 0}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);
			}
			break;
		case VolRenAxisZPos:
			if((dv < 0)&&(c[0][2] > 0)&&(c[0][2]<v->zfSize)){
				float ov[3] = {0, 0, offset}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);
			}
			break;
		case VolRenAxisZNeg:
			if((dv > 0)&&(c[0][2] > 0)&&(c[0][2]<v->zfSize)){
				float ov[3] = {0, 0, -offset}; //offset vector
				for(int i=0; i<4; ++ i)
					addV3(c[i],c[i],ov);
				renderSlice(c,vol);			
			}
			break;
		}
	} glPopMatrix();
	glDisable(GL_REGISTER_COMBINERS_NV);  //clean up opengl state
	glActiveTexture(GL_TEXTURE0_ARB); { 
				glDisable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
	}

	glDisable(GL_BLEND);
}

//============================================== Render Slice w/ corners
//======================================================================
void NV20VolRen3D2::renderSlice(float c[4][3], int vol)
{
	glActiveTexture(GL_TEXTURE0_ARB); { //this is for the slice
				glEnable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
				glBindTexture(GL_TEXTURE_3D, texNames[vol]);
	}
	
	Volume *v = &gluvv.mv->volumes[vol];

	glBegin(GL_QUADS);{
		glMultiTexCoord3fARB(GL_TEXTURE0_ARB,c[0][0]/(v->xfSize),c[0][1]/(v->yfSize),c[0][2]/(v->zfSize));
		glVertex3f(c[0][0], c[0][1], c[0][2]);
		glMultiTexCoord3fARB(GL_TEXTURE0_ARB,c[1][0]/(v->xfSize),c[1][1]/(v->yfSize),c[1][2]/(v->zfSize));
		glVertex3f(c[1][0], c[1][1], c[1][2]);
		glMultiTexCoord3fARB(GL_TEXTURE0_ARB,c[2][0]/(v->xfSize),c[2][1]/(v->yfSize),c[2][2]/(v->zfSize));
		glVertex3f(c[2][0], c[2][1], c[2][2]);
		glMultiTexCoord3fARB(GL_TEXTURE0_ARB,c[3][0]/(v->xfSize),c[3][1]/(v->yfSize),c[3][2]/(v->zfSize));
		glVertex3f(c[3][0], c[3][1], c[3][2]);			
	} glEnd();

}

//=========================================================== resetClips
//======================================================================
void NV20VolRen3D2::resetClips()
{
	glDisable(GL_CLIP_PLANE0);
}

//======================================================== Setup Shaders
//======================================================================
void NV20VolRen3D2::setupShaders()
{
	glEnable(GL_TEXTURE_SHADER_NV); {
		GlErr("nv20volren", "enable shader");
		
		glActiveTexture(GL_TEXTURE0_ARB); { //this is for the slice
			glEnable(GL_TEXTURE_3D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
		}
		glActiveTexture(GL_TEXTURE1_ARB); { //this is for the transfer function
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_AR_TEXTURE_2D_NV);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
			glBindTexture(GL_TEXTURE_2D, deptexName);
		}
		glActiveTexture(GL_TEXTURE2_ARB); { // for shading or clipping (not both)
			if((gluvv.shade == gluvvShadeDiff) || (gluvv.shade == gluvvShadeDSpec)){
				//shading
				glEnable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
				//glBindTexture(GL_TEXTURE_3D, shadeNames[0]);
			} else if(gluvv.light.shadow){ //shadows w/no shading
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, shadowName);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
			} else {
				//no shading
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			}
		}
		glActiveTexture(GL_TEXTURE3_ARB); { //3rd & 4th axis, or clipping, or shadows
			if((gluvv.shade == gluvvShadeDiff) || (gluvv.shade == gluvvShadeDSpec)){ //we have to use this for shadows
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, shadowName);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
			} else {                           //use this for 3rd axis
				switch(gluvv.dmode){       //hey! we use this for 3rd & 4th axies
				case GDM_VGH:
				case GDM_V1GH:
				case GDM_V2G:
				case GDM_V2GH:
				case GDM_V3:
				case GDM_V3G:
				case GDM_V4:
					glEnable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_GB_TEXTURE_2D_NV);
					glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
					glBindTexture(GL_TEXTURE_2D, deptex2Name);
					break;
				case GDM_V1:
				case GDM_V1G:
				case GDM_V2:
				case GDM_VGH_VG:
				case GDM_VGH_V:
					if(!gluvv.clip.ortho) break; //we want to take care of this elsewhere
				default:
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_TEXTURE_3D);
					glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
				}
			}
		}
		GlErr("set up shader", "after texture3");
	}
}

void NV20VolRen3D2::resetShaders()
{
		glActiveTexture(GL_TEXTURE3_ARB); { //not used
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
		}
		glActiveTexture(GL_TEXTURE2_ARB); { //not used
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
		}
		glActiveTexture(GL_TEXTURE1_ARB); { //not used
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
		}
		glActiveTexture(GL_TEXTURE0_ARB); { //not used
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
	glDisable(GL_TEXTURE_SHADER_NV); 

}

//======================================================= Setup Reg Comb
//======================================================================

void NV20VolRen3D2::setupRegComb(float ftb)
{
	if((gluvv.shade == gluvvShadeDiff) || (gluvv.shade == gluvvShadeDSpec)){
		float vdir[3];
		subV3(vdir, gluvv.env.eye, gluvv.env.at);
		normalizeV3(vdir);
		float ltdir[3];
		subV3(ltdir, gluvv.light.pos, gluvv.env.at);
		normalizeV3(ltdir);
		float ltoe[3];
		subV3(ltoe,vdir,ltdir);
		scaleV3(.5, ltoe);
		float half[4];
		addV3(half,ltdir, ltoe);
		half[3] = 1;
		float vhalf[4];
		float invx[16];
		inverseMatrix(invx, gluvv.rinfo.xform);
		translateV3(vhalf,invx,half);
		negateV3(vhalf);
		normalizeV3(vhalf);
		vhalf[0] = vhalf[0]/2 + .5;
		vhalf[1] = vhalf[1]/2 + .5;
		vhalf[2] = vhalf[2]/2 + .5;
		glFogfv(GL_FOG_COLOR, vhalf);
		
		float lpos[4];
		translateV3W(lpos, invx, ltdir); 
		negateV3(lpos);
		normalizeV3(ltdir);
		lpos[0] = lpos[0]/2 + .5;
		lpos[1] = lpos[1]/2 + .5;
		lpos[2] = lpos[2]/2 + .5;
		lpos[3] = 1 - gluvv.light.amb;
		glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, lpos); //set light direction
		
		glEnable(GL_REGISTER_COMBINERS_NV);
		
		glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 8);
	
		//diffuse dot product (2 sided)
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE2_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR1_NV, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_EXPAND_NEGATE_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB, 
			GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_TRUE, GL_FALSE);
		GlErr("nv20volren","gcom0.4");

		//compute diffuse contribution (sum of diff-dot times color)
		glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
			GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom1.4");

		//add diffuse and ambient (add in ambient and square the diffuse)
		glCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_C_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		glCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER2_NV, GL_RGB, 
			GL_DISCARD_NV, GL_DISCARD_NV, GL_TEXTURE1_ARB, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom2.4");

		//specular dod product (2 sided)
		glCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE2_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_B_NV, GL_FOG, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_EXPAND_NEGATE_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_D_NV, GL_FOG, GL_EXPAND_NORMAL_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER3_NV, GL_RGB, 
			GL_SPARE1_NV, GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_TRUE, GL_FALSE);
		GlErr("nv20volren","gcom3.4");

		//specular 2nd power & sum to one value
		glCombinerInputNV(GL_COMBINER4_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER4_NV, GL_RGB, GL_VARIABLE_B_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER4_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER4_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER4_NV, GL_RGB, 
			GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom4.4");

		//shadow weight, specular 4th power
		glCombinerInputNV(GL_COMBINER5_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
		glCombinerInputNV(GL_COMBINER5_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
		glCombinerInputNV(GL_COMBINER5_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER5_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER5_NV, GL_RGB, 
			GL_TEXTURE3_ARB, GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom5.4");

		//shadow diffuse & specular 8th power
		glCombinerInputNV(GL_COMBINER6_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		if(gluvv.light.shadow)
			glCombinerInputNV(GL_COMBINER6_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		else
			glCombinerInputNV(GL_COMBINER6_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		glCombinerInputNV(GL_COMBINER6_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER6_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER6_NV, GL_RGB, 
			GL_TEXTURE1_ARB, GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom6.4");

		//specular shadow add to diffuse
		glCombinerInputNV(GL_COMBINER7_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		if(gluvv.light.shadow)
			glCombinerInputNV(GL_COMBINER7_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		else
			glCombinerInputNV(GL_COMBINER7_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER7_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER7_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER7_NV, GL_RGB, 
			GL_DISCARD_NV, GL_TEXTURE2_ARB, GL_TEXTURE1_ARB, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		GlErr("nv20volren","gcom7.4");

		//final combiner
		glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		if(ftb <= 0)
			glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
		else 
			glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
		glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		//glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);	
		glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
		GlErr("nv20volren","fcom.0");
	}
	else{ //no shading =================================================

		float amb[4] = {1,1,1, 1- gluvv.light.amb}; //ambiant shadow factor
		
		switch(gluvv.dmode){ //third axis stuff
		case GDM_VGH:
		case GDM_V1GH:
		case GDM_V2G:
		case GDM_V2GH:
		case GDM_V3:
		case GDM_V3G:
		case GDM_V4:
			glEnable(GL_REGISTER_COMBINERS_NV);

			glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, amb); //set ambiant contribution

			glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);
			
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, 
				GL_TEXTURE1_ARB, GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			GlErr("nv20volren","gcom7.4");
			
			if(gluvv.light.shadow){
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
					GL_TEXTURE1_ARB, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
				GlErr("nv20volren","gcom7.4");
			} else {
				glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
					GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
			
			glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			//glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			if(ftb <= 0)
				glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
			else
				glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);	
			glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			GlErr("nv20volren","fcom.0");
		  break;
		default:
			glEnable(GL_REGISTER_COMBINERS_NV);

			glCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV, amb); //set ambiant contribution

			glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);
			
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, 
				GL_TEXTURE2_ARB, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			GlErr("nv20volren","gcom7.4");
			
			if(gluvv.light.shadow){
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
				glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
					GL_TEXTURE1_ARB, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
				GlErr("nv20volren","gcom7.4");
			} else {
				glCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, 
					GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			}

			glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			if(ftb <= 0)
				glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
			else
				glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);	
			glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			GlErr("nv20volren","fcom.0");
		  break;
		}
	}

}


void NV20VolRen3D2::resetRegComb()
{
	glDisable(GL_REGISTER_COMBINERS_NV);
}

//====================================================== Set Up P-Buffer
//======================================================================
void NV20VolRen3D2::setupPBuff()       //prepare pbuffer for rendering
{
	if(gluvv.picking) return;
	
	pbuff->MakeCurrent();

	GlErr("volren", "setupPbub()-start");
	//compute the view-port for this projection
	float winsz[3] = {.8, .8, .5};
	float winszp[3];
	translateV3WD(winszp, gluvv.light.pj,winsz);
	xbias = winszp[0];
	ybias = winszp[1];
	xscale = gluvv.light.buffsz[0]/(winszp[0]*2);
	yscale = gluvv.light.buffsz[1]/(winszp[1]*2);

#if 0
	glMatrixMode( GL_PROJECTION );{  //figure out the window size
			glLoadIdentity();
			glOrtho(0,gluvv.light.buffsz[0],0,gluvv.light.buffsz[1],1,3);
	}
	glMatrixMode(GL_MODELVIEW);{
			glLoadIdentity();
			gluLookAt(0,0,2,
								0,0,0,
								0,1,0);
	}
#endif

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT);

	

	//setup shader stage
	glEnable(GL_TEXTURE_SHADER_NV); {
		GlErr("nv20volren", "enable shader");
		
		glActiveTexture(GL_TEXTURE0_ARB); { //this is for the slice
			glEnable(GL_TEXTURE_3D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
		} //end texture 2
		glActiveTexture(GL_TEXTURE1_ARB); { //this is for the transfer function
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_AR_TEXTURE_2D_NV);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
			glBindTexture(GL_TEXTURE_2D, deptexName);
		} //end texture 1
		glActiveTexture(GL_TEXTURE2_ARB); { // shadow texture
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, shadowName);
		} //end texture 2
		glActiveTexture(GL_TEXTURE3_ARB); { //3rd & 4th axis, or clipping
			switch(gluvv.dmode){       //hey! we use this for 3rd & 4th axies
			case GDM_VGH:
			case GDM_V1GH:
			case GDM_V2G:
			case GDM_V2GH:
			case GDM_V3:
			case GDM_V3G:
			case GDM_V4:
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_GB_TEXTURE_2D_NV);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
				glBindTexture(GL_TEXTURE_2D, deptex2Name);
				break;
			case GDM_V1:
			case GDM_V1G:
			case GDM_V2:
			case GDM_VGH_VG:
			case GDM_VGH_V:
				if(!gluvv.clip.ortho) break; //we want to take care of this elsewhere
			default:
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_TEXTURE_3D);
				glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			}
		} //end texture 3
		GlErr("pbuffer::set up shader", "after texture3");
	}
	
	//now setup the register combiners
	switch(gluvv.dmode){ //third axis stuff
		case GDM_VGH:
		case GDM_V1GH:
		case GDM_V2G:
		case GDM_V2GH:
		case GDM_V3:
		case GDM_V3G:
		case GDM_V4:
			glEnable(GL_REGISTER_COMBINERS_NV);
			
			glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);
			//compute alpha with 3rd axis
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, 
				GL_TEXTURE1_ARB, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			GlErr("nv20volren","gcom7.4");
			//composite new slice under old slice
#if 0
			glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
			glCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA, 
				GL_DISCARD_NV, GL_DISCARD_NV, GL_TEXTURE1_ARB, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			GlErr("nv20volren","gcom7.4");
			//what is in the frame buffer? (always front to back)
#endif

			glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		  glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
		  //glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);	
			glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			//glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
			GlErr("nv20volren","fcom.0");
			break;
		default:
			glEnable(GL_REGISTER_COMBINERS_NV);
			glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);
			//NO OP
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
			glCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, 
				GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
			GlErr("nv20volren","gcom7.4");
			//to the frame buffer!!! (always front to back)
			glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
			glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);	
			glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
			GlErr("nv20volren","fcom.0");
			break;
	}

	GlErr("volren", "end of setup pbuff");
	MakeCurrent();

}

//=================================================== Render 3D Vec Algn
//======================================================================

void NV20VolRen3D2::render3DVA(float sampleFrequency,
		GLdouble mv[16],
		unsigned int v,
		float vo[8][3],	  //volume vertex coords model-space coords
		float tx[8][3],	  //texture vertex coords tex-space coords
		float axis[4])	 //axis to slice along world-space coords
{

   float rv[8][3];     //the rotated volume (may include a scale)
	 float lv[8][4];
   float maxval = -100; //(tmp)
   float minval = 100;
   int minvert = 0;
   GLdouble mvinv[16];

   inverseMatrix(mvinv, mv); //invert model view matrix

	 float start[3];       //pick a reference point for rendering
	 copyV3(start, axis);  // this is some distance out the rendering axis
	 negateV3(start);
	 scaleV3(100, start);

	 //build the light transform
	 float tm0[16];
	 float tb[16] = {1,0,0,0,         //translate back by a half
		               0,1,0,0,
									 0,0,1,0,
									 gluvv.mv->volumes[v].xfPos-gluvv.mv->xfSize/2, 
									 gluvv.mv->volumes[v].yfPos-gluvv.mv->yfSize/2, 
									 gluvv.mv->volumes[v].zfPos-gluvv.mv->zfSize/2, 1};
	 matrixMult(tm0,gluvv.rinfo.xform,tb);
	 float ltxf[16];
	 matrixMult(ltxf,gluvv.light.xf,tm0);

   int vnear = 0;
	 int vfar  = 0;

   for(int i=0; i<8; ++i){   //translate model to world coords (view space)
		 translateV3(rv[i], mv, vo[i]);
		 float tv[3];
		 subV3(tv, start, rv[i]);       //compute vertex distance from reference
		 float tvd = normV3(tv);
#if 0
		 translateV4_3W(lv[i], ltxf, vo[i]);
		 cerr << " " << i << " "; printV4(lv[i]); cerr << endl;
		 lv[i][0] /= lv[i][3];
		 lv[i][1] /= lv[i][3];
		 lv[i][2] /= lv[i][3];
		 lv[i][0] = (lv[i][0] + xbias)*xscale;
		 lv[i][1] = (lv[i][1] + ybias)*yscale;
#endif
		 if(maxval < tvd){
			 maxval = tvd;
			 vfar = i;   //end point for slicing
		 }
		 if(minval > tvd){
			 minval = tvd;
			 vnear = i;  //starting point for slicing
		 }
   }

#if 0
	 glPushMatrix();{
		 	glMatrixMode( GL_PROJECTION );
			glPushMatrix();
				glLoadIdentity();
				float winsz[3] = {.7, .7, .5};
				float winszp[3];
				translateV3WD(winszp, gluvv.light.pj,winsz);
				//cerr << "winsz "; printV3(winszp); cerr << endl;
				glOrtho(0,gluvv.light.buffsz[0],0,gluvv.light.buffsz[1],1,3);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			gluLookAt(0,0,2,
				        0,0,0,
								0,1,0);
			glColor4f(1,0,0,1);
			glBegin(GL_LINES);{
				glVertex2fv(lv[0]);
				glVertex2fv(lv[1]);

				glVertex2fv(lv[2]);
				glVertex2fv(lv[3]);

				glVertex2fv(lv[4]);
				glVertex2fv(lv[5]);

				glVertex2fv(lv[6]);
				glVertex2fv(lv[7]);
			} glEnd();
			
	 }glPopMatrix();
	 glMatrixMode(GL_PROJECTION);
	 glPopMatrix();
	 glMatrixMode(GL_MODELVIEW);
#endif

	 //find the slice plane normal (sn)
	 float vpn[3];
	 vpn[0] = axis[0]; vpn[1] = axis[1]; vpn[2] = axis[2];
   float sn[3];		    //slice plane normal
   translateV3(sn, mvinv, vpn); //move vpn to sn (model space);
	 normalizeV3(sn);

   //note: sn & sp are defined in Model Space, ie the space where the volume
   // is alligned with the (x,y,z) axies

	 //distance between samples
   float dis = gluvv.mv->xfSize / (gluvv.mv->xiSize * sampleFrequency); 
	
	 cscaleV3(start, 100, sn);  //pick a point far from the volume
	 negateV3(start);

	 //find the nearest slicing point (the closest sample)
	 float np[3];// = {vo[minvert][0], vo[minvert][1], vo[minvert][2]}; 
	 float vms[3];   //v minus start (this is the vector from the point to the near vertex)
	 subV3(vms, vo[vnear], start);  
	 float vmsps[3];  //v minus start project on sn
	 cscaleV3(vmsps, dotV3(vms, sn), sn);
	 int sts = (int)(normV3(vmsps)/dis) + 1; //samples to start
	 cscaleV3(np,sts*dis,sn); //the nearest slicing point
	 addV3(np,np,start);

	 //find the farthest slicing point (same process as near point)
	 float fp[3];
	 subV3(vms, vo[vfar], start);
	 cscaleV3(vmsps, dotV3(vms, sn), sn);
	 sts = (int)(normV3(vmsps)/dis);
	 cscaleV3(fp,sts*dis,sn);
	 addV3(fp,fp,start);

	 float distv[3];            //distance to sample
	 subV3(distv, np, fp);      //needs work (fix so that off angle still has the correct #of slices)
	 float dist = normV3(distv);
	 float del[3];// = {sn[0]*dis, sn[1]*dis, sn[2]*dis};
	 subV3(distv, vo[vfar], vo[vnear]);
	 //normalizeV3(distv);   //now figure out how to move the slice point

   int samples = (int)((dist) / dis);//(total distance to be sampled)/(sample spacing)
	 cscaleV3(del, 1/(float)samples, distv);


	 //set up opengl
   glDisable(GL_LIGHTING); //light makes it look bad!

   glDisable(GL_CULL_FACE);
   glPolygonMode(GL_FRONT, GL_FILL);
   glPolygonMode(GL_BACK, GL_FILL);

   //glEnable(GL_TEXTURE_3D_EXT);
	 
	 glActiveTexture(GL_TEXTURE2_ARB);
   glBindTexture(GL_TEXTURE_3D, shadeNames[v]);

	 if(gluvv.light.shadow){
		 if(!gluvv.picking){
			pbuff->MakeCurrent();{
				glActiveTexture(GL_TEXTURE0_ARB);
				glBindTexture(GL_TEXTURE_3D, texNames[v]);			 
			} MakeCurrent();
			GlErr("volren", "setting data volume");
		 }
	 }

	 glActiveTexture(GL_TEXTURE0_ARB);
   glBindTexture(GL_TEXTURE_3D, texNames[v]);

	 glEnable(GL_BLEND);
	 if(axis[3] <= 0){
		 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	 } else {
		 glEnable(GL_BLEND);
		 glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
		 glEnable(GL_BLEND);
		 glDisable(GL_DEPTH_TEST);
	 }

   glColor4f(1, 1, 1, 1);
   GlErr("NV20VolRen3D2","drawVA");


   float poly[6][3];   // for edge intersections
   float tcoord[6][3]; // for texture intersections
   float tpoly[6][3];  // for transformed edge intersections
   int edges;	       // total number of edge intersections

	 float sp[3]; //starting point for slicing (aka slice point)
	 copyV3(sp,np);

   //highly un-optimized!!!!!!!!!
   for(i = 0 ; i < samples; ++i){ //for each slice
      //increment the slice plane point by the slice distance
      sp[0] += del[0];
      sp[1] += del[1];
      sp[2] += del[2];

      edges = 0; 
      //now check each edge of the volume for intersection with.. 
      //the plane defined by sp & sn
      //front bottom edge
      edges += intersect(vo[0], vo[1], tx[0], tx[1], rv[0], rv[1], sp, sn, 
			 poly[edges], tcoord[edges], tpoly[edges]);
      //front left edge
      edges += intersect(vo[0], vo[2], tx[0], tx[2], rv[0], rv[2], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //front right edge
      edges += intersect(vo[1], vo[3], tx[1], tx[3], rv[1], rv[3], sp, sn, 
			 poly[edges], tcoord[edges], tpoly[edges]);
      //left bottom edge
      edges += intersect(vo[4], vo[0], tx[4], tx[0], rv[4], rv[0], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //right bottom edge
      edges += intersect(vo[1], vo[5], tx[1], tx[5], rv[1], rv[5], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //front top edge
      edges += intersect(vo[2], vo[3], tx[2], tx[3], rv[2], rv[3], sp, sn, 
			 poly[edges], tcoord[edges], tpoly[edges]);
      //back bottom edge
      edges += intersect(vo[4], vo[5], tx[4], tx[5], rv[4], rv[5], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //back left edge
      edges += intersect(vo[4], vo[6], tx[4], tx[6], rv[4], rv[6], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //back right edge
      edges += intersect(vo[5], vo[7], tx[5], tx[7], rv[5], rv[7], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //back top edge
      edges += intersect(vo[6], vo[7], tx[6], tx[7], rv[6], rv[7], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //left top edge
      edges += intersect(vo[2], vo[6], tx[2], tx[6], rv[2], rv[6], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);
      //right top edge
      edges += intersect(vo[3], vo[7], tx[3], tx[7], rv[3], rv[7], sp, sn,
			 poly[edges], tcoord[edges], tpoly[edges]);

#if 0
      if(edges == 3){ //no sort required for triangles
				//cerr << "doing a triangle" <<endl;
				glBegin(GL_TRIANGLES);
				{
					glTexCoord3fv(tcoord[0]);
					glVertex3fv(poly[0]);
					glTexCoord3fv(tcoord[1]);
					glVertex3fv(poly[1]);
					glTexCoord3fv(tcoord[2]);
					glVertex3fv(poly[2]);
				}
				glEnd();
      }
      else { //compute convex hull and sort, a little piece of magic from:
#endif
				// B.M.E. Moret & H.D. Shapiro "P to NP" pp. 453
				
				float dx, dy, tt ,theta, cen[2];  //tt= TempTheta
				cen[0] = cen[1] = 0.0;
				int next;
				//rather than swap 3 arrays, only one?
				int order[6] ={0,1,2,3,4,5};
				
				// order[6] could be an extreemly inefficient way to do this
				for( int j=0; j<edges; ++j){ //find the center of the points
					cen[0] += tpoly[j][0];
					cen[1] += tpoly[j][1];
				} //by averaging	
				cen[0]/= edges;
				cen[1]/= edges;	 
				
				for(j=0; j<edges; ++j){ //for each vertex
					theta = -10;	       //find one with largest angle from center.. 
					next = j;
					for ( int k= j; k<edges; ++k){ 
						//... and check angle made between other edges
						dx = tpoly[order[k]][0] - cen[0];
						dy = tpoly[order[k]][1] - cen[1];
						if( (dx == 0) && (dy == 0)){ //same as center? 
							next = k;
							cout << "what teh " << endl;
							break; //out of this for-loop
						}
						tt = dy/(ABS(dx) + ABS(dy)); //else compute theta [0-4]
						if( dx < 0.0 ) tt = (float)(2.0 - tt); //check quadrants 2&3
						else if( dy < 0.0 ) tt = (float)(4.0 + tt); //quadrant 4
						if( theta <= tt ){  //grab the max theta
							next = k;
							theta = tt;
						}
					} //end for(k) angle checking
					// i am using 'tt' as a temp
					// swap polygon vertex ( is this better than another branch?)
					// I am not sure wich is worse: swapping 3 vectors for every edge
					// or: using an array to index into another array??? hmmm....
					//   should have payed more attention in class
					int tmp = order[j];
					order[j] = order[next];
					order[next] = tmp;
					
				} //end for(j) edge /angle sort
#if 0
				glBegin(GL_POLYGON); //draw slice and texture map it
				{
					for(int j=0; j< edges; ++j){
						glTexCoord3fv(tcoord[order[j]]);
						glVertex3fv(poly[order[j]]);
					}
				}
				glEnd();
#else
				//volSlice(edges, tcoord, poly, order);
				volShadow(edges,tcoord,poly,order,ltxf,texNames[v]);
#endif
				//glFlush();
      //}//end else compute convex hull
   }// end for(i) each slice
	 
   glDisable(GL_BLEND);
   glDisable(GL_TEXTURE_3D_EXT);
   glEnable(GL_LIGHTING);
	 glEnable(GL_DEPTH_TEST);
}

//============================================================ Vol Slice
//======================================================================

//standard renderer
inline void NV20VolRen3D2::volSlice(int edges, 
											             float tc[6][3], 
											             float pc[6][3],
											             int   order[6])
{
	glBegin(GL_POLYGON); //draw slice and texture map it
	{
		for(int j=0; j< edges; ++j){
			glMultiTexCoord3fvARB(GL_TEXTURE0_ARB,tc[order[j]]);
			glMultiTexCoord3fvARB(GL_TEXTURE2_ARB,tc[order[j]]);
			//glTexCoord3fv(tc[order[j]]);
			glVertex3fv(pc[order[j]]);
		}
	}
	glEnd();
}

//render with shadows
inline void NV20VolRen3D2::volShadow(int edges,
																		 float tc[6][3],
																		 float pc[6][3],
																		 int   order[6],
																		 float ltxf[16],
																		 int   vol)
{

	float minx = gluvv.light.buffsz[0];
	float maxx = 0;
	float miny = gluvv.light.buffsz[1];
	float maxy = 0;
	
	float lv[6][4];  //light space verticies
	float lc[6][2];  //light buffer texture coords
	for(int i=0; i<edges; ++i){
		//move the coords into light space
		//first the vertices
		int idx = order[i];
		translateV4_3W(lv[idx], ltxf, pc[idx]);
		lv[idx][0] /= lv[idx][3];
		lv[idx][1] /= lv[idx][3];
		//now the texture coords in screen space
		lc[idx][0] = (lv[idx][0] + xbias)/(2*xbias);
		lc[idx][1] = (lv[idx][1] + ybias)/(2*ybias);
		//get the light buffer coords (screen space) 
		lv[idx][0] = lc[idx][0]*gluvv.light.buffsz[0];
		lv[idx][1] = lc[idx][1]*gluvv.light.buffsz[1];
		//now figure out the rectangular region to copy
		maxx = MAX(maxx, lv[idx][0]);
		minx = MIN(minx, lv[idx][0]);
		maxy = MAX(maxy, lv[idx][1]);
		miny = MIN(miny, lv[idx][1]);
	}
	//copy the current alpha from the pbuffer, and draw the latest
	pbuff->MakeCurrent();{
		//use render to texture here??? (probably not)
		glDisable(GL_TEXTURE_SHADER_NV);
		glDisable(GL_REGISTER_COMBINERS_NV);
		
		glActiveTexture(GL_TEXTURE0_ARB);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, shadowName);
		//glBindTexture(GL_TEXTURE_2D, gluvv.volren.deptexName);
		//minx = MAX(minx-20, 0);
		//miny = MAX(miny-20, 0);
		//maxx = MIN(maxx+20, gluvv.light.buffsz[0]);
		//maxy = MIN(maxy+20, gluvv.light.buffsz[1]);
		maxx = MAX(maxx, minx);
		maxy = MAX(maxy, miny);
		minx = MIN(maxx, minx);
		miny = MIN(maxy, miny);
		//cerr << "min " << minx << ", " << miny << "   max " << maxx << ", " << maxy << endl;
		//cerr << " size " << (int)(maxx-minx) << ", " << (int)(maxy-miny) << endl;
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, (int)minx, (int)miny, 
			                  (int)minx, (int)miny,
		                    (int)(maxx-minx), (int)(maxy-miny));
		//glRasterPos2i(minx, miny);
		//glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		//                    (int)(maxx-minx), (int)(maxy-miny));

		//glClear(GL_COLOR_BUFFER_BIT);
#if 0
		glBegin(GL_QUADS);{
			glVertex3f(0,0,1);
		} glEnd():
#endif

		//glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		//                    512, 512);
		GlErr("volren","after pbuff- copy tex sub");

		glEnable(GL_TEXTURE_SHADER_NV);
		glEnable(GL_REGISTER_COMBINERS_NV);
		glActiveTexture(GL_TEXTURE0_ARB);
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, vol);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
		
		glFlush();
		
	  //glDisable(GL_TEXTURE_3D);

		glColor4f(1,1,1,1);
		glBegin(GL_POLYGON); //draw slice and texture map it
		{
			for(int j=0; j< edges; ++j){
				//the incomming slice
				glMultiTexCoord3fvARB(GL_TEXTURE0_ARB,tc[order[j]]);
				//the slice from the shadow buffer
				glMultiTexCoord2fvARB(GL_TEXTURE2_ARB,lc[order[j]]);
				//the coords in screen space
				glVertex2fv(lv[order[j]]);
			}
		}
		glEnd();

	} MakeCurrent();

	//now draw to the frame buffer
	glBegin(GL_POLYGON); //draw slice and texture map it
	{
		for(int j=0; j< edges; ++j){
			glMultiTexCoord3fvARB(GL_TEXTURE0_ARB,tc[order[j]]);
			//glMultiTexCoord3fvARB(GL_TEXTURE2_ARB,tc[order[j]]);
			if((gluvv.shade == gluvvShadeDiff) || (gluvv.shade == gluvvShadeDSpec)){ //we have to use this for shadows
				glMultiTexCoord3fvARB(GL_TEXTURE3_ARB,lc[order[j]]);
				glMultiTexCoord3fvARB(GL_TEXTURE2_ARB,tc[order[j]]);
			} else {
				glMultiTexCoord3fvARB(GL_TEXTURE2_ARB,lc[order[j]]);
			}
			glVertex3fv(pc[order[j]]);
		}
	}
	glEnd();

	GlErr("volren","end of shadow slice");

}

//============================================================ INTERSECT
//======================================================================

inline int NV20VolRen3D2::intersect(
		     const float p0[3], const float p1[3], //line end points
		     const float t0[3], const float t1[3], //texture points
		     const float v0[3], const float v1[3], //view coord points
		     const float sp[3], const float sn[3], //plane point & norm
		     float pnew[3], float tnew[3], float vnew[3]) //new values 
{
   //t = (sn.(sp - p0))/(sn.(p1 - p0))
   float t = ((sn[0]*(sp[0] - p0[0]) + sn[1]*(sp[1] - p0[1]) 
	       + sn[2]*(sp[2] - p0[2])) / 
	      (sn[0]*(p1[0] - p0[0]) + sn[1]*(p1[1] - p0[1])
	       + sn[2]*(p1[2] - p0[2])));
   //note if the denominator is zero t is a NAN so we should have no problems?
   
   if( (t>=0) && (t<=1) ){
      //compute line intersection
      pnew[0] = p0[0] + t*(p1[0] - p0[0]); 
      pnew[1] = p0[1] + t*(p1[1] - p0[1]); 
      pnew[2] = p0[2] + t*(p1[2] - p0[2]);
      //compute texture interseciton
      tnew[0] = t0[0] + t*(t1[0] - t0[0]); 
      tnew[1] = t0[1] + t*(t1[1] - t0[1]); 
      tnew[2] = t0[2] + t*(t1[2] - t0[2]);
      //compute view coordinate intersections
      vnew[0] = v0[0] + t*(v1[0] - v0[0]); 
      vnew[1] = v0[1] + t*(v1[1] - v0[1]); 
      vnew[2] = v0[2] + t*(v1[2] - v0[2]);
      return 1;
   }
   return 0;
} 

//========================================================= Render Slice
//======================================================================

void NV20VolRen3D2::renderSlice()
{
	glPushMatrix();{
		glMultMatrixf(gluvv.rinfo.xform);
		glDisable(GL_LIGHTING);

		glActiveTexture(GL_TEXTURE0_ARB); {
			glEnable(GL_TEXTURE_3D);
			//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			//glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, texNames[0]);
		}
		
		glColor4f(1,1,1,1);
		glBegin(GL_QUADS);{
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,0,0,.5);
			//glTexCoord3f(0.0,0.0,.5);
			glVertex3f(0,0,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,1,0,.5);
			//glTexCoord3f(1.0,0.0,.5);
			glVertex3f(1,0,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,1,1,.5);
			//glTexCoord3f(1.0,1.0,.5);
			glVertex3f(1,1,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,0,1,.5);
			//glTexCoord3f(0.0,1.0,.5);
			glVertex3f(0,1,.5);
		} glEnd();
		
		glActiveTexture(GL_TEXTURE0_ARB); {
			glDisable(GL_TEXTURE_3D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
		}

		glEnable(GL_TEXTURE_SHADER_NV);
		glActiveTexture(GL_TEXTURE0_ARB); {
			glEnable(GL_TEXTURE_3D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, texNames[0]);
		}
		glActiveTexture(GL_TEXTURE1_ARB); {
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_GB_TEXTURE_2D_NV);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
			glBindTexture(GL_TEXTURE_2D, deptexName);
		}
		
		glColor4f(1,1,1,1);
		glBegin(GL_QUADS);{
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,0,0,.5);
			glVertex3f(0,0,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,1,0,.5);
			glVertex3f(-1,0,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,1,1,.5);
			glVertex3f(-1,1,.5);
			
			glMultiTexCoord3fARB(GL_TEXTURE0_ARB,0,1,.5);
			glVertex3f(0,1,.5);
		} glEnd();

		glActiveTexture(GL_TEXTURE1_ARB); {
			glDisable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
		}

		glActiveTexture(GL_TEXTURE0_ARB); {
			glDisable(GL_TEXTURE_3D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE);
			glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_NONE);
		}

		glDisable(GL_TEXTURE_SHADER_NV);

		

		glEnable(GL_LIGHTING);

	} glPopMatrix();

	GlErr("NV20VolRen3D2", "Draw Slice");	

}

//======================================================= Create Volumes
//======================================================================

void NV20VolRen3D2::createVolumes()
{
	if(!glTexImage3DEXT){
		go = 0;
		return;
	}

	if(gluvv.mv){
			createBricks();
	} else {
		go = 0;
	}
}


//======================================================= Create Bricks
//======================================================================
void NV20VolRen3D2::createBricks()
{
	texNames = new unsigned int[gluvv.mv->numSubVols];
	shadeNames = new unsigned int[gluvv.mv->numSubVols];
	glGenTextures(gluvv.mv->numSubVols, texNames);
	glGenTextures(gluvv.mv->numSubVols, shadeNames);

	int ne = gluvv.mv->nelts;

	for(int v=0; v<gluvv.mv->numSubVols; ++v){
		int sx = gluvv.mv->volumes[v].xiSize;
		int sy = gluvv.mv->volumes[v].yiSize;
		int sz = gluvv.mv->volumes[v].ziSize;

		unsigned char *d = gluvv.mv->volumes[v].currentData;
		unsigned char *tex3D;
		int i, j, k;
	

		switch(gluvv.dmode){
		case GDM_V1: //one byte texture
		case GDM_VGH_V:
			//this is where the texture data goes
			cerr << "Creating 1 byte volume ";
			tex3D = new unsigned char[sx*sy*sz];
			for(i = 0; i < sz; ++i){
				for(j = 0; j < sy; ++j){
					for(k = 0; k < sx; ++ k){
						tex3D[i*sx*sy + j*sx + k] = d[i*sx*sy*ne + j*sx*ne + k*ne + 0];
					}
				}
			}
			loadTex1B(tex3D, texNames[v], sx, sy, sz);	
			cerr << " -done" << endl;
			break;
		case GDM_V1G: //two byte texture
		case GDM_V2:
		case GDM_VGH_VG:
			cerr << "Creating 2 byte volume ";
			tex3D = new unsigned char[sx*sy*sz*2];
			for(i = 0; i < sz; ++i){
				for(j = 0; j < sy; ++j){
					for(k = 0; k < sx; ++ k){
						tex3D[i*sx*sy*2 + j*sx*2 + k*2 + 0] = d[i*sx*sy*ne + j*sx*ne + k*ne + 1];
						tex3D[i*sx*sy*2 + j*sx*2 + k*2 + 1] = d[i*sx*sy*ne + j*sx*ne + k*ne + 0];
					}
				}
			}
			loadTex2B(tex3D, texNames[v], sx, sy, sz);
			delete[] tex3D;
			cerr << " -done" << endl;
			break;
		case GDM_V1GH: //3 or 4 byte data == 4 byte texture
		case GDM_V2G:
		case GDM_V2GH:
		case GDM_V3:
		case GDM_V3G:
		case GDM_V4:
		case GDM_VGH:
			cerr << "Creating 4 byte volume ";
			tex3D = new unsigned char[sx*sy*sz*4];
			for(i = 0; i < sz; ++i){
				for(j = 0; j < sy; ++j){
					for(k = 0; k < sx; ++ k){
						tex3D[i*sx*sy*4 + j*sx*4 + k*4 + 0] = d[i*sx*sy*ne + j*sx*ne + k*ne + 1];
						tex3D[i*sx*sy*4 + j*sx*4 + k*4 + 1] = d[i*sx*sy*ne + j*sx*ne + k*ne + 2];
						if(ne > 3)
							tex3D[i*sx*sy*4 + j*sx*4 + k*4 + 2] = d[i*sx*sy*ne + j*sx*ne + k*ne + 3];
						else
							tex3D[i*sx*sy*4 + j*sx*4 + k*4 + 2] = 0;
						tex3D[i*sx*sy*4 + j*sx*4 + k*4 + 3] = d[i*sx*sy*ne + j*sx*ne + k*ne + 0];
					}
				}
			}
			loadTex4B(tex3D, texNames[v], sx, sy, sz);
			delete[] tex3D;
			cerr << " -done" << endl;
			break;
		default:
			cerr << "Error NV20VolRen3D2::createBricks(), data type unknown" << endl;
			return;
		}

		if(gluvv.mv->volumes[v].currentGrad){
			cerr << " Down Loading Shade Texture " << v << " ... ";
			glBindTexture(GL_TEXTURE_3D, shadeNames[v]);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			
			glTexImage3DEXT(GL_TEXTURE_3D,
				0,
				GL_RGB8,
				sx,
				sy,
				sz,
				0,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				gluvv.mv->volumes[v].currentGrad);
		}

		glFlush();
		GlErr("NV20VolRen3D2", "Create Texture - Shade Texture");
		cerr << "Done" << endl;
	


	}//for each sub-volume

	glDisable(GL_TEXTURE_3D);
}

//=================================================== load 1 byte volume
//======================================================================
void NV20VolRen3D2::loadTex1B(unsigned char *tex, unsigned int name, int sx, int sy, int sz)
{
		cerr << " Down Loading Data Texture " << name << " ... ";
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, name);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		glTexImage3DEXT(GL_TEXTURE_3D,
			0,
			GL_ALPHA8,
			sx,
			sy,
			sz,
			0,
			GL_ALPHA,
			GL_UNSIGNED_BYTE,
			tex);
		
		glFlush();
		GlErr("NV20VolRen3D2", "Create Texture - Data Texture");	
		cerr << "Done" << endl;	
}

//=================================================== load 2 byte volume
//======================================================================
void NV20VolRen3D2::loadTex2B(unsigned char *tex, unsigned int name, int sx, int sy, int sz)
{
		cerr << " Down Loading Data Texture " << name << " ... ";
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, name);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		glTexImage3DEXT(GL_TEXTURE_3D,
			0,
			GL_LUMINANCE8_ALPHA8,
			sx,
			sy,
			sz,
			0,
			GL_LUMINANCE_ALPHA,
			GL_UNSIGNED_BYTE,
			tex);
		
		glFlush();
		GlErr("NV20VolRen3D2", "Create Texture - Data Texture");	
		cerr << "Done" << endl;	
}

//=================================================== load 4 byte volume
//======================================================================
void NV20VolRen3D2::loadTex4B(unsigned char *tex, unsigned int name, int sx, int sy, int sz)
{
		cerr << " Down Loading Data Texture " << name << " ... ";
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, name);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		glTexImage3DEXT(GL_TEXTURE_3D,
			0,
			GL_RGBA8,
			sx,
			sy,
			sz,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			tex);
		
		glFlush();
		GlErr("NV20VolRen3D2", "Create Texture - Data Texture");	
		cerr << "Done" << endl;	
}



//==================================================== Create 2D Dep Tex
//======================================================================

void NV20VolRen3D2::create2DDepTex()
{
	//------------- load gb deptex --------------------
	glGenTextures(1, &deptexName);   //the dep tex that we use for the tf
	glGenTextures(1, &gluvv.volren.deptexName);  //the dep tex that the tf widget sees

	int dsx = gluvv.tf.ptexsz[0];
	int dsy = gluvv.tf.ptexsz[1];

	deptex = new unsigned char[dsx*dsy*4]; //reference texture

	gluvv.volren.deptex = deptex;

	for(int j=0; j<dsy; ++j){
		for(int k=0; k<dsx; ++k){
			deptex[j*dsx*4 + k*4 + 0] =   k/(float)dsx*255;							
			deptex[j*dsx*4 + k*4 + 1] = 	j/(float)dsy*255;//k==0 ? 0 : 255;							
			deptex[j*dsx*4 + k*4 + 2] = 	255 - j/(float)dsy*255; //k==0 ? 255 : 0;							
			deptex[j*dsx*4 + k*4 + 3] = 	(unsigned char)(j/(float)dsy*255/(float)2);							
		}
	}
		
	gDeptex = new unsigned char[dsx*dsy*4]; //for good sample rates
	iDeptex = new unsigned char[dsx*dsy*4]; //for interactive sample rate
	copyScale(gluvv.volren.goodSamp, gDeptex);
	copyScale(gluvv.volren.interactSamp, iDeptex);

	lastSamp = gluvv.volren.goodSamp;
	lastGoodSamp = gluvv.volren.goodSamp;
	lastInteSamp = gluvv.volren.interactSamp;

	loadDepTex(gDeptex);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gluvv.volren.deptexName);
		
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			dsx,
			dsy,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			deptex);
	glFlush();

	//--------------- load ar deptex ------------------
	glGenTextures(1, &deptex2Name);   //the dep tex that we use for the tf
	glGenTextures(1, &gluvv.volren.deptex2Name);  //the dep tex that the tf widget sees

	deptex2 = new unsigned char[dsx*dsy*4]; //reference texture

	gluvv.volren.deptex2 = deptex2;

	for(j=0; j<dsy; ++j){
		for(int k=0; k<dsx; ++k){
			deptex2[j*dsx*4 + k*4 + 0] =  k/(float)dsx *255;							
			deptex2[j*dsx*4 + k*4 + 1] = 	j/(float)dsy *255;//k==0 ? 0 : 255;							
			deptex2[j*dsx*4 + k*4 + 2] = 	255 - j/(float)dsy*255; //k==0 ? 255 : 0;							
			deptex2[j*dsx*4 + k*4 + 3] = 	255;							
		}
	}
		
	loadDepTex(deptex2, deptex2Name);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gluvv.volren.deptex2Name);
		
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			dsx,
			dsy,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			deptex2);
	glFlush();

#if 0
	qnDeptex = new unsigned char[256*256*4];
	qnRef    = new float[256*256*3];

	for(int i=0; i<256; ++i){
		for(int j=0; j<256; ++j){
			qnRef[i*256*3 + j*3 + 0] = (i - 128)/(128.0);
			qnRef[i*256*3 + j*3 + 1] = (j - 128)/(128.0);
			qnRef[i*256*3 + j*3 + 2] = sqrt(1-(qnRef[i*256*3 + j*3 + 0]*qnRef[i*256*3 + j*3 + 0])-
																				(qnRef[i*256*3 + j*3 + 1]*qnRef[i*256*3 + j*3 + 1]));
		}
	}

	glGenTextures(1, &qnDeptexName);
#endif

}

//========================================================= load Dep tex
//======================================================================
void NV20VolRen3D2::loadDepTex(unsigned char *dtex)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, deptexName);
		
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			gluvv.tf.ptexsz[0],
			gluvv.tf.ptexsz[1],
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			dtex);
	glFlush();
}

void NV20VolRen3D2::loadDepTex(unsigned char *dtex, unsigned int name)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, name);
		
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			gluvv.tf.ptexsz[0],
			gluvv.tf.ptexsz[1],
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			dtex);
	glFlush();
}

//==================================================== create Shadow tex
//======================================================================
void NV20VolRen3D2::createShadowTex()  //creates the shadow texture
{
	glGenTextures(1, &shadowName);   //the dep tex that we use for the tf
	unsigned char *stex = new unsigned char[gluvv.light.buffsz[0]*gluvv.light.buffsz[1]*4];

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, shadowName);		
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			gluvv.light.buffsz[0],
			gluvv.light.buffsz[1],
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			stex);
	glFlush();
	glDisable(GL_TEXTURE_2D);
}

//=================================================== copy & scale alpha
//======================================================================

void NV20VolRen3D2::scaleAlpha(float nsr, float osr, unsigned char *dtex)
{
	if(nsr == osr) return;

	int dsx = gluvv.tf.ptexsz[0];
	int dsy = gluvv.tf.ptexsz[1];

	float alphaScale = 1/nsr;
	for(int i=0; i<dsy; ++i){
		for(int j=0; j<dsx; ++j){
			dtex[i*dsx*4 + j*4 + 3] = 
				(1- pow((1.0-deptex[i*dsx*4 + j*4 + 3]/255.0), alphaScale))*255;
		}
	}
}

void NV20VolRen3D2::copyScale(float sr, unsigned char *dtex)
{
	int dsx = gluvv.tf.ptexsz[0];
	int dsy = gluvv.tf.ptexsz[1];
	
	float alphaScale = 1.0/sr;
	for(int i=0; i<dsy; ++i){
		for(int j=0; j<dsx; ++j){
			dtex[i*dsx*4 + j*4 + 0] = deptex[i*dsx*4 + j*4 + 0];
			dtex[i*dsx*4 + j*4 + 1] = deptex[i*dsx*4 + j*4 + 1];
			dtex[i*dsx*4 + j*4 + 2] = deptex[i*dsx*4 + j*4 + 2];
			dtex[i*dsx*4 + j*4 + 3] = 
				(1.0 - pow((1.0-(deptex[i*dsx*4 + j*4 + 3]/255.0)), alphaScale))*255;
		}
	}
}

//========================================================= make current
//======================================================================

void NV20VolRen3D2::MakeCurrent()
{
	glutSetWindow(gluvv.mainWindow);
}
