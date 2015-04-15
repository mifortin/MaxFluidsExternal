

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
	
	vpx_image_t m_raw;
	vpx_codec_ctx_t m_codec;
	vpx_codec_enc_cfg_t m_cfg;
	
	int m_frameCnt;

public:
	information()
	{
		m_raw.planes[0] = NULL;
		m_codec.iface = NULL;
		
		vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &m_cfg, 0);
		
		m_cfg.rc_target_bitrate = 2000;
		m_cfg.g_w = 640;
		m_cfg.g_h = 480;
		
		m_frameCnt = 0;
		
		vpx_codec_enc_init(&m_codec, vpx_codec_vp8_cx(), &m_cfg, 0);
		
		vpx_img_alloc(&m_raw, VPX_IMG_FMT_I420, 640, 480, 1);
	}
	
	~information()
	{
		vpx_img_free(&m_raw);
		vpx_codec_destroy(&m_codec);
	}
	
	void matrixCalc(void *inputs, void *outputs, long in_bitRate)
	{
		//Validate the input
		TML::Matrix i(inputs,0);
		if (i.dims() != 2)
			throw "Input should be a 2D matrix";
		
		if (!i.isChar())
			throw "Input should have character data";
		
		if (i.planes() != 4)
			throw "Input needs 4 planes";
		
		if (i.dim(0) % 64 != 0)
			throw "Width needs to be a multiple of 64";
		
		if (i.dim(1) % 2 != 0)
			throw "Height needs to be a multiple of 2";
		
		if (i.dim(0) != m_cfg.g_w || i.dim(1) != m_cfg.g_h || m_cfg.rc_target_bitrate != in_bitRate)
		{
			vpx_img_free(&m_raw);
			vpx_img_alloc(&m_raw, VPX_IMG_FMT_I420, i.dim(0), i.dim(1), 1);
			
			vpx_codec_destroy(&m_codec);
			
			vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &m_cfg, 0);
			
			m_cfg.rc_target_bitrate = in_bitRate;
			m_cfg.g_w = i.dim(0);
			m_cfg.g_h = i.dim(1);
			
			vpx_codec_enc_init(&m_codec, vpx_codec_vp8_cx(), &m_cfg, 0);
		}
		
		
		//ARGB -> YYYY U V
		int x;
		const int N = 32;
		
		const int Uoffset = i.dim(0)*i.dim(1);
		const int Voffset = Uoffset + Uoffset/4;
		const int w = i.dim(0);
		const int h = i.dim(1);
		const int sy = i.stride(1);
		unsigned char *data = (unsigned char*)i.data();
		int y;
		
		unsigned char *buffer = m_raw.planes[0];
		
		//RRRR
		__v16qi rShuffle = {1, -1,-1,-1,
							5, -1,-1,-1,
							9, -1,-1,-1,
							13, -1,-1,-1};
		
		__v16qi gShuffle = {2, -1,-1,-1,
							6, -1,-1,-1,
							10,-1,-1,-1,
							14,-1,-1,-1};
		
		__v16qi bShuffle = {3,-1,-1,-1,
							7,-1,-1,-1,
							11,-1,-1,-1,
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
		
		__v8hi R2Y = {	27,	27,	27,	27,	27,	27,	27,	27};
		__v8hi G2Y = {	91,	91,	91,	91,	91,	91,	91,	91};
		__v8hi B2Y = {	9,	9,	9,	9,	9,	9,	9,	9};
		
		__v8hi R2U = {	-12,-12,-12,-12,-12,-12,-12,-12};
		__v8hi G2U = {	-43,-43,-43,-43,-43,-43,-43,-43};
		__v8hi B2U = {	55,	55,	55,	55,	55,	55,	55,	55};
		
		__v8hi R2V = {	78,	78,	78,	78,	78,	78,	78,	78};
		__v8hi G2V = {	-71,-71,-71,-71,-71,-71,-71,-71};
		__v8hi B2V = {	-7,	-7,	-7,	-7,	-7,	-7,	-7,	-7};
		
		__v8hi m127 = {127,127,127,127,127,127,127,127};
		
		__v8hi zero = {	0,	0,	0,	0,	0,	0,	0,	0};
		__v8hi two55 = {255,255,255,255,255,255,255,255};
		
		for (y=0; y<h; y+=2)
		{
			for (x=0; x<w; x+=N)
			{
				__v8hi tY[N/8];
				__v8hi bY[N/8];
				
				__v8hi tU[N/8];
				__v8hi bU[N/8];
				
				__v8hi tV[N/8];
				__v8hi bV[N/8];
				
				//Step 1: Convert to YUV
				int n;
				for (n=0; n<N; n+=8)	//Read 8x per lane
				{
					__v16qi tARGBx4_l = _mm_load_si128((__m128i*)(data + y*w*4 + x*4 + n*4));
					__v16qi tARGBx4_r = _mm_load_si128((__m128i*)(data + y*w*4 + x*4 + n*4 + 16));
					__v16qi bARGBx4_l = _mm_load_si128((__m128i*)(data + y*w*4 + x*4 + n*4 + sy));
					__v16qi bARGBx4_r = _mm_load_si128((__m128i*)(data + y*w*4 + x*4 + n*4 + sy + 16));
					
					// ARGB(1) ARGB(2) ARGB(3) ARGB(4) | ARGB(5) ARGB(6) ARGB(7) ARGB(8)
					// => AARRGGBB(1,5) AARRGGBB(2,6) | AARRGGBB(3,7) AARRGGBB(4,8)
					__v16qi tARGBx2_15 = _mm_unpacklo_epi8(tARGBx4_l, tARGBx4_r);
					__v16qi tARGBx2_26 = _mm_unpackhi_epi8(tARGBx4_l, tARGBx4_r);
					
					__v16qi bARGBx2_15 = _mm_unpacklo_epi8(bARGBx4_l, bARGBx4_r);
					__v16qi bARGBx2_26 = _mm_unpackhi_epi8(bARGBx4_l, bARGBx4_r);
					
					// AARRGGBB(1,5) AARRGGBB(2,6) | AARRGGBB(3,7) AARRGGBB(4,8)
					// => AAAARRRRGGGGBBBB(1,3,5,7) | AAAARRRRGGGGBBBB(2,4,6,8)
					__v16qi tARGB_1357 = _mm_unpacklo_epi8(tARGBx2_15, tARGBx2_26);
					__v16qi tARGB_2468 = _mm_unpackhi_epi8(tARGBx2_15, tARGBx2_26);
					
					__v16qi bARGB_1357 = _mm_unpacklo_epi8(bARGBx2_15, bARGBx2_26);
					__v16qi bARGB_2468 = _mm_unpackhi_epi8(bARGBx2_15, bARGBx2_26);
					
					//AAAARRRRGGGGBBBB(1,3,5,7) | AAAARRRRGGGGBBBB(2,4,6,8)
					// => AAAAAAAARRRRRRRR | GGGGGGGGBBBBBBBB
					__v16qi tAARR = _mm_unpacklo_epi8(tARGB_1357, tARGB_2468);
					__v16qi tGGBB = _mm_unpackhi_epi8(tARGB_1357, tARGB_2468);
					
					__v16qi bAARR = _mm_unpacklo_epi8(bARGB_1357, bARGB_2468);
					__v16qi bGGBB = _mm_unpackhi_epi8(bARGB_1357, bARGB_2468);
					
					//Unpack to 8 R's, 8 G's, and 8 B's.
					__v8hi tRRRR = _mm_unpackhi_epi8(tAARR, zero);
					__v8hi tGGGG = _mm_unpacklo_epi8(tGGBB, zero);
					__v8hi tBBBB = _mm_unpackhi_epi8(tGGBB, zero);
					
					__v8hi bRRRR = _mm_unpackhi_epi8(bAARR, zero);
					__v8hi bGGGG = _mm_unpacklo_epi8(bGGBB, zero);
					__v8hi bBBBB = _mm_unpackhi_epi8(bGGBB, zero);
					
					//Convert to YUV (8x parallel)
					__v8hi tYYYY = _mm_add_epi16(_mm_mullo_epi16(tRRRR, R2Y), _mm_add_epi16(_mm_mullo_epi16(tGGGG, G2Y), _mm_mullo_epi16(tBBBB, B2Y)));
					__v8hi tUUUU = _mm_add_epi16(_mm_mullo_epi16(tRRRR, R2U), _mm_add_epi16(_mm_mullo_epi16(tGGGG, G2U), _mm_mullo_epi16(tBBBB, B2U)));
					__v8hi tVVVV = _mm_add_epi16(_mm_mullo_epi16(tRRRR, R2V), _mm_add_epi16(_mm_mullo_epi16(tGGGG, G2V), _mm_mullo_epi16(tBBBB, B2V)));
					
					__v8hi bYYYY = _mm_add_epi16(_mm_mullo_epi16(bRRRR, R2Y), _mm_add_epi16(_mm_mullo_epi16(bGGGG, G2Y), _mm_mullo_epi16(bBBBB, B2Y)));
					__v8hi bUUUU = _mm_add_epi16(_mm_mullo_epi16(bRRRR, R2U), _mm_add_epi16(_mm_mullo_epi16(bGGGG, G2U), _mm_mullo_epi16(bBBBB, B2U)));
					__v8hi bVVVV = _mm_add_epi16(_mm_mullo_epi16(bRRRR, R2V), _mm_add_epi16(_mm_mullo_epi16(bGGGG, G2V), _mm_mullo_epi16(bBBBB, B2V)));
					
					tUUUU = _mm_add_epi16(_mm_srai_epi16(tUUUU, 7), m127);
					tVVVV = _mm_add_epi16(_mm_srai_epi16(tVVVV, 7), m127);
					
					bUUUU = _mm_add_epi16(_mm_srai_epi16(bUUUU, 7), m127);
					bVVVV = _mm_add_epi16(_mm_srai_epi16(bVVVV, 7), m127);
					
					//Remove the fractional portion and clamp in 0...255
					tY[n/8] = _mm_min_epi16(_mm_srai_epi16(_mm_max_epi16(tYYYY,zero), 7), two55);
					tU[n/8] = _mm_min_epi16(_mm_max_epi16(tUUUU,zero), two55);
					tV[n/8] = _mm_min_epi16(_mm_max_epi16(tVVVV,zero), two55);
					
					bY[n/8] = _mm_min_epi16(_mm_srai_epi16(_mm_max_epi16(bYYYY,zero), 7), two55);
					bU[n/8] = _mm_min_epi16(_mm_max_epi16(bUUUU,zero), two55);
					bV[n/8] = _mm_min_epi16(_mm_max_epi16(bVVVV,zero), two55);
				}
				
				
				// Step 2 - Write out Luma (part 1)
				for (n=0; n<N; n+=16)
				{
					__v8hi A = tY[n/8];
					__v8hi B = tY[n/8+1];
					
					__m128i Y = _mm_packus_epi16(A,B);
					
					_mm_storeu_si128((__m128i*)(buffer+y*w+x+n), Y);
				}
				
				for (n=0; n<N; n+=16)
				{
					__v8hi A = bY[n/8];
					__v8hi B = bY[n/8+1];
					
					__m128i Y = _mm_packus_epi16(A,B);
					
					_mm_storeu_si128((__m128i*)(buffer+y*w+x+n+w), Y);
				}
				
				//Step 3 -- U and V data...
				for (n=0; n<N; n+=32)
				{
					__m128i U16a = _mm_add_epi16(tU[n/8], bU[n/8]);
					__m128i U16b = _mm_add_epi16(tU[n/8+1], bU[n/8+1]);
					__m128i U16c = _mm_add_epi16(tU[n/8+2], bU[n/8+2]);
					__m128i U16d = _mm_add_epi16(tU[n/8+3], bU[n/8+3]);		
					
					U16a = _mm_srli_epi16(_mm_hadd_epi16(U16a, U16b),2);
					U16c = _mm_srli_epi16(_mm_hadd_epi16(U16c, U16d),2);
					
					__m128i U = _mm_packus_epi16(U16a, U16c);
					
					_mm_storeu_si128((__m128i*)(buffer+Uoffset+y/2*w/2 + x/2+n/2), U);
				}
				
				for (n=0; n<N; n+=32)
				{
					__m128i U16a = _mm_add_epi16(tV[n/8], bV[n/8]);
					__m128i U16b = _mm_add_epi16(tV[n/8+1], bV[n/8+1]);
					__m128i U16c = _mm_add_epi16(tV[n/8+2], bV[n/8+2]);
					__m128i U16d = _mm_add_epi16(tV[n/8+3], bV[n/8+3]);		
					
					U16a = _mm_srli_epi16(_mm_hadd_epi16(U16a, U16b),2);
					U16c = _mm_srli_epi16(_mm_hadd_epi16(U16c, U16d),2);
					
					__m128i U = _mm_packus_epi16(U16a, U16c);
					
					_mm_storeu_si128((__m128i*)(buffer+Voffset+y/2*w/2 + x/2+n/2), U);	
				}
			}
		}
		
		m_frameCnt++;
		vpx_codec_encode(&m_codec, &m_raw, m_frameCnt, 1, 0, VPX_DL_REALTIME);
		vpx_codec_iter_t iter = NULL;
		
		const vpx_codec_cx_pkt_t *pkt;
		
		while ((pkt = vpx_codec_get_cx_data(&m_codec, &iter)))
		{
			if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
			{
				
				//Generate output
				TML::Matrix o(outputs,0);
				
				_jit_matrix_info m;
				memset(&m, 0, sizeof(m));
				
				m.dimcount = 2;
				m.dim[0] = pkt->data.frame.sz;
				m.dim[1] = 1;
				m.dimstride[0] = pkt->data.frame.sz;
				m.dimstride[1] = 1;
				m.planecount = 1;
				m.type = _jit_sym_char;
				
				o.resizeTo(&m);
				
				memcpy(o.data(), pkt->data.frame.buf, pkt->data.frame.sz);
				
				break;
			}
		}
		
	}
};


typedef struct _jit_tml_vp8_encode 
{
	t_object				ob;
	
	long					bitRate;
	
	information				*i;
} t_jit_tml_vp8_encode ;



void *_jit_tml_vp8_encode_class;

//Various methods
t_jit_tml_vp8_encode *jit_tml_vp8_encode_new(void);
void jit_tml_vp8_encode_allocate(t_jit_tml_vp8_encode *x, long width, long height);
void jit_tml_vp8_encode_free(t_jit_tml_vp8_encode *x);
t_jit_err jit_tml_vp8_encode_matrix_calc(t_jit_tml_vp8_encode *x, void *inputs, void *outputs);
t_jit_err jit_tml_vp8_encode_init(void);



t_jit_err jit_tml_vp8_encode_open(t_jit_tml_vp8_encode *x)
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

t_jit_err jit_tml_vp8_encode_matrix_calc(t_jit_tml_vp8_encode *x, void *inputs, void *outputs)
{
	t_jit_err err = JIT_ERR_NONE;
	
	try
	{
		
		information *i = x->i;
		
		if (i == NULL)	return err;
		
		i->matrixCalc(inputs, outputs, x->bitRate);
		
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

t_jit_err jit_tml_vp8_encode_setBitrate(t_jit_tml_vp8_encode *x, void *attr, long argc, t_atom *argv)
{
	long ss;
	
	if (argc != 1)
	{
		error("jit.tml.vp8.encode: bitrate is a scalar!");
		return JIT_ERR_GENERIC;
	}
	
	jit_atom_arg_getlong(&ss, 0, 1, argv);
	
	if (ss < 100)
	{
		error("jit.tml.rockeby: Very low (<100 kilobit) bitrates not recommended.");
		return JIT_ERR_GENERIC;
	}
	
	x->bitRate = ss;
	
	return JIT_ERR_NONE;
}


t_jit_err jit_tml_vp8_encode_init(void) 
{
	long attrflags=0;
	t_jit_object *attr;
	t_jit_object *mop, *o;
	
	//Create class with given constructors & destructors
	_jit_tml_vp8_encode_class = jit_class_new("jit_tml_vp8_encode",(method)jit_tml_vp8_encode_new,(method)jit_tml_vp8_encode_free,
		sizeof(t_jit_tml_vp8_encode),A_CANT,0L); //A_CANT = untyped

	// 0 matrix input / 4 matrix output
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,1,1);

	// need this for getting correct matrix_info from 2nd input matrix....  (see jit.concat.c...)
	//jit_mop_input_nolink(mop,2);
	// o= (t_jit_object*)jit_object_method(mop,_jit_sym_getinput,2);
	//jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt); 
	
	jit_class_addadornment(_jit_tml_vp8_encode_class,mop);
	
	// add methods
	jit_class_addmethod(_jit_tml_vp8_encode_class, (method)jit_tml_vp8_encode_matrix_calc, "matrix_calc", A_CANT, 0L);
	
	
	jit_class_addmethod(_jit_tml_vp8_encode_class, (method)jit_tml_vp8_encode_open, "open", A_DEFLONG, 0L);
	
	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset, "kilobitrate", _jit_sym_long,
										 attrflags, (method)0L,
										 (method)jit_tml_vp8_encode_setBitrate,
										 calcoffset(t_jit_tml_vp8_encode, bitRate));
	jit_class_addattr(_jit_tml_vp8_encode_class, attr);
	
	
	//Done!	
	jit_class_register(_jit_tml_vp8_encode_class);

	return JIT_ERR_NONE;
}



t_jit_tml_vp8_encode *jit_tml_vp8_encode_new(void)
{
	t_jit_tml_vp8_encode *x;
	
	try
	{
		if (x=(t_jit_tml_vp8_encode *)jit_object_alloc(_jit_tml_vp8_encode_class))
		{
			x->bitRate = 1000*2;	//2 megabit
			x->i = new information;
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

void jit_tml_vp8_encode_free(t_jit_tml_vp8_encode *x)
{
	if (x->i)	delete x->i;
}



