/*
 *  tmlMatrix.cpp
 *  jit.tml.TimeWarp
 *
 */

#include "tmlMatrix.h"

using namespace TML;

Matrix::Matrix(void *jit_list, int in_index)
{
	m_matrix = jit_object_method(jit_list, _jit_sym_getindex, in_index);
	m_dat = NULL;
	if (m_matrix)
	{
		m_lock = (long)jit_object_method(m_matrix, _jit_sym_lock, 1);
		jit_object_method(m_matrix, _jit_sym_getinfo, &m_info);
	}
}


Matrix::~Matrix()
{
	if (m_matrix)
		jit_object_method(m_matrix, _jit_sym_unblock, m_lock);
}


void Matrix::resizeTo(_jit_matrix_info *t)
{
	if (m_info.type == t->type
		&& m_info.dimcount == t->dimcount
		&& m_info.planecount == t->planecount)
	{
		int x;
		bool same = true;
		for (x=0; x<m_info.dimcount; x++)
		{
			if (m_info.dim[x] != t->dim[x])
			{
				t = false;
				break;
			}
		}
		
		if (t)	return;
	}

	m_info.type = t->type;
	m_info.dimcount = t->dimcount;
	m_info.planecount = t->planecount;
	memcpy(m_info.dim, t->dim, sizeof(m_info.dim));
	memcpy(m_info.dimstride, t->dimstride, sizeof(m_info.dimstride));
	
	if (jit_object_method(m_matrix, _jit_sym_setinfo, &(m_info) ))
	{
		throw "Failed Setting Matrix Info";
	}
	jit_object_method(m_matrix,_jit_sym_getinfo,&(m_info) );
}


void Matrix::resizeTo2D(int w, int h, int p, t_symbol *t)
{
	t_jit_matrix_info m;
	memset(&m, 0, sizeof(m));
	
	m.dimcount = 2;
	m.dim[0] = w;
	m.dim[1] = h;
	m.planecount = p;
	m.type = t;
	
	resizeTo(&m);
}

void Matrix::resizeTo1D(int w, int p, t_symbol *t)
{
	t_jit_matrix_info m;
	memset(&m, 0, sizeof(m));
	
	m.dimcount = 1;
	m.dim[0] = w;
	m.planecount = p;
	m.type = t;
	
	resizeTo(&m);
}


void *Matrix::data()
{
	if (m_dat == NULL && m_matrix)
	{
		jit_object_method(m_matrix,_jit_sym_getdata,&m_dat);
	}
	
	return m_dat;
}
