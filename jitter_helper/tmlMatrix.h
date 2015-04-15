/*
 *  tmlMatrix.h
 *  jit.tml.TimeWarp
 
 2011 / Michael Fortin / TML
 
 *
 
 */

#ifndef TML_MATRIX
#define TML_MATRIX

#include "jit.common.h"
#include "math.h"
#include "limits.h"



namespace TML
{

class Math
{
public:
	static float sqrt(float in_f)	{	return ::sqrtf(in_f);	}
	static double sqrt(double in_f)	{	return ::sqrt(in_f);	}
	static long sqrt(long in_f)	{	return ::sqrtl(in_f);	}
	static int sqrt(int in_f)	{	return ::sqrtl(in_f);	}

	static float abs(float in_f)	{	return ::fabs(in_f);	}
	static double abs(double in_f)	{	return ::abs(in_f);	}
	static long abs(long in_f)	{	return ::labs(in_f);	}
	static int abs(int in_f)	{	return ::labs(in_f);	}

	static int maxValue(int in_f)	{	return INT_MAX;		}
};

class Matrix
{
private:
	void *m_matrix;
	long m_lock;
	
	char *m_dat;
	
	t_jit_matrix_info m_info;
	
public:
	Matrix(void *jit_list, int in_index);
	~Matrix();
	
	long dims()			{	return m_info.dimcount;		}
	long dim(int in_ind)	{	assert(in_ind < dims());
								return m_info.dim[in_ind];	}
	long stride(int in_ind)	{	assert(in_ind < dims());
								return m_info.dimstride[in_ind];	}
	
	const t_jit_matrix_info& rawInfo()	{	return m_info;	}
	
	long planes()		{	return m_info.planecount;	}
	
	t_symbol* type()	{	return m_info.type;			}
	
	bool isChar()		{	return type() == _jit_sym_char;	}
	bool isFloat32()	{	return type() == _jit_sym_float32;	}
	bool isFloat64()	{	return type() == _jit_sym_float64;	}
	
	void resizeTo(_jit_matrix_info *t);
	void resizeTo2D(int w, int h, int p, t_symbol *t);
	void resizeTo1D(int w, int p, t_symbol *t);
	void resizeTo(Matrix *m)	{	resizeTo(&m->m_info); }
	
	void *data();

#ifdef __OPENCV_CORE_C_H__
#warning "Building TML::Matrix with OpenCV bridge"
private:
	CvMat m_cv;

public:
	CvMat *OpenCVMatrix()
	{
		int r,c,step;
		if (dims() == 1)
		{
			r = dim(0);
			c = 1;
			step = stride(0);
		}
		else
		{
			r = dim(1);
			c = dim(0);
			step = stride(1);
		}
		
		int t;
		if (isChar())
			t = CV_MAKETYPE(CV_8U,planes());
		else if (isFloat32())
			t = CV_MAKETYPE(CV_32F,planes());
		else if (isFloat64())
			t = CV_MAKETYPE(CV_64F,planes());
	
		return cvInitMatHeader(&m_cv, r, c, t, data(), step);
	}
#endif
};

}

#endif
