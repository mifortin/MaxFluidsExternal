

extern "C" {
	#include "jit.common.h"
}

#include "tmlMatrix.h"
#include "x_simd.h"
#include "SmartMM.h"
#include "Smart.h"
#import <Foundation/Foundation.h>
#include "limits.h"


typedef struct _jit_tml_KinectColourDepth 
{
	t_object				ob;
		
	//! Minimum distance between bg and blobs.
	float depthScale;
	
	//! Minimum between blobs.
	float minBlob;
	
	long bgTime;
	long thresh;
} t_jit_tml_KinectColourDepth ;



void *_jit_tml_KinectColourDepth_class;

//Various methods
t_jit_tml_KinectColourDepth *jit_tml_KinectColourDepth_new(void);
void jit_tml_KinectColourDepth_allocate(t_jit_tml_KinectColourDepth *x, long width, long height);
void jit_tml_KinectColourDepth_free(t_jit_tml_KinectColourDepth *x);
t_jit_err jit_tml_KinectColourDepth_matrix_calc(t_jit_tml_KinectColourDepth *x, void *inputs, void *outputs);
t_jit_err jit_tml_KinectColourDepth_init(void);




unsigned char clampChar(float inVal)
{
	if (inVal > 255) return 255;
	if (inVal < 0) return 0;
	return (unsigned char) inVal;
}

t_jit_err jit_tml_KinectColourDepth_matrix_calc(t_jit_tml_KinectColourDepth *o, void *inputs, void *outputs)
{
	t_jit_err err = JIT_ERR_NONE;
	
	try
	{
		//References to input
		TML::Matrix mtxDepth(inputs, 0);
		TML::Matrix mtxColour(inputs,1);
		
		TML::Matrix mtxOutput(outputs, 0);
		
		
		//Validate input
		if (mtxDepth.dims() != 2)	throw "Inlet 0 needs to be 2-dimensional";
		if (mtxColour.dims() != 2)	throw "Inlet 1 needs to be 2-dimensional";
		
		if (mtxDepth.planes() != 1)	throw "Inlet 0 needs 1 plane";
		if (mtxColour.planes() != 4)	throw "Inlet 1 needs 4 planes";
		
		if (!mtxDepth.isFloat32())	throw "Inlet 0 needs to be float32";
		if (!mtxColour.isChar())	throw "Inlet 1 needs to be character type";
		
		if (mtxDepth.dim(0) != mtxColour.dim(0)	||
			mtxDepth.dim(1)	!= mtxColour.dim(1))
			throw "Width and height of matrices in inlets 0 and 1 must be equal";
			
		mtxOutput.resizeTo(&mtxColour);
		
		
		//Get data
		float *depth = (float*)mtxDepth.data();
		int *colour = (int*)mtxColour.data();
		int *output = (int*)mtxOutput.data();
		
		int width = mtxDepth.dim(0);
		int height = mtxDepth.dim(1);
		
		int dsx = mtxDepth.stride(0)/4;
		int dsy = mtxDepth.stride(1)/4;
		
		//Constants from http://nicolas.burrus.name/index.php/Research/KinectCalibration
		const float fx_rgb = 5.2921508098293293e+02;
		const float fy_rgb = 5.2556393630057437e+02;
		const float cx_rgb = 3.2894272028759258e+02;
		const float cy_rgb = 2.6748068171871557e+02;
		const float k1_rgb = 2.6451622333009589e-01;
		const float k2_rgb = -8.3990749424620825e-01;
		const float p1_rgb = -1.9922302173693159e-03;
		const float p2_rgb = 1.4371995932897616e-03;
		const float k3_rgb = 9.1192465078713847e-01;
		
		const float fx_d = 5.9421434211923247e+02;
		const float fy_d = 5.9104053696870778e+02;
		const float cx_d = 3.3930780975300314e+02;
		const float cy_d = 2.4273913761751615e+02;
		const float k1_d = -2.6386489753128833e-01;
		const float k2_d = 9.9966832163729757e-01;
		const float p1_d = -7.6275862143610667e-04;
		const float p2_d = 5.0350940090814270e-03;
		const float k3_d = -1.3053628089976321e+00;
		
		const float Tx = 1.9985242312092553e-02;
		const float Ty = -7.4423738761617583e-04;
		const float Tz = -1.0916736334336222e-02;
		
		const float R11 = 9.9984628826577793e-01;
		const float R21 = 1.2635359098409581e-03;
		const float R31 = -1.7487233004436643e-02;
		
		const float R12 = -1.4779096108364480e-03;
		const float R22 = 9.9992385683542895e-01;
		const float R32 = -1.2251380107679535e-02;
		
		const float R13 = 1.7470421412464927e-02;
		const float R23 = 1.2275341476520762e-02;
		const float R33 = 9.9977202419716948e-01;
		
		
		
		int y,x;
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				float D = (o->depthScale * depth[x+y*dsy]);
				float P3Dx = (x - cx_d) * D / fx_d;
				float P3Dy = (y - cy_d) * D / fy_d;
				float P3Dz = D;
				
				float P3Dx_ = Tx + R11*P3Dx + R21*P3Dy + R31*P3Dz;
				float P3Dy_ = Ty + R12*P3Dx + R22*P3Dy + R32*P3Dz;
				float P3Dz_ = Tz + R13*P3Dx + R23*P3Dy + R33*P3Dz;
				
				if (P3Dz_ == 0)
					output[x + y*dsy] = 0;
				else
				{
					float X = P3Dx_ * fx_rgb / P3Dz_ + cx_rgb;
					float Y = P3Dy_ * fy_rgb / P3Dz_ + cy_rgb;
				
					//Color camera calibration
					float fx = (float)X / (float)width-0.5f;
					float fy = (float)Y / (float)height-0.5f;
				
					float r = fx*fx + fy*fy;
					float x_ = fx*(1+k1_rgb*r + k2_rgb*r*r
										+ k3_rgb*r*r*r*r)
										+ 2*p1_rgb*fx*fy
										+ p2_rgb*(r + 2*fx*fx);
					float y_ = fy*(1+k1_rgb*r + k2_rgb*r*r
										+ k3_rgb*r*r*r*r)
										+ p1_rgb*(r + 2*fy*fy)
										+ 2*p2_rgb*fx*fy;
					
					x_ = x_ + 2*(fx - x_);
					y_ = y_ + 2*(fy - y_);
					
					int u = (float)(640 * x_ + 320);
					int v = (float)(480 * y_ + 240);
					
					if (u < 0 || v < 0 || u >= width || v >= height)
						output[x+y*dsy] = 0;
					else
						output[x+y*dsy] = colour[u + v*dsy];
				}
			}
		}
		
//		for (y=0; y<height; y++)
//		{
//			for (x=0; x<width; x++)
//			{
//				float D = o->depthScale * depth[x+y*dsy];
//				float P3Dx = (x - cx_d) * D / fx_d;
//				float P3Dy = (y - cy_d) * D / fy_d;
//				float P3Dz = D;
//				
//				float P3Dx_ = Tx + R11*P3Dx + R21*P3Dy + R31*P3Dz;
//				float P3Dy_ = Ty + R12*P3Dx + R22*P3Dy + R32*P3Dz;
//				float P3Dz_ = Tz + R13*P3Dx + R23*P3Dy + R33*P3Dz;
//				
//				if (P3Dz_ == 0)
//					output[x + y*dsy] = 0;
//				else
//				{
//					float X = P3Dx_ * fx_rgb / P3Dz_ + cx_rgb;
//					float Y = P3Dy_ * fy_rgb / P3Dz_ + cy_rgb;
//					
//					int iX = (int)X;
//					int iY = (int)Y;
//					
//					//iX = x - (iX - x);
//					//iY = y - (iY - y);
//					
//					if (iX < 0)	iX = 0;
//					if (iY < 0)	iY = 0;
//					if (iX >= width) iX = width;
//					if (iY >= height) iY = height;
//					
//					output[x + y*dsy] = colour[iX + iY*dsy];
//				}
//
//			}
//		}
	}
	catch (const char *in_s)
	{
		post("Error in Matrix Calc: %s", in_s);
		return JIT_ERR_GENERIC;
	}
	catch (...)
	{
		return JIT_ERR_GENERIC;
	}
	
	
	return err;
}


t_jit_err jit_tml_KinectColourDepth_init(void) 
{
	long attrflags=0;
	t_jit_object *attr;
	t_jit_object *mop, *o;
	
	//Create class with given constructors & destructors
	_jit_tml_KinectColourDepth_class = jit_class_new("jit_tml_KinectColourDepth",(method)jit_tml_KinectColourDepth_new,(method)jit_tml_KinectColourDepth_free,
		sizeof(t_jit_tml_KinectColourDepth),A_CANT,0L); //A_CANT = untyped

	// 0 matrix input / 4 matrix output
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,2,1);

	// need this for getting correct matrix_info from 2nd input matrix....  (see jit.concat.c...)
	jit_mop_input_nolink(mop,2);
	o= (t_jit_object*)jit_object_method(mop,_jit_sym_getinput,2);
	jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt); 
	
	jit_class_addadornment(_jit_tml_KinectColourDepth_class,mop);
	
	// add methods
	jit_class_addmethod(_jit_tml_KinectColourDepth_class, (method)jit_tml_KinectColourDepth_matrix_calc, "matrix_calc", A_CANT, 0L);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"depthScale",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectColourDepth,depthScale));
	jit_class_addattr(_jit_tml_KinectColourDepth_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"jitterBlob",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectColourDepth,minBlob));
	jit_class_addattr(_jit_tml_KinectColourDepth_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"threshold",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectColourDepth,thresh));
	jit_class_addattr(_jit_tml_KinectColourDepth_class,attr);
	
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"expireFrames",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectColourDepth,bgTime));
	jit_class_addattr(_jit_tml_KinectColourDepth_class,attr);
	
	
	//Done!	
	jit_class_register(_jit_tml_KinectColourDepth_class);

	return JIT_ERR_NONE;
}



t_jit_tml_KinectColourDepth *jit_tml_KinectColourDepth_new(void)
{
	t_jit_tml_KinectColourDepth *x;
	
	try
	{
		if (x=(t_jit_tml_KinectColourDepth *)jit_object_alloc(_jit_tml_KinectColourDepth_class))
		{
			x->depthScale = 1.0f;
			x->minBlob = 0.01f;
			x->thresh = 10;
			x->bgTime = 30000;
		}
		else
			x = NULL;
	}
	catch(...)
	{
		x = NULL;
	}
	return x;
}

void jit_tml_KinectColourDepth_free(t_jit_tml_KinectColourDepth *x)
{
}



