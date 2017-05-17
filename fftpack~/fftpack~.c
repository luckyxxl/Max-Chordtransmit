#include "ext.h"
#include "ext_critical.h"
#include "z_dsp.h"

#define assert(x) if(!(x)) post("Assertion failed at %s:%d: %s", __FILE__, __LINE__, #x)
#define swap(x, y) { int t = x; x = y; y = t; }
#define countof(x) (sizeof(x)/sizeof(*(x)))

typedef struct _buffer
{
	size_t size;
	double data[4096];
} t_buffer;

typedef struct _fftpack
{
	t_pxobject obj;
	void *outlet, *outlet_peaks;
	int write_buffer, read_buffer, copy_buffer;
	int warn_overflow;

	t_buffer buffers[3];
} t_fftpack;

void *fftpack_new(t_symbol *s, long argc, t_atom *argv);
void fftpack_free(t_fftpack *x);
void fftpack_dsp64(t_fftpack *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void fftpack_perform64(t_fftpack *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void fftpack_bang(t_fftpack *x);

static t_class *fftpack_class = NULL;

void ext_main(void *r)
{
	t_class *c = class_new("fftpack~", (method)fftpack_new, (method)fftpack_free, sizeof(t_fftpack), NULL, 0);
	class_dspinit(c);
	class_addmethod(c, (method)fftpack_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)fftpack_bang, "bang", 0);

	class_register(CLASS_BOX, c);
	fftpack_class = c;
}


void *fftpack_new(t_symbol *s, long argc, t_atom *argv)
{
	t_fftpack *x = (t_fftpack*)object_alloc(fftpack_class);
	if(!x) return NULL;

	dsp_setup(&x->obj, 2);
	x->outlet_peaks = listout(x);
	x->outlet = listout(x);

	x->write_buffer = 0;
	x->read_buffer = 1;
	x->copy_buffer = 2;

	x->warn_overflow = 0;

	for(size_t i=0u; i<countof(x->buffers); ++i)
	{
		x->buffers[i].size = 0u;
		memset(x->buffers[i].data, 0, sizeof(x->buffers[i].data));
	}

	return x;
}

void fftpack_free(t_fftpack *x)
{
	dsp_free(&x->obj);
}

void fftpack_dsp64(t_fftpack *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	object_method(dsp64, gensym("dsp_add64"), x, fftpack_perform64, 0, NULL);
}

void fftpack_perform64(t_fftpack *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	assert(numins == 2);
	assert(numouts == 0);

	double *in_value = ins[0];
	double *in_index = ins[1];

	t_buffer *write_buffer = &x->buffers[x->write_buffer];
	double *write_data = write_buffer->data;
	size_t write_size = write_buffer->size;

	for(long i=0; i<sampleframes; ++i)
	{
		long index = (long)in_index[i];

		if(index >= countof(write_buffer->data))
		{
			x->warn_overflow = 1;
			continue;
		}

		if(index == 0)
		{
			write_buffer->size = write_size;

			critical_enter(0);
			swap(x->write_buffer, x->read_buffer);
			critical_exit(0);

			write_buffer = &x->buffers[x->write_buffer];
			write_data = write_buffer->data;
			write_size = 0u;
		}

		write_data[index] = in_value[i];

		assert(write_size == index);
		write_size = index + 1u;
	}

	write_buffer->size = write_size;
}

void fftpack_bang(t_fftpack *x)
{
	if(x->warn_overflow)
	{
		post("fftpack~: fft buffer overflow!");
		x->warn_overflow = 0;
	}

	critical_enter(0);
	swap(x->read_buffer, x->copy_buffer);
	critical_exit(0);

	t_buffer *copy_buffer = &x->buffers[x->copy_buffer];

	// peaks
	{
		t_atom result[countof(copy_buffer->data)];
		long result_length = 0;

		for(long i=0; i<copy_buffer->size/2; ++i)
		{
			if(copy_buffer->data[i] > 100.f)
			{
				atom_setlong(&result[result_length++], i);
			}
		}

		outlet_list(x->outlet_peaks, NULL, result_length, result);
	}

	// fft
	{
		t_atom result[countof(copy_buffer->data)];
		long result_length = copy_buffer->size/2; //:HACK: reducing list length by 2 here... this is not really nice but zl does not work very well with large lists...

		atom_setdouble_array(result_length, result, countof(copy_buffer->data), copy_buffer->data);

		outlet_list(x->outlet, NULL, result_length, result);
	}
}
