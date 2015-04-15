

extern "C" {
	#include "jit.common.h"
}

#include "tmlMatrix.h"
#include "x_simd.h"
#include "SmartMM.h"
#include "Smart.h"
#import <Foundation/Foundation.h>
#include "limits.h"

class information;

@interface TmlJitDepthBGOp : NSOperation
{
	information* m_info;
}

+ (TmlJitDepthBGOp*)TmlJitDepthBGOp;

- (void)setup:(information*)in_info;

@end



//! Information about blobs
struct blobInformation
{
	unsigned char m_id;
	float m_mass;
	float m_centerX;
	float m_centerY;
	float m_centerZ;
	
	blobInformation(unsigned char in_id = 0)
	: m_id(in_id)
	, m_mass(0)
	, m_centerX(0)
	, m_centerY(0)
	, m_centerZ(0)
	{}
	
	blobInformation&operator+=(const blobInformation &in_o)
	{
		m_centerX = m_centerX*m_mass + in_o.m_centerX * in_o.m_mass;
		m_centerY = m_centerY*m_mass + in_o.m_centerY * in_o.m_mass;
		m_centerZ = m_centerZ*m_mass + in_o.m_centerZ * in_o.m_mass;
		
		m_mass += in_o.m_mass;
		
		m_centerX /= m_mass;
		m_centerY /= m_mass;
		m_centerZ /= m_mass;
		
		return *this;
	}
};



class information
{
private:
	//!Background + first matrix
	Many<float> bgData;
	
	//!Latency until sure it's in foreground
	Many<int>	fgData;
	
	//! First input matrix (raw depth data assumed)
	Many<float> i1;
	
	//! Second output matrix (blobs!)
	Many<float> o2;
	
	//! Buffer to simplify the recursion...
	Many<int>	m_r;
	
	//! Buffer to hold blob data
	Many<unsigned char>	m_blob;
	Many<unsigned char>	m_blob2;
	
	//! Whether a blob ID is used or not
	bool m_used[256];
	
	//! Avoided conflicting among frames...
	bool m_oldused[256];
	
	//! Old blob information.
	/*! We wish to minimize the distance between the centers of mass as blobs
		"split".  Two people cross paths, for example.
		
		 We may also use this to determine when blobs should be separated... */
	blobInformation m_blobInfo[256];
	
	//! Number of blobs
	int m_nBlobs;
		
	int dSize;
	int e;
	
	//! Threshold -- minimum distance from background
	int m_t;
	
	//! Minimum 
	float m_min;
	
	//! Minimum distance of blobs
	float m_minBlob;
	
	//! Time (frames) until object is part of background...
	int m_bgTime;
	
	t_jit_matrix_info	inf;	//Info for matrix info...
	
	//! Operation queue
	OneNS<NSOperationQueue*>		m_queue;
	
	//! Single operation (jumps onto grand central)
	OneNS<TmlJitDepthBGOp*>				m_o;
	
public:
	information()
	{
		dSize = 0;
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		m_queue = [[[NSOperationQueue alloc] init] autorelease];
		[pool release];
	}
	
	~information()
	{
		[m_queue() waitUntilAllOperationsAreFinished];
	}
	
	//! Compute the energy (mass) of a blob
	/*!	\param k[in]	The index into the blob
		\param sy[in]	The stride y in bytes
		\param h[in]	The height in cells */
	blobInformation energy(const int k, const int sy, const int h)
	{
		int start = 0;
		int end = 1;
		fgData()[k] = -fgData()[k];
		m_r()[0] = k;
		
		unsigned char r = 0;
		
		int z[256] = {0};
		
		blobInformation b;
		
		//Examine all the pixels
		while (start != end)
		{
			int c = m_r()[start];
			b.m_mass++;
			b.m_centerX += c/sy;
			b.m_centerY += c%sy;
			b.m_centerZ += fgData()[c];
			
			if (m_blob2()[c] != 0)
			{
				z[m_blob2()[c]]++;
			}
			
			if (c-1 > 0 && fgData()[c-1] > 3 && fabs(bgData()[c-1] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c-1] = -fgData()[c-1];
				m_r()[end] = c-1;
				end++;
			}
			
			if (c-sy > 0 && fgData()[c-sy] > 3 && fabs(bgData()[c-sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c-sy] = -fgData()[c-sy];
				m_r()[end] = c-sy;
				end++;
			}
			
			if (c+1 < dSize/4 && fgData()[c+1] > 3 && fabs(bgData()[c+1] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c+1] = -fgData()[c+1];
				m_r()[end] = c+1;
				end++;
			}
			
			if (c+sy < dSize/4 && fgData()[c+sy] > 3 && fabs(bgData()[c+sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c+sy] = -fgData()[c+sy];
				m_r()[end] = c+sy;
				end++;
			}
			start++;
		}
		
		//Average out the values...
		b.m_centerX /= b.m_mass;
		b.m_centerY /= b.m_mass;
		b.m_centerZ /= b.m_mass;
		
		int m = z[0];
		r = 0;
		for (start =0; start<256; start++)
		{
			if (m < z[start])
			{
				m = z[start];
				r = start;
			}
		}
		
		e = end;
		
		b.m_id = r;
		
		return b;
	}
	
	void clear(const int k, const int sy, const int h)
	{
		int start = 0;
		int end = 1;
		fgData()[k] = 0;
		m_r()[0] = k;
		
		while (start != end)
		{
			int c = m_r()[start];
			
			if (c-1 > 0 && fgData()[c-1] < -3 && fabs(bgData()[c-1] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c-1] = 0;
				m_r()[end] = c-1;
				end++;
			}
			
			if (c-sy > 0 && fgData()[c-sy] < -3 && fabs(bgData()[c-sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c-sy] = 0;
				m_r()[end] = c-sy;
				end++;
			}
			
			if (c+1 < dSize/4 && fgData()[c+1] < -3 && fabs(bgData()[c+1] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c+1] = 0;
				m_r()[end] = c+1;
				end++;
			}
			
			if (c+sy < dSize/4 && fgData()[c+sy] < -3 && fabs(bgData()[c+sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				fgData()[c+sy] = 0;
				m_r()[end] = c+sy;
				end++;
			}
			start++;
		}
		
		e = end;
	}
	
	//! Marks the blob
	void markBlob(const int k, const int sy, const int h, const blobInformation &b)
	{
		int start = 0;
		int end = 1;
		m_r()[0] = k;
		
		unsigned char r = b.m_id;
		
		if (r == 0)
		{
			int x;
			for (x=0; x<256; x++)
			{
				r = m_nBlobs;
				if (!m_used[r] && !m_oldused[r])
				{
					break;
				}
				m_nBlobs = (m_nBlobs + 1) % 256;
			}
			
			if (m_nBlobs == 0)	m_nBlobs++;
		}
		
		m_used[r] = true;
		m_blobInfo[r] += b;
		
		while (start != end)
		{
			int c = m_r()[start];
			
			if (c-1 > 0 && fgData()[c-1] < -3 && m_blob()[c-1] != r && fabs(bgData()[c-1] - bgData()[c])*m_minBlob < 0.1f)
			{
				m_blob()[c-1] = r;
				m_r()[end] = c-1;
				end++;
			}
			
			if (c-sy > 0 && fgData()[c-sy] < -3 && m_blob()[c-sy] != r && fabs(bgData()[c-sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				m_blob()[c-sy] = r;
				m_r()[end] = c-sy;
				end++;
			}
			
			if (c+1 < dSize/4 && fgData()[c+1] < -3 && m_blob()[c+1] != r && fabs(bgData()[c+1] - bgData()[c])*m_minBlob < 0.1f)
			{
				m_blob()[c+1] = r;
				m_r()[end] = c+1;
				end++;
			}
			
			if (c+sy < dSize/4 && fgData()[c+sy] < -3 && m_blob()[c+sy] != r && fabs(bgData()[c+sy] - bgData()[c])*m_minBlob < 0.1f)
			{
				m_blob()[c+sy] = r;
				m_r()[end] = c+sy;
				end++;
			}
			start++;
		}
		
		e = end;
	}
	
	//! Second thread is called by GCD to do extra work in parallel with MAX
	void secondThread()
	{
		//Clear out data
		memcpy(m_oldused, m_used, sizeof(m_used));
		memset(m_used, 0, sizeof(m_used));
		memset(m_blobInfo, 0, sizeof(m_blobInfo));
		
		//Length of loop
		const int ll = dSize / 4;
		
		float *fi = i1();
		
		//Update the background
		int k;
		
		#ifdef __SSE3__
		if (ll %4 == 0 && (unsigned int)(fi)%16 == 0)
		{
			x128f *xfi = (x128f*)fi;
			x128f *xbg = (x128f*)bgData();
			x128i *xfg = (x128i*)fgData();
			
			x128i thousand = {m_bgTime,m_bgTime,m_bgTime,m_bgTime};
			
			x128f maskGT0, maskGTBG;
			x128f maskGT1000;
			
			for (k=0; k<ll/4; k++)
			{
				maskGT0 = x_cmp_lt(x_simd_zero, xfi[k]);
				maskGTBG = x_cmp_lt(xbg[k], xfi[k]);
				maskGT1000 = (x128f)x_cmp_lti(thousand, xfg[k]);
				
				maskGT0 = x_and(maskGT0, x_or(maskGTBG, maskGT1000));
				
				xbg[k] = x_sel(maskGT0, xbg[k], xfi[k]);
			}
			
		}
		else
		#endif
		{
			for (k=0; k<ll; k++)
			{
				if (fi[k] > 0 && (fi[k] > bgData()[k] || fgData()[k] > m_bgTime))
				{
					bgData()[k] = fi[k];
				}
			}
			
		}
		
		
		//Update the foreground
		
		#ifdef __SSE3__
		if (ll %4 == 0 && (unsigned int)(fi)%16 == 0)
		{
			x128f maskGT0, maskLTexpr;
			
			x128f *xfi = (x128f*)fi;
			x128f *xbg = (x128f*)bgData();
			x128i *xfg = (x128i*)fgData();
			
			x128f xmin = {m_min, m_min, m_min, m_min};
			
			for (k=0; k<ll/4; k++)
			{
				x128i p1 = x_iadd(xfg[k], x_simd_ione);
				
				maskGT0 = x_cmp_lt(x_simd_zero, xfi[k]);
				
				x128f rhs = x_sub(xbg[k], x_mul(xmin, xbg[k]));
				
				maskLTexpr = x_cmp_lt(xfi[k], rhs);
				
				maskGT0 = x_and(maskGT0, maskLTexpr);
				
				xfg[k] = (x128i)x_sel(maskGT0, x_simd_zero, (x128f)p1);
			}
		}
		else
		#endif
		{
			for (k=0; k<ll; k++)
			{
				if (fi[k] > 0 && fi[k] < bgData()[k] - m_min*bgData()[k])
				{
					fgData()[k]++;
				}
				else
					fgData()[k] = 0;
			}
		}
		
		memcpy(m_blob2(), m_blob(), inf.dim[1]*inf.dimstride[1]/4);
		memset(m_blob(), 0, inf.dim[1]*inf.dimstride[1]/4);
		for (k=0; k<ll; k++)
		{
			if (fgData()[k] > 3)
			{
				e = 0;
				//post("-> %i %i %i\n", k ,i.stride(1)/sizeof(float), i.dim(1));
				blobInformation r = energy(k, inf.dimstride[1]/4, inf.dim[1]);
				if (e < m_t)
				{
					//int oldE = e;
					//post("--> %i %i %i\n", k ,i.stride(1)/sizeof(float), i.dim(1));
					e = 0;
					clear(k, inf.dimstride[1]/4, inf.dim[1]);
					
//					if (e > in_thresh)
//						post("ERROR %i %i %i\n", e, oldE, in_thresh);
				}
				else
					markBlob(k, inf.dimstride[1]/4, inf.dim[1], r);
			}
		}
		
		//Now, wait a few frames until we
		//display the data...
		
		for (k=0; k<ll; k++)
			fgData()[k] = labs(fgData()[k]);
		
		float *ff = o2();
		for (k=0; k<ll; k++)
		{
			if (fgData()[k] > 3)
				ff[k] = fi[k];
			else
				ff[k] = 0;
		}
	}
	
	void matrixCalc(void *inputs, void *outputs, const float min, const int in_thresh,
					const int in_bgTime, const float in_minBlob)
	{
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		try {
			TML::Matrix i(inputs, 0);
			
			//Halt previous
			[m_queue() waitUntilAllOperationsAreFinished];
			
			//Coerce outputs first...
			if (dSize > 0)
			{
				//Setup matrices
				TML::Matrix fg(outputs,0);
				fg.resizeTo(&inf);
				
				TML::Matrix bg(outputs,1);
				bg.resizeTo(&inf);
				
				inf.type = _jit_sym_char;
				TML::Matrix bd(outputs,2);
				bd.resizeTo(&inf);
				
				inf.type = _jit_sym_float32;
				inf.dimcount = 1;
				inf.planecount = 4;
				inf.dim[0] = 256;
				TML::Matrix blobInfo(outputs,3);
				blobInfo.resizeTo(&inf);
				
				
				//Update the data.
				memcpy(fg.data(), bgData(), dSize);
				memcpy(bg.data(), o2(), dSize);
				
				int i;
				unsigned char *b = (unsigned char*)bd.data();
				for (i=0; i<inf.dim[1 ]; i++)
				{
					memcpy(b + bd.stride(1)*i, m_blob() + inf.dimstride[1]*i/4, 
								bd.dim(0));
				}
				
				b = (unsigned char*)blobInfo.data();
				for (i=0; i<256; i++)
				{
					float *r = (float*)(b + blobInfo.stride(0)*i);
					
					r[0] = m_blobInfo[i].m_mass;
					r[1] = m_blobInfo[i].m_centerX;
					r[2] = m_blobInfo[i].m_centerY;
					r[3] = m_blobInfo[i].m_centerZ;
				}
			}
			
			
			//post("Thresh %i\n", in_thresh);
			m_t = in_thresh;
			m_min =min;
			m_bgTime = in_bgTime;
			m_nBlobs = 1;
			m_minBlob = in_minBlob;
			
			if (!i.isFloat32())
				throw "Need float32 data";
			
			if (i.dims() != 2)
				throw "Need two dimensions";
			
			TML::Matrix	bg(outputs,0);
			
			//Match input to output....
			inf = i.rawInfo();
			
			//If the data size is too small, resize
			if (dSize != i.stride(1)*i.dim(1))
			{
				bgData = new float[i.stride(1)*i.dim(1)/4];
				fgData = new int[i.stride(1)*i.dim(1)/4];
				dSize = i.stride(1)*i.dim(1);
				
				i1 = new float[i.stride(1)*i.dim(1)/4];
				o2 = new float[i.stride(1)*i.dim(1)/4];
				
				m_r = new int[i.stride(1)*i.dim(1)/4];
				m_blob = new unsigned char[i.stride(1)*i.dim(1)/4];
				m_blob2 = new unsigned char[i.stride(1)*i.dim(1)/4];
				
				int x;
				for (x=0; x<dSize/4; x++)
					bgData()[x] = 0;
				
				for (x=0; x<dSize/4; x++)
					fgData()[x] = 0;
			}
			
			//Copy data...
			memcpy(i1(), i.data(), i.stride(1)*i.dim(1));
			
			m_o = [TmlJitDepthBGOp TmlJitDepthBGOp];
			//secondThread();
			[m_o() setup:this];
			[m_queue() addOperation:m_o()];
			
			[pool release];
		} catch(...)
		{
			[pool release];
			throw;
		}
	}
};

@implementation TmlJitDepthBGOp

+(TmlJitDepthBGOp*)TmlJitDepthBGOp
{
	return [[[TmlJitDepthBGOp alloc] init] autorelease];
}

- (void)setup:(information*)in_info
{
	m_info = in_info;
}

- (void)main
{
	m_info->secondThread();
}

@end



typedef struct _jit_tml_DepthBG 
{
	t_object				ob;
	
	information				*i;
		
	//! Minimum distance between bg and blobs.
	float min;
	
	//! Minimum between blobs.
	float minBlob;
	
	long bgTime;
	long thresh;
} t_jit_tml_DepthBG ;



void *_jit_tml_DepthBG_class;

//Various methods
t_jit_tml_DepthBG *jit_tml_DepthBG_new(void);
void jit_tml_DepthBG_allocate(t_jit_tml_DepthBG *x, long width, long height);
void jit_tml_DepthBG_free(t_jit_tml_DepthBG *x);
t_jit_err jit_tml_DepthBG_matrix_calc(t_jit_tml_DepthBG *x, void *inputs, void *outputs);
t_jit_err jit_tml_DepthBG_init(void);




unsigned char clampChar(float inVal)
{
	if (inVal > 255) return 255;
	if (inVal < 0) return 0;
	return (unsigned char) inVal;
}

t_jit_err jit_tml_DepthBG_matrix_calc(t_jit_tml_DepthBG *x, void *inputs, void *outputs)
{
	t_jit_err err = JIT_ERR_NONE;
	
	try
	{
		
		information *i = x->i;
		
		if (i == NULL)	return err;
		
		i->matrixCalc(inputs, outputs, x->min, x->thresh, x->bgTime, x->minBlob);
		
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


t_jit_err jit_tml_DepthBG_init(void) 
{
	long attrflags=0;
	t_jit_object *attr;
	t_jit_object *mop, *o;
	
	//Create class with given constructors & destructors
	_jit_tml_DepthBG_class = jit_class_new("jit_tml_DepthBG",(method)jit_tml_DepthBG_new,(method)jit_tml_DepthBG_free,
		sizeof(t_jit_tml_DepthBG),A_CANT,0L); //A_CANT = untyped

	// 0 matrix input / 4 matrix output
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,1,4);

	// need this for getting correct matrix_info from 2nd input matrix....  (see jit.concat.c...)
	//jit_mop_input_nolink(mop,2);
	// o= (t_jit_object*)jit_object_method(mop,_jit_sym_getinput,2);
	//jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt); 
	
	jit_class_addadornment(_jit_tml_DepthBG_class,mop);
	
	// add methods
	jit_class_addmethod(_jit_tml_DepthBG_class, (method)jit_tml_DepthBG_matrix_calc, "matrix_calc", A_CANT, 0L);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"jitter",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_DepthBG,min));
	jit_class_addattr(_jit_tml_DepthBG_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"jitterBlob",_jit_sym_float32,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_DepthBG,minBlob));
	jit_class_addattr(_jit_tml_DepthBG_class,attr);
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"threshold",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_DepthBG,thresh));
	jit_class_addattr(_jit_tml_DepthBG_class,attr);
	
	
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,"expireFrames",_jit_sym_long,attrflags, 
		(method)0L,(method)0L,calcoffset(t_jit_tml_DepthBG,bgTime));
	jit_class_addattr(_jit_tml_DepthBG_class,attr);
	
	
	//Done!	
	jit_class_register(_jit_tml_DepthBG_class);

	return JIT_ERR_NONE;
}



t_jit_tml_DepthBG *jit_tml_DepthBG_new(void)
{
	t_jit_tml_DepthBG *x;
	
	try
	{
		if (x=(t_jit_tml_DepthBG *)jit_object_alloc(_jit_tml_DepthBG_class))
		{
			x->i = new information;
			x->min = 0.1f;
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

void jit_tml_DepthBG_free(t_jit_tml_DepthBG *x)
{
	if (x->i)	delete x->i;
}



