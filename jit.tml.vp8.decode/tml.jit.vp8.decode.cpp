

extern "C" {
	#include "jit.common.h"
	
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"
}

#include "tmlMatrix.h"
#include "Smart.h"

#include <tmmintrin.h>

class information
{
private:
	
	vpx_codec_ctx_t m_codec;
	
public:
	information()
	{
		vpx_codec_dec_init(&m_codec, vpx_codec_vp8_dx(), NULL, 0);
	}
	
	~information()
	{
		vpx_codec_destroy(&m_codec);
	}
	
	void matrixCalc(void *inputs, void *outputs)
	{
		//Validate the input
		TML::Matrix i(inputs,0);
		if (i.dims() != 2)
			throw "Input should be a 2D matrix";
		
		if (!i.isChar())
			throw "Input should have character data";
		
		if (i.planes() != 1)
			throw "Input needs 1 planes";
		
		if (i.dim(1) != 1)
			throw "Height needs to be 1";
		
		//Pass the data to the decoder
		vpx_codec_decode(&m_codec, (unsigned char*)i.data(), i.dim(0), NULL, VPX_DL_REALTIME);
		
		vpx_codec_iter_t iter = NULL;
		vpx_image_t *img = vpx_codec_get_frame(&m_codec, &iter);
		
		if (img)
		{
			
			//RRRR
			__v16qi aShuffle = {0, -1,-1,-1,
				1, -1,-1,-1,
				2, -1,-1,-1,
				3, -1,-1,-1};
			
			__v16qi rShuffle = {4, -1,-1,-1,
				5, -1,-1,-1,
				6, -1,-1,-1,
				7, -1,-1,-1};
			
			__v16qi gShuffle = {8, -1,-1,-1,
				9, -1,-1,-1,
				10,-1,-1,-1,
				11,-1,-1,-1};
			
			__v16qi bShuffle = {12,-1,-1,-1,
				13,-1,-1,-1,
				14,-1,-1,-1,
				15,-1,-1,-1};
			
			//Shuffle duplicating each element.
			
			__v16qi a1Shuffle = {0, -1,-1,-1,
				0, -1,-1,-1,
				1, -1,-1,-1,
				1, -1,-1,-1};
			
			__v16qi a2Shuffle = {2, -1,-1,-1,
				2, -1,-1,-1,
				3, -1,-1,-1,
				3, -1,-1,-1};
			
			__v16qi r1Shuffle = {4, -1,-1,-1,
				4, -1,-1,-1,
				5,-1,-1,-1,
				5,-1,-1,-1};
			
			__v16qi r2Shuffle = {6,-1,-1,-1,
				6,-1,-1,-1,
				7,-1,-1,-1,
				7,-1,-1,-1};
			
			
			__v16qi g1Shuffle = {8, -1,-1,-1,
				8, -1,-1,-1,
				9, -1,-1,-1,
				9, -1,-1,-1};
			
			__v16qi g2Shuffle = {10, -1,-1,-1,
				10, -1,-1,-1,
				11, -1,-1,-1,
				11, -1,-1,-1};
			
			__v16qi b1Shuffle = {12, -1,-1,-1,
				12, -1,-1,-1,
				13,-1,-1,-1,
				13,-1,-1,-1};
			
			__v16qi b2Shuffle = {14,-1,-1,-1,
				14,-1,-1,-1,
				15,-1,-1,-1,
				15,-1,-1,-1};
			
			
			//Shuffle so elements are moved to front/back
			__v16qi _aShuffle = {
				0, 4, 8, 12,
				-1, -1, -1, -1,
				-1, -1, -1, -1,
				-1, -1, -1, -1};
			
			__v16qi _bShuffle = {
				-1, -1, -1, -1,
				0, 4, 8, 12,
				-1, -1, -1, -1,
				-1, -1, -1, -1};
			
			__v16qi _cShuffle = {
				-1, -1, -1, -1,
				-1, -1, -1, -1,
				0, 4, 8, 12,
				-1, -1, -1, -1};
			
			__v16qi _dShuffle = {
				-1, -1, -1, -1,
				-1, -1, -1, -1,
				-1, -1, -1, -1,
				0, 4, 8, 12};
			
			//Shuffle AAAARRRRGGGGBBBB into ARGB ARGB ARGB ARGB
			__v16qi swizzle = {
				0, 4, 8, 12,
				1, 5, 9, 13,
				2, 6, 10, 14,
				3, 7, 11, 15
			};
			
			const __v8hi Y2 = {64,64,64,64,64,64,64,64};
			
			const __v8hi U2G = {-13,-13,-13,-13,-13,-13,-13,-13};
			const __v8hi U2B = {136,136,136,136,136,136,136,136};
			
			const __v8hi V2R = {81,81,81,81,81,81,81,81};
			const __v8hi V2G = {-24,-24,-24,-24,-24,-24,-24,-24};
			
			__m128 m127 = {127.0f,127.0f,127.0f,127.0f};
			
			const __v8hi n127 = {-127,-127,-127,-127,-127,-127,-127,-127};
			
			const __v16qi FF =
			{0xFF,0,0,0,
			0xFF,0,0,0,
			0xFF,0,0,0,
			0xFF,0,0,0};
			
			const __v8hi n255 = {255,255,255,255,255,255,255,255};
			
			const int sW = img->stride[0];
			const int h = img->d_h;
			const int w = img->d_w;
			const int N = 64;
			
			TML::Matrix o(outputs,0);
			
			_jit_matrix_info m;
			memset(&m, 0, sizeof(m));
			
			m.dimcount = 2;
			m.dim[0] = img->d_w;
			m.dim[1] = img->d_h;
			m.dimstride[0] = 4;
			m.dimstride[1] = img->d_w*4;
			m.planecount = 4;
			m.type = _jit_sym_char;
			
			o.resizeTo(&m);
			const __v8hi zero = {0,0,0,0,0,0,0,0};
			const __v8hi two55 = {255,255,255,255,255,255,255,255};
			
			const __v8hi zero8i = {0,0,0,0,0,0,0,0};
			
			const int sy = o.stride(1);
			
			unsigned char *data = (unsigned char*)o.data();

			
			int x,y;
			int y2;
			for (y2=0; y2<h; y2+=2)
			{
				y = y2;// * (img->d_h-1) / img->h;
				
				for (x=0; x<w; x+=N)
				{
					__v8hi tY[N/8];
					__v8hi bY[N/8];
					
					__v8hi tU[N/8];					
					__v8hi tV[N/8];
					int n;
					
					// Step 2 - Write out Luma (part 1)
					for (n=0; n<N; n+=16)
					{
						__v16qi Y = _mm_load_si128((__m128i*)(img->planes[0]+y*sW+x+n));
						
						tY[n/8] = _mm_unpacklo_epi8(Y, zero8i);
						tY[n/8+1] = _mm_unpackhi_epi8(Y, zero8i);
					}
					
					for (n=0; n<N; n+=16)
					{
						__v16qi Y = _mm_load_si128((__m128i*)(img->planes[0]+y*sW+x+n+sW));
						
						bY[n/8] = _mm_unpacklo_epi8(Y, zero8i);
						bY[n/8+1] = _mm_unpackhi_epi8(Y, zero8i);
					}
					
					//Step 3 -- U and V data...
					for (n=0; n<N; n+=32)
					{
						__v16qi U = _mm_load_si128((__m128i*)(img->planes[1]+(y/2)*(sW/2) + x/2+n/2));
						
						__v16qi Uhi = _mm_unpacklo_epi8(U, U);
						__v16qi Ulo = _mm_unpackhi_epi8(U, U);
						
						tU[n/8+0] = (_mm_add_epi16(_mm_unpacklo_epi8(Uhi, zero8i), n127));
						tU[n/8+1] = (_mm_add_epi16(_mm_unpackhi_epi8(Uhi, zero8i), n127));
						tU[n/8+2] = (_mm_add_epi16(_mm_unpacklo_epi8(Ulo, zero8i), n127));
						tU[n/8+3] = (_mm_add_epi16(_mm_unpackhi_epi8(Ulo, zero8i), n127));
					}
					
					for (n=0; n<N; n+=32)
					{
						__v16qi U = _mm_load_si128((__m128i*)(img->planes[2]+(y/2)*(sW/2) + x/2+n/2));
						
						__v16qi Uhi = _mm_unpacklo_epi8(U, U);
						__v16qi Ulo = _mm_unpackhi_epi8(U, U);
						
						tV[n/8+0] = (_mm_add_epi16(_mm_unpacklo_epi8(Uhi, zero8i), n127));
						tV[n/8+1] = (_mm_add_epi16(_mm_unpackhi_epi8(Uhi, zero8i), n127));
						tV[n/8+2] = (_mm_add_epi16(_mm_unpacklo_epi8(Ulo, zero8i), n127));
						tV[n/8+3] = (_mm_add_epi16(_mm_unpackhi_epi8(Ulo, zero8i), n127));
					}
					
					//Step 1: Convert from YUV
					for (n=0; n<N; n+=8)	//Write 4x per lane
					{
						__v8hi YYY = (tY[n/8]);
						__v8hi UUU = (tU[n/8]);
						__v8hi VVV = (tV[n/8]);
						
						__v8hi RRR = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2), _mm_mullo_epi16(VVV, V2R));
						__v8hi GGG = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2), _mm_add_epi16(_mm_mullo_epi16(UUU, U2G), _mm_mullo_epi16(VVV, V2G)));
						__v8hi BBB = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2),  _mm_mullo_epi16(UUU, U2B));
						
						__v8hi RRRsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( RRR, zero),6), two55);
						__v8hi GGGsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( GGG, zero),6), two55);
						__v8hi BBBsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( BBB, zero),6), two55);
						
						//Unpack to 4-byte R-G-B values
						__v16qi R32hi = _mm_slli_epi32(_mm_unpacklo_epi8(RRRsi, zero8i), 8);
						__v16qi R32lo = _mm_slli_epi32(_mm_unpackhi_epi8(RRRsi, zero8i), 8);
						
						__v16qi G32hi = _mm_slli_epi32(_mm_unpacklo_epi8(GGGsi, zero8i), 16);
						__v16qi G32lo = _mm_slli_epi32(_mm_unpackhi_epi8(GGGsi, zero8i), 16);
						
						__v16qi B32hi = _mm_slli_epi32(_mm_unpacklo_epi8(BBBsi, zero8i), 24);
						__v16qi B32lo = _mm_slli_epi32(_mm_unpackhi_epi8(BBBsi, zero8i), 24);
						
						//Combine into two 4x ARGB pairs
						__v16qi ARGBlo = _mm_add_epi8(_mm_add_epi8(R32lo, G32lo), _mm_add_epi8(B32lo, FF));
						__v16qi ARGBhi = _mm_add_epi8(_mm_add_epi8(R32hi, G32hi), _mm_add_epi8(B32hi, FF));
						
						
						//Write
						_mm_store_si128((__m128i*)(data + y2*sy + x*4 + n*4), ARGBhi);
						_mm_store_si128((__m128i*)(data + y2*sy + x*4 + n*4+16), ARGBlo);
					}
					
					for (n=0; n<N; n+=8)	//Write 4x per lane
					{
						__v8hi YYY = (tY[n/8]);
						__v8hi UUU = (tU[n/8]);
						__v8hi VVV = (tV[n/8]);
						
						__v8hi RRR = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2), _mm_mullo_epi16(VVV, V2R));
						__v8hi GGG = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2), _mm_add_epi16(_mm_mullo_epi16(UUU, U2G), _mm_mullo_epi16(VVV, V2G)));
						__v8hi BBB = _mm_add_epi16(_mm_mullo_epi16(YYY,Y2),  _mm_mullo_epi16(UUU, U2B));
						
						__v8hi RRRsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( RRR, zero),6), two55);
						__v8hi GGGsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( GGG, zero),6), two55);
						__v8hi BBBsi = _mm_min_epi16( _mm_srli_epi16(_mm_max_epi16( BBB, zero),6), two55);
						
						//Unpack to 4-byte R-G-B values
						__v16qi R32hi = _mm_slli_epi32(_mm_unpacklo_epi8(RRRsi, zero8i), 8);
						__v16qi R32lo = _mm_slli_epi32(_mm_unpackhi_epi8(RRRsi, zero8i), 8);
						
						__v16qi G32hi = _mm_slli_epi32(_mm_unpacklo_epi8(GGGsi, zero8i), 16);
						__v16qi G32lo = _mm_slli_epi32(_mm_unpackhi_epi8(GGGsi, zero8i), 16);
						
						__v16qi B32hi = _mm_slli_epi32(_mm_unpacklo_epi8(BBBsi, zero8i), 24);
						__v16qi B32lo = _mm_slli_epi32(_mm_unpackhi_epi8(BBBsi, zero8i), 24);
						
						//Combine into two 4x ARGB pairs
						__v16qi ARGBlo = _mm_add_epi8(_mm_add_epi8(R32lo, G32lo), _mm_add_epi8(B32lo, FF));
						__v16qi ARGBhi = _mm_add_epi8(_mm_add_epi8(R32hi, G32hi), _mm_add_epi8(B32hi, FF));
						
						
						//Write
						_mm_store_si128((__m128i*)(data + y2*sy + x*4 + n*4 + sy), ARGBhi);
						_mm_store_si128((__m128i*)(data + y2*sy + x*4 + n*4+16 + sy), ARGBlo);
					}
					
				}
			}
		}
		
		
		
	}
};


typedef struct _jit_tml_vp8_decode 
{
	t_object				ob;
	
	information				*i;
} t_jit_tml_vp8_decode ;



void *_jit_tml_vp8_decode_class;

//Various methods
t_jit_tml_vp8_decode *jit_tml_vp8_decode_new(void);
void jit_tml_vp8_decode_allocate(t_jit_tml_vp8_decode *x, long width, long height);
void jit_tml_vp8_decode_free(t_jit_tml_vp8_decode *x);
t_jit_err jit_tml_vp8_decode_matrix_calc(t_jit_tml_vp8_decode *x, void *inputs, void *outputs);
t_jit_err jit_tml_vp8_decode_init(void);



t_jit_err jit_tml_vp8_decode_open(t_jit_tml_vp8_decode *x)
{
	if (x->i == NULL)
	{
		try {
			x->i = new information;
		} catch (...) {
			return JIT_ERR_GENERIC;
		}
	}
	
	return JIT_ERR_NONE;
}


unsigned char clampChar(float inVal)
{
	if (inVal > 255) return 255;
	if (inVal < 0) return 0;
	return (unsigned char) inVal;
}

t_jit_err jit_tml_vp8_decode_matrix_calc(t_jit_tml_vp8_decode *x, void *inputs, void *outputs)
{
	t_jit_err err = JIT_ERR_NONE;
	
	try
	{
		
		information *i = x->i;
		
		if (i == NULL)	return err;
		
		i->matrixCalc(inputs, outputs);
		
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


t_jit_err jit_tml_vp8_decode_init(void) 
{
	long attrflags=0;
	t_jit_object *attr;
	t_jit_object *mop, *o;
	
	//Create class with given constructors & destructors
	_jit_tml_vp8_decode_class = jit_class_new("jit_tml_vp8_decode",(method)jit_tml_vp8_decode_new,(method)jit_tml_vp8_decode_free,
		sizeof(t_jit_tml_vp8_decode),A_CANT,0L); //A_CANT = untyped

	// 0 matrix input / 4 matrix output
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,1,1);

	// need this for getting correct matrix_info from 2nd input matrix....  (see jit.concat.c...)
	//jit_mop_input_nolink(mop,2);
	// o= (t_jit_object*)jit_object_method(mop,_jit_sym_getinput,2);
	//jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt); 
	
	jit_class_addadornment(_jit_tml_vp8_decode_class,mop);
	
	// add methods
	jit_class_addmethod(_jit_tml_vp8_decode_class, (method)jit_tml_vp8_decode_matrix_calc, "matrix_calc", A_CANT, 0L);
	
	
	jit_class_addmethod(_jit_tml_vp8_decode_class, (method)jit_tml_vp8_decode_open, "open", A_DEFLONG, 0L);
	
	
	
	
	
	//Done!	
	jit_class_register(_jit_tml_vp8_decode_class);

	return JIT_ERR_NONE;
}



t_jit_tml_vp8_decode *jit_tml_vp8_decode_new(void)
{
	t_jit_tml_vp8_decode *x;
	
	try
	{
		if (x=(t_jit_tml_vp8_decode *)jit_object_alloc(_jit_tml_vp8_decode_class))
			x->i = new information;
		else
			x = NULL;
	}
	catch(...)
	{
		x = NULL;
	}
	return x;
}

void jit_tml_vp8_decode_free(t_jit_tml_vp8_decode *x)
{
	if (x->i)	delete x->i;
}



