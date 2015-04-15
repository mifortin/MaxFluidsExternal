

extern "C" {
	#include "jit.common.h"
}

#include "tmlMatrix.h"
#include "x_simd.h"
#include "SmartMM.h"
#include "Smart.h"
#import <Foundation/Foundation.h>
#include "limits.h"


typedef struct _jit_tml_KinectPointCloud 
{
	t_object				ob;
		
	//! Minimum distance between bg and blobs.
	float depthScale;
	
	//! Minimum between blobs.
	float minBlob;
	
	long bgTime;
	long thresh;
} t_jit_tml_KinectPointCloud ;



void *_jit_tml_KinectPointCloud_class;

//Various methods
t_jit_tml_KinectPointCloud *jit_tml_KinectPointCloud_new(void);
void jit_tml_KinectPointCloud_allocate(t_jit_tml_KinectPointCloud *x, long width, long height);
void jit_tml_KinectPointCloud_free(t_jit_tml_KinectPointCloud *x);
t_jit_err jit_tml_KinectPointCloud_matrix_calc(t_jit_tml_KinectPointCloud *x, void *inputs, void *outputs);
t_jit_err jit_tml_KinectPointCloud_init(void);




unsigned char clampChar(float inVal)
{
	if (inVal > 255) return 255;
	if (inVal < 0) return 0;
	return (unsigned char) inVal;
}

t_jit_err jit_tml_KinectPointCloud_matrix_calc(t_jit_tml_KinectPointCloud *o, void *inputs, void *outputs)
{
	t_jit_err err = JIT_ERR_NONE;
	
	try
	{
		//References to input
		TML::Matrix mtxDepth(inputs, 0);
		TML::Matrix mtxColour(inputs,1);
		
		TML::Matrix mtxOutput(outputs, 0);
		TML::Matrix mtxOColour(outputs,1);
		
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
		
		int elements = 0;
		float *depth = (float*)mtxDepth.data();
		int width = mtxDepth.dim(0);
		int height = mtxDepth.dim(1);
		
		int dsy = mtxDepth.stride(1)/4;
		int y,x;
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{

				float d = depth[x+y*dsy];
				if (d > 0)	elements++;
			}
		}
		
		_jit_matrix_info inf;
		memset(&inf, 0, sizeof(inf));
		inf.dimcount = 1;
		inf.dim[0] = elements;
		inf.planecount = 3;
		inf.type = _jit_sym_float32;
		mtxOutput.resizeTo(&inf);
		
		
		memset(&inf, 0, sizeof(inf));
		inf.dimcount = 1;
		inf.dim[0] = elements;
		inf.planecount = 4;
		inf.type = _jit_sym_char;
		mtxOColour.resizeTo(&inf);
		
		//Get data
		int *colour = (int*)mtxColour.data();
		float *output = (float*)mtxOutput.data();
		int *colOut = (int*)mtxOColour.data();
		
		
		
		elements = 0;
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				float d = depth[x+y*dsy];
				if (d > 0)
				{
					int color = colour[x+y*dsy];
					
					char *g = (char*)&color;
					{
						char tmp = g[0];
						g[0] = g[1];
						g[1] = g[2];
						g[2] = g[3];
						g[3] = tmp;
					}
					
					
					output[3*elements + 0] = x/(float)width - 0.5f;
					output[3*elements + 1] = y/(float)height - 0.5f;
					output[3*elements + 2] = d;
					
					colOut[elements] = color;
					
					elements++;
				}
			}
		}
		

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


t_jit_err jit_tml_KinectPointCloud_init(void) 
{
	long attrflags=0;
	t_jit_object *attr;
	t_jit_object *mop, *o;
	
	//Create class with given constructors & destructors
	_jit_tml_KinectPointCloud_class = jit_class_new("jit_tml_KinectPointCloud",(method)jit_tml_KinectPointCloud_new,(method)jit_tml_KinectPointCloud_free,
		sizeof(t_jit_tml_KinectPointCloud),A_CANT,0L); //A_CANT = untyped

	// 0 matrix input / 4 matrix output
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,2,2);

	// need this for getting correct matrix_info from 2nd input matrix....  (see jit.concat.c...)
	jit_mop_input_nolink(mop,2);
	o= (t_jit_object*)jit_object_method(mop,_jit_sym_getinput,2);
	jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt); 
	
	jit_class_addadornment(_jit_tml_KinectPointCloud_class,mop);
	
	// add methods
	jit_class_addmethod(_jit_tml_KinectPointCloud_class, (method)jit_tml_KinectPointCloud_matrix_calc, "matrix_calc", A_CANT, 0L);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"depthScale",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectPointCloud,depthScale));
	jit_class_addattr(_jit_tml_KinectPointCloud_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"jitterBlob",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectPointCloud,minBlob));
	jit_class_addattr(_jit_tml_KinectPointCloud_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"threshold",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectPointCloud,thresh));
	jit_class_addattr(_jit_tml_KinectPointCloud_class,attr);
	
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"expireFrames",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_KinectPointCloud,bgTime));
	jit_class_addattr(_jit_tml_KinectPointCloud_class,attr);
	
	
	//Done!	
	jit_class_register(_jit_tml_KinectPointCloud_class);

	return JIT_ERR_NONE;
}



t_jit_tml_KinectPointCloud *jit_tml_KinectPointCloud_new(void)
{
	t_jit_tml_KinectPointCloud *x;
	
	try
	{
		if (x=(t_jit_tml_KinectPointCloud *)jit_object_alloc(_jit_tml_KinectPointCloud_class))
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

void jit_tml_KinectPointCloud_free(t_jit_tml_KinectPointCloud *x)
{
}



