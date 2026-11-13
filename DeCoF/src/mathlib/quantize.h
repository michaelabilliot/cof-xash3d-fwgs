//
// quantize.h -- tree structured vector quantizer
//

#ifndef QUANTIZE_H
#define QUANTIZE_H

#define MAXDIMS 768
#define MAXQUANT 16000

#include <string.h>

struct Sample;

struct QuantizedValue {
	double MinError;						// minimum possible error. used
											// for neighbor searches.
	struct QuantizedValue *Children[2];		// splits
	int value;								// only exists for leaf nodes
	struct Sample *Samples;					// every sample falling in this
											// subspace
	int NSamples;							// how many samples
	int TotSize;							// sum of the Count fields of all samples
	double *ErrorMeasure;					// variance measure for each dimension
	double TotalError;						// sum of errors
	unsigned char *Mean;					// average value of each dimension
	unsigned char *Mins;					// min value of each dimension
	unsigned char *Maxs;					// max value of each dimension
	int NQuant;								// the number of samples in this node
	int *Sums;								// sum used for avg calcs
	int sortdim;							// dimension currently sorted along.
};

struct Sample {
	int ID;									// identifier of this sample. can
											// be used for any purpose.
	int Count;								// number of samples this sample
											// represents
	int QNum;								// what value this sample ended up being quantized to.
	struct QuantizedValue *qptr;			// ptr to what this was quantized to.
	unsigned char Value[1];					// the values to be quantized
};

inline struct Sample *NthSample(struct Sample *s, int i, int nd)
{
	unsigned char *r=(unsigned char *) s;
	r+=i*(sizeof(*s)+(nd-1));
	return (struct Sample *) r;
}

inline struct Sample *AllocSamples(int ns, int ndims)
{
	size_t size5=(sizeof(struct Sample)+(ndims-1))*ns;
	void *ret=new unsigned char[size5];
	memset(ret,0,size5);
	for(int i=0;i<ns;i++)
		NthSample((struct Sample *) ret,i,ndims)->Count=1;
	return (struct Sample *) ret;
}

// MinimumError: what is the min error which will occur if quantizing
// a sample to the given qnode? This is just the error if the qnode
// is a leaf.
double MinimumError(struct QuantizedValue const *q, unsigned char const *sample,
					int ndims, unsigned char const *weights);
double MaximumError(struct QuantizedValue const *q, unsigned char const *sample,
					int ndims, unsigned char const *weights);

void PrintQTree(struct QuantizedValue const *p,int idlevel=0);
void PrintSamples(struct Sample const *s, int nsamples, int ndims);
void CheckInRange(struct QuantizedValue *q, unsigned char *max, unsigned char *min);

// Quantize: quantize a set of samples into a tree of nvalues leaves
struct QuantizedValue *Quantize(struct Sample *s, int nsamples, int ndims,
								int nvalues, unsigned char *weights, int value0=0);

// CompressSamples: sort and merge equal samples, returning the new count
int CompressSamples(struct Sample *s, int nsamples, int ndims);

// FindMatch: find the leaf of the tree nearest to a sample
struct QuantizedValue *FindMatch(unsigned char const *sample,
								 int ndims,unsigned char *weights,
								 struct QuantizedValue *QTable);

struct QuantizedValue *FindQNode(struct QuantizedValue const *q, int code);

// OptimizeQuantizer: move the leaves to the means accumulated by FindMatch
void OptimizeQuantizer(struct QuantizedValue *q, int ndims);

// RecalculateValues: update the means in a quantization tree, after the samples
// have been modified and re-associated with the nodes in the tree
void RecalculateValues(struct QuantizedValue *q, int ndims);

void FreeQuantization(struct QuantizedValue *t);

// ColorQuantize: quantize a 32-bit image to ncolors palette entries (imagequant.cpp)
#define QUANTFLAGS_NODITHER	1	// don't do floyd-steinberg dither

void ColorQuantize(unsigned char const *pImage,		// 4 byte pixels ARGB
				   int nWidth,
				   int nHeight,
				   int nFlags,						// QUANTFLAGS_xxx
				   int nColors,						// # of colors to fill in in palette
				   unsigned char *pOutPixels,		// where to store resulting 8 bit pixels
				   unsigned char *pOutPalette,		// where to store resulting 768-byte palette
				   int nFirstColor);				// first color to use in mapping

#endif // QUANTIZE_H
