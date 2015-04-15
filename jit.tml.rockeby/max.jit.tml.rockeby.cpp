

#include "jit.common.h"
#include "max.jit.mop.h"

#define EXTERNAL_NAME "jit_tml_rockeby"

typedef struct _max_jit_tml_rockeby 
{
	t_object		ob;
	void			*obex;
} t_max_jit_tml_rockeby;

t_jit_err jit_tml_rockeby_init(void); 

void *max_jit_tml_rockeby_class;

t_symbol *ps_getmap;


/* 
 * Constructor
 */
void *max_jit_tml_rockeby_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_jit_tml_rockeby *x;
	//long attrstart;
	void *o;

	//Allocate memory
	x = (t_max_jit_tml_rockeby *)max_jit_obex_new(max_jit_tml_rockeby_class,NULL); //only max object, no jit object
	
	o = jit_object_new(gensym(EXTERNAL_NAME));
	if (o)
	{
		max_jit_mop_setup_simple(x, o, argc, argv);
		max_jit_attr_args(x, argc, argv);
	}
	else
	{
		error("jit.tml.rockeby: unable to allocate object");
		freeobject((struct object *)x);
	}
	

	return (x);
}


/*
 *	Assist
 */
void max_jit_tmp_rockeby_assist(t_max_jit_tml_rockeby *x, void *b, long m, long a, char *s)
{
	if (m == 1) {
		switch(a)
		{
			case 0:
				sprintf(s, "(Matrix) Depth Information (rgba / 4 plane)");
				break;
		}
	} else { //output
		switch (a) {
			case 0:
				sprintf(s,"(Matrix) Colour background image (rgba / 4 plane)");
				break;
				
			case 1:
				sprintf(s,"(Matrix) Colour foreground image (rgba / 4 plane)");
				break;
				
			case 2:
				sprintf(s,"(Matrix) 4-plane (255, shadow/bg confidence, blobs, uncertain pixels - is it shadow? static?)");
				break;
				
			case 3:
				sprintf(s,"dumpout");
				break; 			
		}
	}
}


/*
 * Destructor
 */	
void max_jit_tml_rockeby_delete(t_max_jit_tml_rockeby *x)
{
	//only max object, no jit object
	max_jit_mop_free(x);
	jit_object_free(max_jit_obex_jitob_get(x));
	max_jit_obex_free(x);
}

/*
 * Main method
 */
int main(void)
{	
	//long attrflags;
	void *p, *q;//, *attr;
	
	//Initialize the ODE stuff
	jit_tml_rockeby_init();
	
	setup((t_messlist**)&max_jit_tml_rockeby_class,		//Define class type
		(method)max_jit_tml_rockeby_new,					//Constructor
		(method)max_jit_tml_rockeby_delete,				//Destructor
		(short)sizeof(t_max_jit_tml_rockeby), 				//Size of data to allocate
		0L, A_GIMME, 0);									//Default get-all

	p = max_jit_classex_setup(calcoffset(t_max_jit_tml_rockeby,obex));
	q = jit_class_findbyname(gensym(EXTERNAL_NAME));
	max_jit_classex_mop_wrap(p,q,0);
	max_jit_classex_standard_wrap(p,q,0);
	
	addmess((method)max_jit_tmp_rockeby_assist, "assist", A_CANT,0);
	post("Initialized: jit.tml.rockeby - XCode Build - " __DATE__ " - " __TIME__);
	
	return 0;
}

